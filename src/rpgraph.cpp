#ifdef SUPPORT_RRD
#include "rrd.h"
static char rrd_file[] = "./infinite.rrd";

#define RRD_DURATION_1D 86400
#define RRD_DURATION_1H 3600
#define RRD_DURATION_1M 60
#endif

#ifdef SUPPORT_RRD

#define __RC_RRD_PUT_ARG(argv, val, line) \
    char arg##line[] = val;\
    (argv).push_back(arg##line);
#define _RC_RRD_PUT_ARG(argv, val, line) \
    __RC_RRD_PUT_ARG(argv, val, line)
#define RC_RRD_PUT_ARG(argv, val) \
    _RC_RRD_PUT_ARG(argv, val, __LINE__)

#define RC_RRD_PUT_ARGSQ(argv, seq, val) \
    _RC_RRD_PUT_ARG(argv, val, seq)



int create_rrd_ds() {

    if(access(rrd_file, F_OK) == 0) {
        std::cout << "DB file '"<<rrd_file<<"' already exits, reuse it.\r\n";
        return 0;
    }

    char arg_start[50];
    time_t tm = time(NULL);
    std::vector<char *>rrd_argv;

    RC_RRD_PUT_ARG(rrd_argv, "create");
    rrd_argv.push_back(rrd_file);
    snprintf(arg_start, sizeof(arg_start), "-b %u", (unsigned int)tm);
    rrd_argv.push_back(arg_start);
    RC_RRD_PUT_ARG(rrd_argv, "-s1");
    //RC_RRD_PUT_ARG(rrd_argv, "-O"); //some old version rrdtool not support -O or --no-overwrite option

    RC_RRD_PUT_ARG(rrd_argv, "DS:read:GAUGE:1:U:U");
    RC_RRD_PUT_ARG(rrd_argv, "DS:read_err:GAUGE:1:U:U");
    RC_RRD_PUT_ARG(rrd_argv, "DS:read_lost:GAUGE:1:U:U");
    RC_RRD_PUT_ARG(rrd_argv, "DS:unmatch:GAUGE:1:U:U");
    RC_RRD_PUT_ARG(rrd_argv, "DS:read_ttl:GAUGE:1:U:U");

    RC_RRD_PUT_ARG(rrd_argv, "DS:write:GAUGE:1:U:U");
    RC_RRD_PUT_ARG(rrd_argv, "DS:write_new:GAUGE:1:U:U");
    RC_RRD_PUT_ARG(rrd_argv, "DS:write_err:GAUGE:1:U:U");
    RC_RRD_PUT_ARG(rrd_argv, "DS:write_ttl:GAUGE:1:U:U");

    RC_RRD_PUT_ARG(rrd_argv, "DS:read_t:GAUGE:1:U:U");
    RC_RRD_PUT_ARG(rrd_argv, "DS:write_t:GAUGE:1:U:U");

    RC_RRD_PUT_ARG(rrd_argv, "RRA:AVERAGE:0.5:2:3600"); // 2 hours
    RC_RRD_PUT_ARG(rrd_argv, "RRA:AVERAGE:0.5:6:14400"); // 24 hours, 6 * 3600 * 4
    RC_RRD_PUT_ARG(rrd_argv, "RRA:AVERAGE:0.5:12:50400" ); // 7 days, 12 * 3600 * 14, see MAX_GRAPH_DURATION

//  setlocale(LC_ALL, "");
    rrd_clear_error();
    int rc = rrd_create(rrd_argv.size(), rrd_argv.data());
    if(rc != 0) {
        std::cerr << rrd_get_error() << std::endl;
        return -1;
    }

    return 0;
}

void update_rrd_file(uint32_t now_sec, uint64_t read, uint64_t read_err, uint64_t read_lost, uint64_t unmatch,uint64_t read_ttl,  // 1
                     uint64_t write, uint64_t write_new, uint64_t write_err, uint64_t write_ttl, uint64_t read_t, uint64_t write_t // 2
                    ) {
    char rrd_arg[200];
    std::vector<char *>rrd_argv;

    RC_RRD_PUT_ARG(rrd_argv, "update");
    rrd_argv.push_back(rrd_file);
    snprintf(rrd_arg, sizeof(rrd_arg), "%u:" "%lu:%lu:%lu:%lu:%lu:" // 1
             "%lu:%lu:%lu:%lu:%lu:%lu", // 2
             (unsigned int)now_sec, read, read_err,read_lost,unmatch,read_ttl,  // 1
             write, write_new, write_err,write_ttl,read_t, write_t// 2
            );
    rrd_argv.push_back(rrd_arg);

    rrd_clear_error();
    int rc = rrd_update(rrd_argv.size(), rrd_argv.data());
    if(rc != 0) {
        std::cerr << rrd_get_error() << std::endl;
    }

}

void update_rrd_png(uint32_t now_sec, bool force) {

    static char rrd_png_read_2h[]  = "./infinite_read_2h.png";
    static char rrd_png_write_2h[] = "./infinite_write_2h.png";
    static char rrd_png_rw_t_2h[]  = "./infinite_rw_t_2h.png";
    static char rrd_png_rw_ttl_2h[]= "./infinite_rw_ttl_2h.png";

#define RC_RRD_PUT_ARG_START_AND_TITLE(rrd_argv, duration, title, seq) \
    char def_hdr_param_start##seq[100];\
    snprintf(def_hdr_param_start##seq, sizeof(def_hdr_param_start##seq), "-s now-%d", duration);\
    rrd_argv.push_back(def_hdr_param_start##seq);\
    unsigned int left##seq,days##seq,hours##seq,minutes##seq;\
    days##seq  = duration / RRD_DURATION_1D;\
    left##seq  = duration % RRD_DURATION_1D;\
    hours##seq = left##seq / RRD_DURATION_1H;\
    left##seq = left##seq % RRD_DURATION_1H;\
    minutes##seq = left##seq / RRD_DURATION_1M;\
    std::ostringstream ss##seq;\
    if(days##seq > 0) {\
        ss##seq<<days##seq<<" days ";\
    }\
    if(hours##seq > 0) {\
        ss##seq<<hours##seq<<" hours ";\
    }\
    if(minutes##seq > 0) {\
        ss##seq<<minutes##seq<<" minutes";\
    }\
    char def_hdr_param_title##seq[200];\
    snprintf(def_hdr_param_title##seq, sizeof(def_hdr_param_title##seq), "-t %s - %s", title,\
             ss##seq.str().length()>0?ss##seq.str().c_str():"unknown");\
    rrd_argv.push_back(def_hdr_param_title##seq);


#define ADD_ARG_HEADER(file, duration, title) \
    std::vector<char *>rrd_argv; \
    RC_RRD_PUT_ARGSQ(rrd_argv,1, "graph");\
    rrd_argv.push_back(file); \
    RC_RRD_PUT_ARGSQ(rrd_argv,20, "-w 600");\
    RC_RRD_PUT_ARGSQ(rrd_argv,21, "-h 300");\
    RC_RRD_PUT_ARG_START_AND_TITLE(rrd_argv, duration, title, 31);\
    RC_RRD_PUT_ARGSQ(rrd_argv,40, "COMMENT:LAST       MAX       AVG       MIN \\r");

#define UPDATE_RRD \
    char    **calcpr;\
    int       xsize, ysize;\
    double    ymin, ymax;\
    rrd_clear_error();\
    int rc = rrd_graph(rrd_argv.size(), rrd_argv.data(), &calcpr, &xsize, &ysize, NULL, &ymin, &ymax);\
    if(rc == 0) {\
        if (calcpr) {\
            for (int i = 0; calcpr[i]; i++) {\
                printf("%s\r\n", calcpr[i]);\
                free(calcpr[i]);\
            }\
            free(calcpr);\
        }\
    } else {\
        std::cerr << __func__<< ":"<<__LINE__<<":"<<rrd_get_error() << std::endl;\
    }

#define __DEF_LINE(_name,_def,_vdef,_draw_type, _draw_detail, _line)\
    char def##_name##_line[200];\
    snprintf(def##_name##_line, sizeof(def##_name##_line), "DEF:" #_name "=%s:" _def, rrd_file);\
    rrd_argv.push_back(def##_name##_line);\
    RC_RRD_PUT_ARGSQ(rrd_argv,_line##1, "VDEF:" #_name "_last=" #_name ",LAST");\
    RC_RRD_PUT_ARGSQ(rrd_argv,_line##2, "VDEF:" #_name "_max=" #_name ",MAXIMUM");\
    RC_RRD_PUT_ARGSQ(rrd_argv,_line##3, "VDEF:" #_name "_avg=" #_name ",AVERAGE");\
    RC_RRD_PUT_ARGSQ(rrd_argv,_line##4, "VDEF:" #_name "_min=" #_name ",MINIMUM");\
    RC_RRD_PUT_ARGSQ(rrd_argv,_line##5, _draw_type ":" #_name "#" _draw_detail ":" #_name);\
    RC_RRD_PUT_ARGSQ(rrd_argv,_line##6, "GPRINT:" #_name "_last:%7.1lf%s");\
    RC_RRD_PUT_ARGSQ(rrd_argv,_line##7, "GPRINT:" #_name "_max:%7.1lf%s");\
    RC_RRD_PUT_ARGSQ(rrd_argv,_line##8, "GPRINT:" #_name "_avg:%7.1lf%s");\
    RC_RRD_PUT_ARGSQ(rrd_argv,_line##9, "GPRINT:" #_name "_min:%7.1lf%s\\r");
#define _DEF_LINE(_name,_def,_vdef,_draw_type, _draw_detail, _line) __DEF_LINE(_name,_def,_vdef,_draw_type, _draw_detail,_line)
#define DEF_LINE(_name,_def,_vdef,_draw_type, _draw_detail) _DEF_LINE(_name,_def,_vdef,_draw_type, _draw_detail,__LINE__)

    static unsigned int last_update_sec = now_sec;

    if(force || (now_sec - last_update_sec > 2)) {
        last_update_sec = now_sec;
        {
            /* read */
            ADD_ARG_HEADER(rrd_png_read_2h, conf_graph_duration, "READ requests per second");
            DEF_LINE(read,"read:AVERAGE",NULL,"LINE1","00FF00");
            DEF_LINE(read_err,"read_err:AVERAGE",NULL,"LINE1","FF0000");
            DEF_LINE(read_lost,"read_lost:AVERAGE",NULL,"LINE1","0000FF");
            DEF_LINE(unmatch,"unmatch:AVERAGE",NULL,"LINE1","F0F0F0");
            UPDATE_RRD
        }
        {
            /* write */
            ADD_ARG_HEADER(rrd_png_write_2h, conf_graph_duration, "WRITE requests per second");
            DEF_LINE(write,"write:AVERAGE", NULL,  "LINE1", "00FF00");
            DEF_LINE(write_err,"write_err:AVERAGE", NULL,  "LINE1", "FF0000");
            DEF_LINE(write_new,"write_new:AVERAGE", NULL,  "LINE1", "0000FF");
            RC_RRD_PUT_ARG(rrd_argv,"COMMENT:\\r"); // draw a blank line to align size with read graph's
            UPDATE_RRD
        }
        {
            /* read_t and write_t */
            ADD_ARG_HEADER(rrd_png_rw_t_2h, conf_graph_duration, "COST microseconds per request");
            DEF_LINE(read_t,"read_t:AVERAGE", NULL,  "LINE1", "00FF00");
            DEF_LINE(write_t,"write_t:AVERAGE", NULL,  "LINE1", "0000FF");
            UPDATE_RRD
        }
        {
            /* read_ttls and write_ttls */
            ADD_ARG_HEADER(rrd_png_rw_ttl_2h, conf_graph_duration, "COST ttls per request");
            DEF_LINE(read_ttl,"read_ttl:AVERAGE", NULL,  "LINE1", "00FF00");
            DEF_LINE(write_ttl,"write_ttl:AVERAGE", NULL,  "LINE1", "0000FF");
            UPDATE_RRD
        }

    }

#undef ADD_ARG_HEADER
#undef __DEF_LINE
#undef _DEF_LINE
#undef DEF_LINE
#undef UPDATE_RRD
}

#undef __RC_RRD_PUT_ARG
#undef _RC_RRD_PUT_ARG
#undef RC_RRD_PUT_ARG
#undef RC_RRD_PUT_ARGSQ
#endif

