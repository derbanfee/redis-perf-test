#ifndef RP_RPINTF_REDIS_C_CLUSTER_H_
#define RP_RPINTF_REDIS_C_CLUSTER_H_

#include <hiredis/hiredis.h>
#include "redis_cluster.hpp"


int redis_set(redis::cluster::Cluster &cluster, const std::string &key, const std::string& value);
int redis_get(redis::cluster::Cluster &cluster, const std::string &key, std::string& value);
char *cur_time();
void log_err(int ttls, int err, const char *strerr);

#endif
