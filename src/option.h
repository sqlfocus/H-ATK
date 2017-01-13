#ifndef __OPTION_H__
#define __OPTION_H__

/* 定义支持的参数 */
typedef struct st_option_t {
    int client;           /* 并发度，客户数 */
    int child;            /* 进/线程数，不能大于核心数 */
    int keepalive;        /* 是否支持keepalive模式, 0/1 */
    int bind_sip;         /* 是否可选择源IP */
    int duration;         /* 运行时长 */
    int stat_dur;         /* 统计输出周期 */
    char dip[32];         /* 目的IP */
    char domain[256];     /* 域名 */
    char sip_min[32];     /* 源IP */
    char sip_max[32];
}OPTION_T;

/* 定义系统信息 */
typedef struct st_sys_info_t {
    int cpu_num;          /* 系统CPU数 */
    int fd_max;           /* 单进程文件描述符号上限 */
}SYS_INFO;


/* 定义全局变量 */
extern OPTION_T g_opt;
extern SYS_INFO g_sysinfo;


/* 解析命令行参数，及配置文件
   @param: int, 主程序的命令行参数个数
   @param: char**, 主程序的命令行参数
   @ret: 0/-1, 成功/失败
   
   @NOTE:
     1) 后期可用lua实现配置文件解析
 */
int parse_option(int, char**);


/* 获取系统参数，以便更好的掌控程序行为
   @param: void
   @ret: 0/-1, 成功/失败
 */
int get_sysinfo(void);

#endif


