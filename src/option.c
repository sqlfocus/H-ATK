#include <sys/sysinfo.h>          /* for get_nprocs() */
#include <sys/time.h>             /* for getrlimit() */
#include <sys/resource.h>
#include "global.h"
#include "option.h"

OPTION_T g_opt = {
    .client = 1,
    .child = 1,
    .keepalive = 0,
    .bind_sip = 0,
    .duration = 30,
    .stat_dur = 3,
    .dip = "127.0.0.1",
    .domain = "localhost",
    .sip_min = "",
    .sip_max = "",
};

SYS_INFO g_sysinfo = {
    .cpu_num = 0,
    .fd_max = 0,
};


int parse_option(int argc, char** argv)
{
    /* 打印系统信息 */
    printf("client=%d, "
           "child=%d, \n"
           "keepalive=%d, "
           "bind_sip=%d, \n"
           "duration=%d, "
           "stat_dur=%d, \n"
           "dip=%s, "
           "domain=%s, "
           "sip_min=%s, "
           "sip_max=%s, "
           "\n\n",
           g_opt.client,
           g_opt.child,
           g_opt.keepalive,
           g_opt.bind_sip,
           g_opt.duration,
           g_opt.stat_dur,
           g_opt.dip,
           g_opt.domain,
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




