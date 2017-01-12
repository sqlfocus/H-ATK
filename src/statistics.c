/* 统计数据都放置在共享内存中，但每个进程拥有自己独立的统计内存；一定
   时间后，父管理进程把结果统计汇总到自己的内存，然后展示到界面。

   为了避免数据不准确，采用冷热备实现切面编程。
*/
#include <sys/mman.h>               /* for mmap() */
#include "global.h"
#include "statistics.h"

static int *hot_cold_flag = NULL;           /* 冷热备切换标识 */
static STAT_INFO* stat_info_mm = NULL;   /* 统计共享内存 */

/* 获取冷表 */
static STAT_INFO* get_cold_stat_info(int worker_id)
{
    return &stat_info_mm[worker_id * 2 + (!(*hot_cold_flag))];
}

STAT_INFO* get_stat_info(int worker_id)
{
    return &stat_info_mm[worker_id * 2 + *hot_cold_flag];
}

void switch_hot_cold()
{
    *hot_cold_flag = !(*hot_cold_flag);
}

void merge_to_master(void)
{
    int num = sizeof(STAT_INFO)/sizeof(uint64_t);
    uint64_t* master_curr = (uint64_t *)&stat_info_mm[1];
    uint64_t* master_total = (uint64_t *)&stat_info_mm[0];

    /* 清空当前汇总项 */
    memset(master_curr, 0, sizeof(STAT_INFO));

    /* 汇总 */
    for (int i=0; i<g_opt.child; i++) {
        /* magic 1: 0预留给了主进程，其他工作进程索引从1开始 */
        uint64_t* worker = (uint64_t *)get_cold_stat_info(i+1);

        for (int j=0; j<num; j++) {
            master_curr[j] += worker[j];
            master_total[j] += worker[j];
        }
        
        /* 清理冷表 */
        memset(worker, 0, sizeof(STAT_INFO));
    }
}

void print_stat_res(void)
{
    for (int k=1; k>=0; k--) {
        STAT_INFO* master = &stat_info_mm[k];

        if (0 == k) {
            printf("\n\nPRINT TOTAL INFO\n");
        } else if (1 == k) {
            printf("\n\n\nPRINT LAST DURATION INFO\n");
        } else {
            /*do nothing*/;
        }
        
        printf("CONNECTION INFO: conn_try %lu, conn_reconn %lu, \n",
               master->conn_try, master->conn_reconn);
        for (int i=0; i<ERR_NUM_MAX; i++) {
            if (master->conn_ERR[i]) {
                printf("\t%s: %lu\n", strerror(i), master->conn_ERR[i]);
            }
        }

        printf("READ INFO: read_try %lu, \n", master->read_try);
        for (int i=0; i<ERR_NUM_MAX; i++) {
            if (master->read_ERR[i]) {
                printf("\t%s: %lu\n", strerror(i), master->read_ERR[i]);
            }
        }

        printf("WRITE INFO: write_try %lu, \n", master->write_try);
        for (int i=0; i<ERR_NUM_MAX; i++) {
            if (master->write_ERR[i]) {
                printf("\t%s: %lu\n", strerror(i), master->write_ERR[i]);
            }
        }

        printf("GET INFO: send %lu, \n", master->get_send);
           
        printf( "RESP INFO: get_response %lu, !!!RPS!!! %lu\n"
                "\tresp_1xx %lu, \n"
                "\tresp_2xx %lu, \n"
                "\tresp_3xx %lu, \n"
                "\tresp_4xx %lu, \n"
                "\tresp_5xx %lu, \n",
                master->get_response, (k==0 ? 0:master->get_response/g_opt.stat_dur),
                master->resp_1xx,
                master->resp_2xx,
                master->resp_3xx,
                master->resp_4xx,
                master->resp_5xx);

        printf("OTHER ERR:\n");
        for (int i=0; i<ERR_NUM_MAX; i++) {
            if (master->other_ERR[i]) {
                printf("\t%s: %lu\n", strerror(i), master->other_ERR[i]);
            }
        }
    }
}

int create_shared_mm(void)
{
    /* 起始位置，int，存放hot_cold_flag; 后续就是各个进程的统计信息结构，
       前两个预留给主进程，用于统计信息汇总 */
    /* magic 1: 主进程； magic 2: 冷热备份 */
    int size = sizeof(STAT_INFO) * (g_opt.child + 1) * 2 + sizeof(int);
    stat_info_mm = mmap(NULL, size, PROT_READ|PROT_WRITE,
                        MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (MAP_FAILED == stat_info_mm) {
        LOG_ERR(strerror(errno));
        return -1;
    }

    memset(stat_info_mm, 0, size);
    hot_cold_flag = (int *)stat_info_mm;
    stat_info_mm = (STAT_INFO *)((char *)stat_info_mm + sizeof(int));
    
    return 0;
}



