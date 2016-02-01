#include "rpmain.h"

#ifdef RP_WITH_GRAPH
#include "rpgraph.h"
#endif

#ifdef RP_USE_REDIS3M
#include "rpintf_redis3m.h"
#endif
#ifdef RP_USE_REDIS_C_CLUSTER
#include "rpintf_redis_c_cluster.h"
#endif

#define THREAD_INITIAL_KEY_SEED 0
#define MAX_THREAD_LOAD 10
#define MAX_THREAD_NUM 48
#define MAX_CONF_LINE_LEN 100
#define MIN_GRAPH_DURATION 600
#define MAX_GRAPH_DURATION (12*50400) // if change, should no more than avaliable in database
#define ERR_BUF_LEN 1000

const static char str_conf_notify_file[] = "./rp.notify";
const static char str_conf_file[]        = "./rp.conf";
const static char str_conf_target_graph_duration[] = "graph_duration";
const static char str_conf_target_thread_num[]  = "thread_num";
const static char str_conf_target_thread_load[] = "thread_load";

volatile unsigned int conf_graph_duration = MIN_GRAPH_DURATION; // 10 minutes
volatile int          conf_threads_num    = 4;
volatile int          conf_thread_load    = MAX_THREAD_LOAD;
volatile bool         conf_is_running     = true;

typedef struct {
    uint64_t read;       // set by read thread
    uint64_t read_t;     // set by read thread
    uint64_t read_ttl;   // set by read thread
    uint64_t read_error; // set by read thread
    uint64_t read_lost;  // set by read thread
    uint64_t unmatch;    // set by read thread

    uint64_t write;      // set by write thread
    uint64_t write_new;  // set by write thread
    uint64_t write_t;    // set by write thread
    uint64_t write_ttl;  // set by write thread
    uint64_t write_error;// set by write thread
} stat_item_t;
static const stat_item_t stat_item_initial = {0};

typedef struct {
    int now_sec;
    int now_us;
    int last_us;
} time_period_t;

typedef struct {
    bool         is_running; // set by main thread , read by work thread
    pthread_t    tid_w;      // thread id of thread_write
    pthread_t    tid_r;      // thread id of thread_read
    unsigned int key_seed;   // set by thread_write, read by thread_read, for reproducing keys in its read thread
    unsigned int seed_count; // set by thread_write, read by thread_read, number of rand_r() calls, for reproducing keys in its read thread
    stat_item_t  stat;       // set by work thread , read by main thread
} work_thread_data_t;

typedef std::map<std::string, std::string> CacheType;

redis::cluster::Cluster *cluster_ = NULL;
CacheType          cache;
pthread_spinlock_t c_lock; //cache lock
pthread_spinlock_t err_lock; // lock for log_err()

char *cur_time() {
    static char cur_time_buf[50];
    time_t      now = time(NULL);
    struct tm *result = localtime(&now);
    snprintf(cur_time_buf, sizeof(cur_time_buf), "%04d-%02d-%02d.%02d:%02d:%02d",
             result->tm_year + 1900, result->tm_mon + 1, result->tm_mday,
             result->tm_hour, result->tm_min, result->tm_sec);
    return cur_time_buf;
}

void log_err(int ttls, int err, const char *func, int line, const char *format, ...) {
    static char last_err[ERR_BUF_LEN]= {'\0'};
    static char this_err[ERR_BUF_LEN]= {'\0'};
    static char buf_fmt[ERR_BUF_LEN]= {'\0'};
    static unsigned int  dup_times = 0;
    static char *plast_err = last_err;
    static char *pthis_err = this_err;
    va_list  vargs;
    va_start(vargs, format);
    

    redis::cluster::LockGuard lg(err_lock);

    //snprintf(buf_fmt, ERR_BUF_LEN, "[ERR] %s %s():%d ttls: %d errno: %d :%s\r\n", cur_time(), func, line, ttls, err, format);
    snprintf(buf_fmt, ERR_BUF_LEN, "[ERR] %s ttls: %d errno: %d :%s\r\n", cur_time(), ttls, err, format);
    vsnprintf(pthis_err, ERR_BUF_LEN, buf_fmt, vargs);
    if(strcmp(plast_err, pthis_err) == 0) {
        dup_times++;
    } else {
        if(dup_times > 0) {
            fprintf(stderr, "[ERR] %s last error repeat %d times\r\n", cur_time(), dup_times);
            dup_times = 0;
        }
        fprintf(stderr, pthis_err);
        char *p = plast_err;
        plast_err = pthis_err;
        pthis_err = p;
    }
    va_end(vargs);
}

std::string get_random_key(unsigned int &seed, unsigned int &count) {
    int n = 1000000*(rand_r(&seed)/(RAND_MAX+0.1));
    std::ostringstream ss;
    ss << "key_" << n;
    count++;
    return ss.str();
}

