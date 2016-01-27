#ifndef RP_RPINTF_REDIS_3M_H_
#define RP_RPINTF_REDIS_3M_H_

#include "redis3m/redis3m.hpp"
#include "redis3m/simple_pool.h"

namespace redis {
namespace cluster {

class Cluster {
public:
    typedef struct{
        std::string host;
        std::string port;
        redis3m::simple_pool::ptr_t pool;
#ifdef DEBUG_CONN_POOL_STAT          
        pthread_spin_lock          *lock;
        uint64_t                    get_count;
#endif
    }Node;
    typedef std::vector<Node> NodePool;

    Cluster(unsigned int timeout = 0):timeout_(timeout){}
   ~Cluster();
    int setup(const char *startup, bool lazy);
    int set(const std::string &key, const std::string& value);
    int get(const std::string &key, std::string& value);
    int ttls(){return 1;};  
    std::string stat_dump();

private:
    NodePool node_pool_;
    unsigned int timeout_;
};

class LockGuard {
public:
    explicit LockGuard(pthread_spinlock_t &lock):lock_(lock) {
        pthread_spin_lock(&lock_);
    }
    ~LockGuard() {
        pthread_spin_unlock(&lock_);
    }

    LockGuard(const LockGuard &lock_guard):lock_(lock_guard.lock_) {
        abort();
    }
    LockGuard& operator=(const LockGuard &lock_guard) {
        abort();
    }

private:
    pthread_spinlock_t &lock_;
};

}}

int redis_set(redis::cluster::Cluster &cluster, const std::string &key, const std::string& value);
int redis_get(redis::cluster::Cluster &cluster, const std::string &key, std::string& value);
void log_err(int ttls, int err, const char *strerr);


#endif
