#include "rpintf_redis_c_cluster.h"
#include "rpmain.h"

int redis_set(redis::cluster::Cluster &cluster, const std::string &key, const std::string& value, unsigned int expired_time) {
    int ret = 0;
    char buf_expire[25];
    snprintf(buf_expire, sizeof(buf_expire),"%u", expired_time);
    std::vector<std::string> commands;
    commands.push_back("SET");
    commands.push_back(key);
    commands.push_back(value);
    commands.push_back("EX");
    commands.push_back(buf_expire);
    redisReply *reply = cluster.run(commands);
    if( !reply ) {
        ret = -1;
        RP_LOG_ERR(cluster.ttls(), cluster.err(), cluster.strerr().c_str());
    } else if( reply->type==REDIS_REPLY_STATUS && !strcmp(reply->str, "OK") ) {
        ret = 0;
    } else if( reply->type==REDIS_REPLY_ERROR ) {
        //std::cout << "redis_set error " << reply->str << std::endl;
        ret = -1;
        RP_LOG_ERR(cluster.ttls(), 100, reply->str);
    } else {
        ret = -1;
        RP_LOG_ERR(cluster.ttls(), 10000 + reply->type, "unknown redis server error");
    }

    if( reply )
        freeReplyObject( reply );

    return ret;
}

int redis_get(redis::cluster::Cluster &cluster, const std::string &key, std::string& value) {
    int ret = 0;
    std::vector<std::string> commands;
    commands.push_back("GET");
    commands.push_back(key);
    redisReply *reply = cluster.run(commands);
    if( !reply ) {
        ret = -1;
        RP_LOG_ERR(cluster.ttls(), cluster.err(), cluster.strerr().c_str());
    } else if( reply->type==REDIS_REPLY_NIL ) {
        ret = 1; //not found
    } else if( reply->type==REDIS_REPLY_STRING ) {
        value = reply->str;
        ret = 0;
    } else if( reply->type==REDIS_REPLY_ERROR ) {
        //std::cout << "redis_get error " << reply->str << std::endl;
        ret = -1;
        RP_LOG_ERR(cluster.ttls(), 200, reply->str);
    } else {
        ret = -1;
        RP_LOG_ERR(cluster.ttls(), 20000 + reply->type, "unknown redis server error");
    }

    if( reply )
        freeReplyObject( reply );

    return ret;
}