std::string get_random_value(unsigned int &seed) {
    int n = 1000000*(rand_r(&seed)/(RAND_MAX+0.1));
    std::ostringstream ss;
    ss << "value_" << n;
    return ss.str();
}

int check_point(int &now_us, int &now_sec, int &last_us) {
    struct timeval tv;
    gettimeofday( &tv, NULL );
    now_sec = tv.tv_sec;
    now_us = tv.tv_sec*1000000 + tv.tv_usec;

    int ret = now_us - last_us;
    last_us = now_us;
    return ret;
}

void* thread_write(void* para) {
    int now_us = 0;
    int now_sec = 0;
    int last_us = 0;
    unsigned int key_seed;
    unsigned int value_seed;
    unsigned int seed_count;
    volatile work_thread_data_t *pmydata = (work_thread_data_t *)para;
    volatile stat_item_t        *pstat   = &pmydata->stat;

    check_point(now_us, now_sec, last_us);
    value_seed = now_us + 5;
    key_seed   = now_us;
    pmydata->key_seed = key_seed;
    seed_count = pmydata->seed_count;
    while( pmydata->is_running) {

        std::string key = get_random_key(key_seed, seed_count);
        std::string value_read;
        std::string value_write;

        /* check  */
        {
            redis::cluster::LockGuard lockguard(c_lock);
            CacheType::iterator iter = cache.find( key );
            if( iter != cache.end() )  {
                value_write = iter->second;
            }
        }

        if(value_write.length() == 0) {
            int rv = redis_get(*cluster_, key, value_read);
            if(rv == 0) {
                value_write = value_read;
                redis::cluster::LockGuard lockguard(c_lock);
                cache[key] = value_write;
            } else {
                redis::cluster::LockGuard lockguard(c_lock);
                CacheType::iterator iter = cache.find( key );
                if( iter != cache.end() )  {
                    value_write = iter->second;
                } else {
                    value_write =  get_random_value(value_seed);
                    pstat->write_new++;
                    cache[key] = value_write;
                }
            }
        }

        /* set */
        check_point(now_us, now_sec, last_us);
        int rv = redis_set(*cluster_, key, value_write, 3600);
        pstat->write_ttl += cluster_->ttls();
        pstat->write ++;
        if( rv<0 ) {
            pstat->write_error ++;
        }
        pstat->write_t += check_point(now_us, now_sec, last_us);

        pmydata->seed_count = seed_count; // this statement must put here, otherwise read_lost got by read thread will be incorrect

        /* load control */
        while(conf_thread_load == 0 && pmydata->is_running) {
            usleep(50 * 1000);
        }
        usleep( (10 - conf_thread_load) * 0.5 * (pstat->write_t/pstat->write) );

    }

    return NULL;
}

void* thread_read(void* para) {
    int now_us = 0;
    int now_sec = 0;
    int last_us = 0;
    unsigned int key_seed;
    unsigned int seed_count_my = 0;
    unsigned int seed_count_to;
    std::set<std::string> keys;
    volatile work_thread_data_t *pmydata = (work_thread_data_t *)para;
    volatile stat_item_t        *pstat   = &pmydata->stat;

    while(pmydata->key_seed == THREAD_INITIAL_KEY_SEED) {
        sleep(1); //thread write is not ready
    }

    key_seed   = pmydata->key_seed;

    while( pmydata->is_running) {

        /* reproduce keys same as in thread_write */

        seed_count_to = pmydata->seed_count;
        while(seed_count_my < seed_count_to) {
            std::string key = get_random_key(key_seed, seed_count_my);
            keys.insert(key);
        }


        std::set<std::string>::iterator itkey = keys.begin();
        for(; itkey != keys.end(); itkey++) {
            const std::string &key = *itkey;

            /* get value from cache */
            std::string value_tobe;
            {
                redis::cluster::LockGuard lockguard(c_lock);
                CacheType::iterator iter = cache.find( key );
                if( iter != cache.end() )  {
                    value_tobe = iter->second;
                } else {
                    abort(); //not allowed
                }
            }

            /* read redis server */
            std::string value_read;
            check_point(now_us, now_sec, last_us);
            int rv = redis_get(*cluster_, key, value_read);
            pstat->read_ttl += cluster_->ttls();
            pstat->read ++;
            if( rv < 0 ) {
                pstat->read_error ++;
            } else if (rv == 1) {
                pstat->read_lost++;
            } else {
                if( value_read != value_tobe )  {
                    pstat->unmatch ++;
                }
            }
            pstat->read_t += check_point(now_us, now_sec, last_us);

            if(!pmydata->is_running) {
                break;
            }

            /* load control */
            while(conf_thread_load == 0 && pmydata->is_running) {
                usleep(50 * 1000);
            }
            usleep( (10 - conf_thread_load) * 0.5 * (pstat->read_t/pstat->read) );
        }

    }
    return NULL;
}

