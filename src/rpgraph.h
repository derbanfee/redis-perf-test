#ifndef RP_RPGRAPH_H_
#define RP_RPGRAPH_H_

#include <stdint.h>

int create_rrd_ds();
void update_rrd_file(uint32_t now_sec, uint64_t read, uint64_t read_err, 
                     uint64_t read_lost, uint64_t unmatch,uint64_t read_ttl,  // 1
                     uint64_t write, uint64_t write_new, uint64_t write_err, 
                     uint64_t write_ttl, uint64_t read_t, uint64_t write_t // 2
                    );
void update_rrd_png(uint32_t now_sec, bool force);

#endif
