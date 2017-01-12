#ifndef __STATISTICS_H__
#define __STATISTICS_H__

#include <stdint.h>         /* for uint64_t */


/* 定义统计信息 */
typedef struct st_stat_info_t {
    uint64_t conn_try;         /* connect()尝试次数 */
    uint64_t conn_reconn;      /* 重试连接次数 */
    uint64_t conn_ERR[ERR_NUM_MAX];
    
    uint64_t read_try;         /* 读取次数 */
    uint64_t read_ERR[ERR_NUM_MAX];
    
    uint64_t write_try;        /* 写次数 */
    uint64_t write_ERR[ERR_NUM_MAX];

    uint64_t get_send;         /* 发送的GET数 */
    
    uint64_t get_response;     /* 对应GET的回应数 */
    uint64_t resp_1xx;         /* 应答统计 */
    uint64_t resp_2xx;
    uint64_t resp_3xx;
    uint64_t resp_4xx;
    uint64_t resp_5xx;
                               /* 其他错误统计 */
    uint64_t other_ERR[ERR_NUM_MAX];
}STAT_INFO;


/* 创建共享内存以存放统计信息
   @param: void
   @ret: -1/0, failed/success
*/
int create_shared_mm(void);

/* 实现冷热切换
   @param: void
   @ret: void
*/
void switch_hot_cold(void);

/* 各子进程信息汇总到主进程
   @param: void
   @ret: void
*/
void merge_to_master(void);

/* 输出汇总信息
   @param: void
   @ret: void
*/
void print_stat_res(void);

/* 获取当前统计信息热表
   @param: int, 工作进程索引ID
   @ret: STAT_INFO*, 统计信息结构

   @NOTE:
     1)主进程索引ID为0, 工作进程1~N
*/
STAT_INFO* get_stat_info(int);


#endif