/* draw graphic output */
#ifdef RP_WITH_GRAPH
void* thread_graph(void* para) {
    struct  timeval tv;
    unsigned int last_rrd_duration = conf_graph_duration;

    while(conf_is_running) {
        gettimeofday( &tv, NULL );
        if(last_rrd_duration != conf_graph_duration) {
            last_rrd_duration = conf_graph_duration;
            update_rrd_png(tv.tv_sec, true);
        } else {
            update_rrd_png(tv.tv_sec, false);
        }
        usleep(200000); // 200 milliseconds
    }
    return NULL;
}
#endif

bool update_config(unsigned int usec, bool force) {
    static unsigned int last_usec = usec;

    if(!force) {
        if(usec - last_usec < 400000) {
            return false;
        }
        last_usec = usec;
        if(access(str_conf_notify_file, F_OK) != 0) {
            return false;
        }
        if(remove(str_conf_notify_file) != 0) {
            RP_LOG_ERR(0, errno, strerror(errno));
        }
    }

    FILE *fp = fopen(str_conf_file, "r");
    if(!fp) {
        RP_LOG_ERR(0, errno,"fail to open file %s: %s", str_conf_file, strerror(errno));
        return false;;
    }

    char buf[MAX_CONF_LINE_LEN];
    char *pk,*pv, *saveptr;
    unsigned int uval;
    while(fgets(buf,sizeof(buf),fp)) {
        pk = strtok_r(buf, " \t", &saveptr);
        if(strcmp(pk, str_conf_target_graph_duration) == 0) {
            pv = strtok_r(NULL, " \t\r\n", &saveptr);
            if(pv) {
                uval = atoi(pv);
                if(uval < MIN_GRAPH_DURATION) {
                    uval = MIN_GRAPH_DURATION;
                }
                if(uval > MAX_GRAPH_DURATION) {
                    uval = MAX_GRAPH_DURATION;
                }
                conf_graph_duration = uval;
                printf("load configuration %s to %u\r\n", pk, uval);
            }
        } else if(strcmp(pk, str_conf_target_thread_num) == 0) {
            pv = strtok_r(NULL, " \t\r\n", &saveptr);
            if(pv) {
                uval = atoi(pv);
                if(uval > MAX_THREAD_NUM) {
                    uval = MAX_THREAD_NUM;
                }
                conf_threads_num = uval;
                printf("load configuration %s to %u\r\n", pk, uval);
            }
        } else if(strcmp(pk, str_conf_target_thread_load) == 0) {
            pv = strtok_r(NULL, " \t\r\n", &saveptr);
            if(pv) {
                uval = atoi(pv);
                if(uval > MAX_THREAD_LOAD) {
                    uval = MAX_THREAD_LOAD;
                }
                conf_thread_load = uval;
                printf("load configuration %s to %u\r\n", pk, uval);
            }
        }
    }
    fclose(fp);
    return true;
}

