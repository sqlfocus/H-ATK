/**
   本程序模拟HTTP flood行为，试图糅合ab、wrk等测试工具的优点，并解决
   其不能选择源IP的弊端，以期能够更高效率用于压力测试等场景。

   本程序主要适用三种场景，1)测试防护设备的新建能力；2)测试防护设备的
   并发能力；3)测试防护设备的吞吐。

   无论哪种场景，都要求打出更多的数据报文；本程序首先尝试非EPOLL模型，
   类似于轮询机制，循环扫瞄连接结构，根据相应的状态发送相应的数据包；
   如果验证结果无法达到预期，则改用EPOLL模型，以期加深对它的理解。

   并发场景下，连接结构非常多，循环扫瞄时间可能过长，不过没关系；并发
   场景不需要高吞吐，正好利用循环耗时模拟用户的访问延迟。
*/
#include "global.h"
#include "option.h"
#include "statistics.h"
#include "worker.h"

int main(int argc, char **argv)
{
    /* 解析参数 */
    if (-1 == parse_option(argc, argv)) {
        LOG_ERR_EXIT("parse option failed");
    }

    /* 获取系统信息 */
    if (-1 == get_sysinfo()) {
        LOG_ERR_EXIT("get sysinfo failed");
    }

    /* 创建共享内存以存储统计信息 */
    if (-1 == create_shared_mm()) {
        LOG_ERR_EXIT("shared memory created failed");
    }
    
    /* 创建子进程 */
    spawn_child_process(g_opt.child, g_sysinfo.cpu_num);
    
    /* 主处理循环 */
    for(;;) {
        sleep(g_opt.stat_dur);
        switch_hot_cold();
        merge_to_master();
        print_stat_res();
    }
    
    return 0;
}
