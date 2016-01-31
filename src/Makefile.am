bin_PROGRAMS=

if RP_ENABLE_RP3M
bin_PROGRAMS += rp3m 
rp3m_SOURCES = rpmain.h rpmain.cpp hash_slot.h hash_slot.cpp rpintf_redis3m.h rpintf_redis3m.cpp 
rp3m_CPPFLAGS = -DRP_USE_REDIS3M
rp3m_LDADD = @redis_3m_libs@
if RP_USE_GRAPH
rp3m_SOURCES  += rpgraph.h rpgraph.cpp
rp3m_CPPFLAGS += -I@rrd_header_path@
rp3m_LDADD += @rrd_lib@ 
endif
endif

if RP_ENABLE_RPCLUSTER
bin_PROGRAMS += rpcluster
rpcluster_SOURCES = rpmain.h rpmain.cpp hash_slot.h hash_slot.cpp rpintf_redis_c_cluster.h rpintf_redis_c_cluster.cpp 
rpcluster_CPPFLAGS = -DRP_USE_REDIS_C_CLUSTER
rpcluster_LDADD = @redis_cluster_libs@
if RP_USE_GRAPH
rpcluster_SOURCES  += rpgraph.h rpgraph.cpp
rpcluster_CPPFLAGS += -I@rrd_header_path@
rpcluster_LDADD += @rrd_lib@
endif
endif