int main(int argc, char *argv[]) {

    int ret = pthread_spin_init(&err_lock, PTHREAD_PROCESS_PRIVATE);
    if(ret != 0) {
        std::cerr << "pthread_spin_init fail" << std::endl;
        return 1;
    }

    ret = pthread_spin_init(&c_lock, PTHREAD_PROCESS_PRIVATE);
    if(ret != 0) {
        std::cerr << "pthread_spin_init fail" << std::endl;
        return 1;
    }

    struct  timeval tv;

    gettimeofday( &tv, NULL );
    if( !update_config(tv.tv_usec, true)) {
        return 1;
    }

#ifdef RP_WITH_GRAPH
    if(create_rrd_ds() != 0) {
        return 1;
    }

    pthread_t thgraph;
    if(pthread_create(&thgraph, NULL, thread_graph, NULL) != 0) {
        std::cerr << "create graph thread fail" << std::endl;
        return 1;
    }
#endif

    /* init cluster */

    std::string startup = "127.0.0.1:7000,127.0.0.1:7001";
    if(argc > 1)
        startup = argv[1];

    std::cout << "cluster startup with " << startup << " RAND_MAX:"<< RAND_MAX<< std::endl;
    cluster_ = new redis::cluster::Cluster(1);

    if( cluster_->setup(startup.c_str(), true)!=0 ) {
        std::cerr << "cluster setup fail" << std::endl;
        return 1;
    }


    /* control work threads and do statistics */

    std::vector<work_thread_data_t> threads_data;
    std::vector<stat_item_t>        thread_stats_last;
    int                 workers_num_now;
    int                 workers_num_to;
    work_thread_data_t *pdata;
    int                 last_sec = 0;


    uint64_t total_read  = 0;
    uint64_t total_write = 0;

    threads_data.reserve(MAX_THREAD_NUM);

    while(true) {
        workers_num_now = threads_data.size();
        if(!conf_is_running)
            workers_num_to = 0;
        else
            workers_num_to  = conf_threads_num;

        if(workers_num_to > workers_num_now) {
            /* create more work threads */

            threads_data.resize(workers_num_to);
            thread_stats_last.resize(workers_num_to, stat_item_initial);


            for(int idx = workers_num_now; idx < workers_num_to; idx++) {
                pdata = &(threads_data[idx]);
                pdata->key_seed   = THREAD_INITIAL_KEY_SEED;
                pdata->seed_count = 0;
                pdata->is_running = true;
                pdata->stat       = stat_item_initial;
                thread_stats_last[idx] = stat_item_initial;
                if(pthread_create(&pdata->tid_w, NULL, thread_write, pdata) != 0) {
                    std::cerr << "create write thread "<<idx<<" fail" << std::endl;
                    exit(0);
                }
                if(pthread_create(&pdata->tid_r, NULL, thread_read, pdata) != 0) {
                    std::cerr << "create read thread "<<idx<<" fail" << std::endl;
                    exit(0);
                }
            }
        } else if (workers_num_to < workers_num_now) {
            /* end some work threads */

            for(int idx = workers_num_to; idx < workers_num_now; idx++) {
                pdata = &(threads_data[idx]);
                pdata->is_running = false;
                if(pthread_join(pdata->tid_w, NULL) != 0) {
                    std::cerr << "join write thread "<<idx<<" fail" << std::endl;
                }
                if(pthread_join(pdata->tid_r, NULL) != 0) {
                    std::cerr << "join read thread "<<idx<<" fail" << std::endl;
                }
            }
            threads_data.resize(workers_num_to);
            thread_stats_last.resize(workers_num_to);
        }

        if(!conf_is_running)
            break;

        /* statistic */

        gettimeofday( &tv, NULL );
        int now_sec = tv.tv_sec;

        (void)update_config(tv.tv_usec, false);


        if( last_sec != now_sec ) {

            last_sec = now_sec;

            uint64_t read        = 0;
            uint64_t read_t      = 0;
            uint64_t read_ttl    = 0;
            uint64_t read_error  = 0;
            uint64_t read_lost   = 0;
            uint64_t unmatch     = 0;
            uint64_t write       = 0;
            uint64_t write_new   = 0;
            uint64_t write_t     = 0;
            uint64_t write_ttl   = 0;
            uint64_t write_error = 0;

            stat_item_t stat_now;

            for(int idx = 0; idx < workers_num_to; idx++) {
                stat_now = threads_data[idx].stat;

                read        += (stat_now.read        - thread_stats_last[idx].read);
                read_t      += (stat_now.read_t      - thread_stats_last[idx].read_t);
                read_ttl    += (stat_now.read_ttl    - thread_stats_last[idx].read_ttl);
                read_error  += (stat_now.read_error  - thread_stats_last[idx].read_error);
                read_lost   += (stat_now.read_lost   - thread_stats_last[idx].read_lost);
                unmatch     += (stat_now.unmatch     - thread_stats_last[idx].unmatch);
                write       += (stat_now.write       - thread_stats_last[idx].write);
                write_new   += (stat_now.write_new   - thread_stats_last[idx].write_new);
                write_t     += (stat_now.write_t     - thread_stats_last[idx].write_t);
                write_ttl   += (stat_now.write_ttl   - thread_stats_last[idx].write_ttl);
                write_error += (stat_now.write_error - thread_stats_last[idx].write_error);

                thread_stats_last[idx] = stat_now;
            }
            total_read  += read;
            total_write += write;

#ifdef RP_WITH_GRAPH
            update_rrd_file(now_sec, read, read_error,read_lost, unmatch, (read?(read_ttl/read):0), write, write_new, write_error,(write?(write_ttl/write):0),   // 1
                            (read?(read_t/read):0), (write?(write_t/write):0) // 2
                           );
#endif

            static unsigned int ctrl_cnt = 0;
            if(((++ctrl_cnt) & 1) == 0) {
                std::cout << cur_time() <<" "<<conf_threads_num <<" threads " << conf_thread_load << " loads "
                          << total_read << " R("<<read<<" read, " << read_error << " err, "<< read_lost << " lost, " <<  unmatch << " unmatch, "
                          << (read?(read_t/read):0)<<" usec per op, "
                          << (read?(read_ttl/read):0)<<" ttls) | "

                          << total_write << " W(" << write << " write, " << write_error << " err, "<< write_new << " new, "
                          << (write?(write_t/write):0)<<" usec per op, "
                          << (write?(write_ttl/write):0)<<" ttls) " << cluster_->stat_dump() << "\r\n";

                fflush(stdout);
            }

        }
        usleep( 1000*10 );
    }

    return 0;
}

