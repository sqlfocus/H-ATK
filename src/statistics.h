#ifndef __STATISTICS_H__
#define __STATISTICS_H__

/* 定义统计信息 */
typedef struct st_stat_info_t {
    int conn_try;         /* connect()尝试次数 */
    int conn_active;      /* 当前的连接数 */
    
    int read_try;         /* 读取次数 */
    int write_try;        /* 写次数 */
    
    int get_send;         /* 发送的GET数 */
    int get_response;     /* 对应GET的回应数 */
    
    int res_1xx;          /* 错误信息统计 */
    int res_2xx;
    int res_3xx;
    int res_4xx;
    int res_5xx;
}STAT_INFO;

#endif
