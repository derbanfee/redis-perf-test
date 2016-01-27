#include <stdio.h>
#include "rpintf_redis3m.h"
#include "hash_slot.h"


namespace redis {
namespace cluster {

Cluster::~Cluster() {
    for(size_t idx = 0; idx < node_pool_.size(); idx++) {
#ifdef DEBUG_CONN_POOL_STAT
        delete node_pool_[idx].lock;
#endif
        ;
    }
    node_pool_.clear();
}

int Cluster::setup(const char *startup, bool lazy) {
    char *buf = (char *)malloc(strlen(startup) + 1);
    strcpy(buf, startup);

    char *srv,*port,*saveptr;

    srv = strtok_r(buf, ",;", &saveptr);
    while(srv) {
        port = strchr(srv, ':');
        if(port &&  *(port + 1) != '\0') {
            *port = '\0';
            Cluster::Node node;
            node.host = srv;
            node.port = port+1;

#ifdef DEBUG_CONN_POOL_STAT
            pthread_spin_lock *lock = new pthread_spin_lock;
            if(pthread_spin_init(lock, PTHREAD_PROCESS_PRIVATE) != 0) {
                fprintf(stderr, "fail to init spin_lock for node %s:%s\r\n", node.host.c_str(), node.port.c_str());
                free(buf);
                return -1;
            }
            node.lock = lock;
            node.get_count = 0;
#endif
            node.pool = redis3m::simple_pool::create(node.host, atoi(node.port.c_str()), timeout_);
            if(!node.pool) {
                fprintf(stderr, "fail to connect to server:%s:%s\r\n", node.host.c_str(), node.port.c_str());
                free(buf);
#ifdef DEBUG_CONN_POOL_STAT                
                delete lock;
#endif
                return -1;
            }
            node_pool_.push_back(node);
        } else {
            free(buf);
            fprintf(stderr, "invalid parameter server:[%s]\r\n", srv);
            return -1;
        }
        srv = strtok_r(NULL, ",;", &saveptr);
    }

    free(buf);
    if(node_pool_.size() == 0) {
        fprintf(stderr, "no avaliable server\r\n");
        return -1;
    }
    return 0;
}

int Cluster::set(const std::string &key, const std::string& value) {
    Node     &node = node_pool_[get_node_index(key, node_pool_.size())];
    assert(node.pool);
    try {
#ifdef DEBUG_CONN_POOL_STAT
        {
            LockGuard lock(node.lock);
            node.get_count++;
        }
#endif
        redis3m::connection::ptr_t conn = node.pool->get();
        if(!conn) {
            log_err(0, -1, "no avaliable connection");
            return -1;
        }
        redis3m::reply rp = conn->run(redis3m::command("SET")(value)(value)("EX")(timeout_));
        node.pool->put(conn);

        if (rp.type() == redis3m::reply::STATUS && rp.str() == "OK") {
            return 0;
        } else {
            log_err(0, 10000+rp.type(), "redis3m set fail");
            return -1;
        }
    } catch(...) {
        log_err(0, 100, "redis3m exception");
        return -1;
    }

}

int Cluster::get(const std::string &key, std::string& value) {
    Node     &node = node_pool_[get_node_index(key, node_pool_.size())];
    assert(node.pool);

    try {
#ifdef DEBUG_CONN_POOL_STAT
        {
            LockGuard lock(node.lock);
            node.get_count++;
        }
#endif
        redis3m::connection::ptr_t conn = node.pool->get();
        if(!conn) {
            log_err(0, -1, "no avaliable connection");
            return -1;
        }

        redis3m::reply rp = conn->run(redis3m::command("GET")(key));
        node.pool->put(conn);

        if (rp.type() == redis3m::reply::STRING) {
            value = rp.str();
            return 0;

        } else if(rp.type() == redis3m::reply::ERROR) {
            log_err(0, -1, "redis3m error");
            return -1;

        } else if(rp.type() == redis3m::reply::NIL) {
            return 1; // not found

        } else {
            log_err(0, 20000+rp.type(), "unkonwn redis3m::replay type");
            return -1;
        }
    } catch(...) {
        log_err(0, 200, "redis3m exception");
        return -1;
    }
}

std::string Cluster::stat_dump() {
    std::ostringstream ss;
    ss<<"Cluster have "<<node_pool_.size() <<" nodes: ";

    for(size_t idx = 0; idx < node_pool_.size(); idx++) {
#ifdef DEBUG_CONN_POOL_STAT
        LockGuard lock(node_pool_[idx].lock);
#endif
        ss<<"\r\nNode{"<< node_pool_[idx].host << ":" << node_pool_[idx].port
#ifdef DEBUG_CONN_POOL_STAT
          <<" conn_get: "<< node_pool_[idx].get_count;
#endif
                <<"}";
    }
    ss << "\r\n";
    return ss.str();
}
}
}

int redis_set(redis::cluster::Cluster &cluster, const std::string &key, const std::string& value)
{
    return cluster.set(key, value);
}
int redis_get(redis::cluster::Cluster &cluster, const std::string &key, std::string& value)
{
    return cluster.get(key, value);

}


