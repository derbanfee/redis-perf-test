#ifndef RP_MAIN_H_
#define RP_MAIN_H_

#include "config.h"
#include <errno.h>
#include <stdarg.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <iostream>
#include <map>
#include <sstream>
#include <vector>


extern volatile unsigned int conf_graph_duration;

void log_err(int ttls, int err, const char *func, int line, const char *format, ...);
#define RP_LOG_ERR(ttls, err, format...) log_err(ttls, err, __func__, __LINE__, ##format)




#endif
