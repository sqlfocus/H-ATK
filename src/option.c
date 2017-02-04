#include <sys/sysinfo.h>          /* for get_nprocs() */
#include <sys/time.h>             /* for getrlimit() */
#include <sys/resource.h>
#include "global.h"
#include "option.h"
#include "cJSON.h"


/* 定义全局变量 */
OPTION_T g_opt = {
    .client = 1,
    .child = 1,
    .keepalive = 0,
    .bind_sip = 0,
    .duration = 30,
    .stat_dur = 3,
    .dport = 80,
    .dip = "127.0.0.1",
    .domain = "localhost",
    .sip_min = "",
    .sip_max = "",
};

SYS_INFO g_sysinfo = {
    .cpu_num = 0,
    .fd_max = 0,
};


static int parse_conf(const char *conf)
{
    cJSON *cj_res;
    cJSON *obj;

    cj_res = cJSON_Parse(conf);
    if (NULL == cj_res) {
        LOG_ERR("conf parse failed!!!");
        return -1;
    }

#define SET_VAL_INT(name, off) do {                         \
        obj = cJSON_GetObjectItem(cj_res, name);            \
        if (NULL == obj) {                                  \
            break;                                          \
        }                                                   \
        if (cJSON_Number == obj->type) {                    \
            g_opt.off = obj->valueint;                      \
        } else {                                            \
            LOG_ERR("MAYBE some conf param type error!!!"); \
        }                                                   \
    }while(0);
    
#define SET_VAL_STR(name, off) do {                                     \
        obj = cJSON_GetObjectItem(cj_res, name);                        \
        if (NULL == obj) {                                              \
            break;                                                      \
        }                                                               \
        if (cJSON_String == obj->type) {                                \
            snprintf(g_opt.off, sizeof(g_opt.off), "%s", obj->valuestring); \
        } else {                                                        \
            LOG_ERR("MAYBE some conf param type error!!!");             \
        }                                                               \
    }while(0);

    SET_VAL_INT("client", client);
    SET_VAL_INT("child", child);
    SET_VAL_INT("keepalive", keepalive);
    SET_VAL_INT("bind_sip", bind_sip);
    SET_VAL_INT("duration", duration);
    SET_VAL_INT("stat_dur", stat_dur);
    SET_VAL_INT("dport", dport);
    SET_VAL_STR("dip", dip);
    SET_VAL_STR("domain", domain);
    SET_VAL_STR("sip_min", sip_min);
    SET_VAL_STR("sip_max", sip_max);

    cJSON_Delete(cj_res);
    return 0;
}

int parse_option(int argc, char** argv)
{
    char *conf_file = NULL;
    int fd;
    char conf[MSG_MAX_LEN] = {0};
    
    /* 解析配置文件 */
    if (argc > 1) {
        conf_file = argv[1];
    }
    if (NULL != conf_file) {
        fd = open(conf_file, O_RDONLY);
    } else {
        LOG_ERR("NO conf file!!!");
        return -1;
    }
    if (-1 == fd) {
        LOG_ERR("open conf file failed!!!");
        LOG_ERR(strerror(errno));
        return -1;
    }
    if (-1 == read(fd, conf, MSG_MAX_LEN)) {
        LOG_ERR(strerror(errno));
        return -1;
    } else {
        if (0 != parse_conf(conf)) {
            return -1;
        }
    }
    close(fd);

    /* 验证参数合理性 */
    if (g_opt.client > 10000000                 /* 最大千万级别 */
        || g_opt.client > g_sysinfo.fd_max) {
        LOG_ERR("too many clients!!!");
        return -1;
    }
    if ( (0==strcmp(g_opt.sip_min, "") && 1==g_opt.bind_sip)
         || (0 != strcmp(g_opt.sip_min, "") && 0==g_opt.bind_sip) ) {
        LOG_ERR("bind SRC option conflict!!!");
        return -1;
    }
    
    /* 打印系统信息 */
    printf("client=%d, "
           "child=%d, \n"
           "keepalive=%d, \n"
           "duration=%d, "
           "stat_dur=%d, \n"
           "dport=%d, "
           "dip=%s, "
           "domain=%s, \n"
           "bind_sip=%d, "
           "sip_min=%s, "
           "sip_max=%s, "
           "\n\n",
           g_opt.client,
           g_opt.child,
           g_opt.keepalive,
           g_opt.duration,
           g_opt.stat_dur,
           g_opt.dport,
           g_opt.dip,
           g_opt.domain,
           g_opt.bind_sip,
           g_opt.sip_min,
           g_opt.sip_max);
    return 0;
}

int get_sysinfo(void)
{
    /* 获取可用CPU核数 */
    g_sysinfo.cpu_num = get_nprocs();

    /* 获取单进程可用fd上限数 */
    struct rlimit  rlmt;
    if (-1 == getrlimit(RLIMIT_NOFILE, &rlmt)) {
        LOG_ERR("RLIMIT_NOFILE failed");
    } else {
        g_sysinfo.fd_max = rlmt.rlim_cur;
    }

    /* 打印系统信息 */
    printf("cpu_num=%d, "
           "fd_max=%d, "
           "\n\n",
           g_sysinfo.cpu_num,
           g_sysinfo.fd_max);
    
    return 0;
}




