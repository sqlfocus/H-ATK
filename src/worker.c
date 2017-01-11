#define _GNU_SOURCE             /* for sched_setaffinity() */
#include <sched.h>
#include <sys/types.h>          /* for socket/bind/connect() */
#include <sys/socket.h>
#include <arpa/inet.h>          /* for htons()/inet_pton() */
#include "global.h"
#include "worker.h"

/* 存储连接结构
   TODO: 目前不关注发送信息的差异性，并且接收信息也不处理；
   因此，暂时利用全局结构代替此处的信息结构
*/
typedef struct st_conn_info_t {
#define STAT_INIT  0
#define STAT_WRITE 1
#define STAT_READ  2
    int state;                 /* 连接状态，INIT/WRITE/READ */
    int fd;                    /* 插口描述符 */

    //char RECV_msg[MSG_MAX_LEN];
    int rlen;                  /* 读取长度 */

    //char GET_msg[MSG_MAX_LEN];
    int wlen;                  /* 发送长度 */
}CONN_INFO;


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


    int conn_num = (g_opt.client + g_opt.child)/g_opt.child;
    CONN_INFO *conn = malloc(sizeof(CONN_INFO) * conn_num);
    if (NULL == conn) {
        LOG_ERR_EXIT(strerror(errno));
    }
    memset(conn, 0, sizeof(CONN_INFO) * conn_num);
    
    /* 主循环 */
    int conn_id = 0;           /* 连接结构索引 */
    for (;;) {
        /* 索引回滚，重新遍历连接结构 */
        if (conn_id >= conn_num) {
            conn_id = 0;
        }
        CONN_INFO *tmp_conn = &conn[conn_id];

        /* 起始状态，创建插口 */
        if (STAT_INIT == tmp_conn->state) {
            /* 关闭各种错误导致的无用插口 */
            if (tmp_conn->fd) {
                close(tmp_conn->fd);
                tmp_conn->fd = 0;
            }
            
            int fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
            if (-1 == fd) {
                LOG_ERR(strerror(errno));
                continue;
            }
            tmp_conn->fd = fd;
        
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
                    continue;
                }

                if(-1 == bind(fd, (struct sockaddr *)&client_ip, sizeof(client_ip))) {
                    LOG_ERR(strerror(errno));
                    //continue;            /* 失败后，由内核选择SIP */
                }
            }
        
            if (-1 == connect(fd, (const struct sockaddr *)&server_ip,
                              sizeof(struct sockaddr_in))) {
                if (errno != EINPROGRESS) {
                    LOG_ERR(strerror(errno));
                    continue;
                }
            }

            tmp_conn->state = STAT_WRITE;
        }

        if (STAT_WRITE == tmp_conn->state) {
            int tmp_loop = 0;
            do {
                int len = write(tmp_conn->fd,
                                GET_msg + tmp_conn->wlen,
                                GET_msg_len - tmp_conn->wlen);
                if (-1 == len) {
                    if (EAGAIN == errno) {
                        continue;
                    } else {
                        LOG_ERR(strerror(errno));
                        goto reconn;
                    }
                }

                tmp_conn->wlen += len;
                if (tmp_conn->wlen == GET_msg_len) {
                    tmp_conn->wlen = 0;
                    tmp_conn->state = STAT_READ;
                    break;
                }
            } while(tmp_loop++ < RW_LOOP_MAX);
        }

        if (STAT_READ == tmp_conn->state) {
            int tmp_loop = 0;
            do {
                int len = read(tmp_conn->fd, RECV_msg, MSG_MAX_LEN);
                if (-1 == len) {
                    if (EAGAIN == errno) {
                        continue;
                    } else {
                        LOG_ERR(strerror(errno));
                        goto reconn;
                    }
                }
                
                tmp_conn->rlen += len;
                if (len <= MSG_MAX_LEN) {
                    LOG_INFO("\n%s\n", RECV_msg);
                    
                    /* FIXME: 目前回应的内容比较少，因此只要收到报文就可用当作
                       接收完毕；后续需要调整，通过解包，明确判断结束点 */
                    if (g_opt.keepalive) {
                        tmp_conn->state = STAT_WRITE;
                    } else {
                    reconn:
                        close(tmp_conn->fd);
                        tmp_conn->fd = 0;
                        tmp_conn->state = STAT_INIT;
                    }
                    tmp_conn->rlen = 0;

                    break;
                }
            } while(tmp_loop++ < RW_LOOP_MAX);
        }
    }

    /* 清理资源 */
    free(conn);
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




