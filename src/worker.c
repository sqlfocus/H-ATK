#define _GNU_SOURCE             /* for sched_setaffinity() */
#include <sched.h>
#include <sys/types.h>          /* for socket/bind/connect() */
#include <sys/socket.h>
#include <arpa/inet.h>          /* for htons()/inet_pton() */
#include "global.h"
#include "worker.h"


/* 工作进程入口
   @param: int, 工作进程PID号
   @ret: void
   
   @TAKECARE:
   1)此函数不应该返回
*/
static void worker_process(int pid)
{
    char RECV_msg[MSG_MAX_LEN] = {0};
    char GET_msg[MSG_MAX_LEN] = {0};     /* 待发送的GET报文 */
    int GET_msg_len = 0;
    struct sockaddr_in server_ip;        /* IP地址 */

    server_ip.sin_family = AF_INET;
    server_ip.sin_port = htons(80);
    if(inet_pton(AF_INET, g_opt.dip, &server_ip.sin_addr) <= 0) {
        LOG_ERR(strerror(errno));
        return;
    }

    
    snprintf(GET_msg, MSG_MAX_LEN, "%s%s%s\r\n\r\n",
             "GET / HTTP/1.1\r\n",
             "Host: ",
             g_opt.dip);
    GET_msg_len = strlen(GET_msg);

    
    
    /* 主循环 */
    int fd = -1;
    int wlen, rlen;
    for (;;) {
        if (-1 == fd) {
            fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
            if (-1 == fd) {
                LOG_ERR(strerror(errno));
                continue;
            }
        
            /* 绑定源IP，目前仅绑定到第一个源IP
               TODO: 实现更多策略的绑定 */
            if (g_opt.bind_sip && g_opt.sip_min
                && 0 != strcmp(g_opt.sip_min, "")) {
                struct sockaddr_in client_ip;

                client_ip.sin_family = AF_INET;
                client_ip.sin_port = 0;
                if(inet_pton(AF_INET, g_opt.sip_min, &client_ip.sin_addr) <= 0) {
                    LOG_ERR(strerror(errno));
                    LOG_INFO("%s", "DISABLE srcIP bind!!!");
                    g_opt.bind_sip = 0;    /* TAKECARE: 仅修改本子进程的内存 */
                    goto reconn;
                }

                if(-1 == bind(fd, (struct sockaddr *)&client_ip, sizeof(client_ip))) {
                    LOG_ERR(strerror(errno));
                    goto reconn;
                }
            }
        
            if (-1 == connect(fd, (const struct sockaddr *)&server_ip,
                              sizeof(struct sockaddr_in))) {
                if (errno != EINPROGRESS) {
                    LOG_ERR(strerror(errno));
                    goto reconn;
                }
            }
        }

        wlen = 0;
        do {
            int len = write(fd, GET_msg, GET_msg_len);
            if (-1 == len) {
                if (EAGAIN == errno) {
                    continue;
                } else {
                    LOG_ERR(strerror(errno));
                    goto reconn;
                }
            } else {
                wlen += len;
                if (wlen == GET_msg_len) {
                    break;
                }
            }
        } while(1);

        rlen = 0;
        do {
            int len = read(fd, RECV_msg, MSG_MAX_LEN);
            if (-1 == len) {
                if (EAGAIN == errno) {
                    continue;
                } else {
                    LOG_ERR(strerror(errno));
                    goto reconn;
                }
            } else {
                rlen += len;
                if (len <= MSG_MAX_LEN) {
                    LOG_INFO("\n%s\n", RECV_msg);
                    break;
                }
            }
        } while(1);

        if (0 == g_opt.keepalive) {
        reconn:
            close(fd);
            fd = -1;
        }
    }
}



void spawn_child_process(int worker_num, int cpu_num)
{
    pid_t pid;
    for (int i=0; i<worker_num; i++){
        pid = fork();
        switch (pid) {
        case -1:
            LOG_ERR(strerror(errno));
            break;
        case 0:            /* 子进程 */
            break;
        default:           /* 父进程 */
            break;
        }

        if (pid == 0) {    /* 子进程退出循环 */
            break;
        }
    }

    /* 父进程返回，不做核绑定 */
    if (pid) {
        return;
    }

    /* 子进程核绑定 */
    cpu_set_t cpuset;
    pid = getpid();
    CPU_ZERO(&cpuset);
    CPU_SET(pid % cpu_num, &cpuset);
    if (-1 == sched_setaffinity(pid, sizeof(cpu_set_t), &cpuset)) {
        LOG_ERR(strerror(errno));
    }
    LOG_INFO("child[%d] bind to CPU[%d]", pid, pid % cpu_num);

    /* 工作线程主处理入口 */
    worker_process(pid);
    LOG_ERR_EXIT("SHOULD NOT HERE!!!");
}




