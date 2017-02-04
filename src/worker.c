#define _GNU_SOURCE             /* for sched_setaffinity() */
#include <sched.h>
#include <sys/types.h>          /* for socket/bind/connect() */
#include <sys/socket.h>
#include <arpa/inet.h>          /* for htons()/inet_pton() */
#include <signal.h>             /* for signal() */
#include <linux/tcp.h>          /* for TCP_NODELAY */
#include <time.h>               /* for nanosleep() */
#include "global.h"
#include "ev.h"
#include "worker.h"
#include "statistics.h"


/* 维护连接的信息结构 */
typedef struct st_socket_conn_t {
    struct ev_loop *loop;
    ev_io read_w;
    ev_io write_w;
    
    int wlen;                    /* 发送长度 */
    int rlen;                    /* 读取的报文长度 */
    char buff[MSG_MAX_LEN];      /* 读取的报文缓存 */
}SOCK_CONN;

static struct ev_loop *loop = NULL;            /* libev消息队列 */
static SOCK_CONN *conn = NULL;                 /* 维护TCP连接的指针数字 */
static int conn_num = 0;
static int worker_id;                          /* 进程ID */
static struct sockaddr_in server_ip;           /* 服务器IP */
static char GET_msg[MSG_MAX_LEN] = {0};        /* 待发送的信息 */
static int GET_msg_len = 0;
static uint32_t sip_min = 0;                   /* 源IP绑定起始地址 */
static uint32_t sip_max = 0;
static uint32_t sip = 0;

/* 清理资源 */
static int clear_res(SOCK_CONN *conn);
/* 重新建立连接 */
static int reconn(SOCK_CONN *conn);
/* 读取响应 */
static void recv_cb(EV_P_ ev_io *w, int revents);
/* 发送请求 */
static void send_cb(EV_P_ ev_io *w, int revents);
/* 构建TCP连接，初始化libev队列 */
static int init_conn();



static int clear_res(SOCK_CONN *conn)
{
    /* 清理fd资源 */
    if (conn->read_w.fd) {
        close(conn->read_w.fd);
    }
    if (conn->write_w.fd) {
        close(conn->write_w.fd);
    }

    conn->wlen = 0;
    conn->rlen = 0;
    memset(conn->buff, 0, MSG_MAX_LEN);

    ev_io_stop(conn->loop, &conn->read_w);
    ev_io_stop(conn->loop, &conn->write_w);

    return 0;
}

static int reconn(SOCK_CONN *conn)
{
    struct sockaddr_in client_ip;
    int fd;
    
    fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (-1 == fd) {
        get_stat_info(worker_id)->other_ERR[errno]++;
        return -1;
    }
        
    /* 绑定源IP */
    if (g_opt.bind_sip) {
        client_ip.sin_family = AF_INET;
        client_ip.sin_port = 0;
        client_ip.sin_addr.s_addr = htonl(sip);
        if (++sip > sip_max) {
            sip = sip_min;
        }

        int on=1;
        if((setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on))) < 0 ) {
            get_stat_info(worker_id)->other_ERR[errno]++;
            //return -1;
        }

        if(-1 == bind(fd, (struct sockaddr *)&client_ip, sizeof(client_ip))) {
            get_stat_info(worker_id)->other_ERR[errno]++;
            //return -1;            /* 失败后，由内核选择SIP */
        }
    }

    get_stat_info(worker_id)->conn_try++;
    if (-1 == connect(fd, (const struct sockaddr *)&server_ip,
                      sizeof(struct sockaddr_in))) {
        if (errno != EINPROGRESS) {
            get_stat_info(worker_id)->conn_ERR[errno]++;
            return -1;
        }
    }

    /* 设置非阻塞模式 */
    int flags = 1;
    if (-1 == setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &flags, sizeof(flags))) {
        get_stat_info(worker_id)->other_ERR[errno]++;
        ;/* do nothing */
    }

    /* 加入事件循环 */
    ev_io_init(&conn->read_w, recv_cb, fd, EV_READ);
    ev_io_init(&conn->write_w, send_cb, fd, EV_WRITE);
    ev_io_start(conn->loop, &conn->write_w);
    ev_io_start(conn->loop, &conn->read_w);

    return 0;
}

static void recv_cb(EV_P_ ev_io *w, int revents)
{
    SOCK_CONN *tmp_conn = NULL;
    tmp_conn = (SOCK_CONN *)w->data;

    get_stat_info(worker_id)->read_try++;
    int len = read(w->fd, tmp_conn->buff, MSG_MAX_LEN);
    if (len < 0) {                     /* 出错 */
        get_stat_info(worker_id)->read_ERR[errno]++;
        if (EAGAIN == errno
            || EINTR == errno
            || EWOULDBLOCK == errno) {
            /* do nothing */
        } else {
            clear_res(tmp_conn);
            reconn(tmp_conn);
        }
        return;
    } else if (0 == len) {             /* 服务器关闭了发送通道(收到了FIN) */
        clear_res(tmp_conn);
        reconn(tmp_conn);
        return;
    }

    /* 当报文长度=MSG_MAX_LEN时，可以借助以下ioctl(fd, FIONREAD, &num)
       判断是否需要继续读取报文 */
    tmp_conn->rlen += len;
    if (len < MSG_MAX_LEN) {
        get_stat_info(worker_id)->get_response++;
                    
        /* FIXME: 目前回应的内容比较少，因此只要收到报文就可用当作
           接收完毕；后续需要调整，通过解包，明确判断结束点 */
        tmp_conn->rlen = 0;
        if (g_opt.keepalive) {
            ev_io_start(EV_A_ &tmp_conn->write_w);
        } else {
            clear_res(tmp_conn);
            reconn(tmp_conn);
            return;
        }
    }
}

static void send_cb(EV_P_ ev_io *w, int revents)
{
    SOCK_CONN *tmp_conn = NULL;
    tmp_conn = (SOCK_CONN *)w->data;
    
    get_stat_info(worker_id)->write_try++;
    int len = write(w->fd,
                    GET_msg + tmp_conn->wlen,
                    GET_msg_len - tmp_conn->wlen);
    if (len < 0) {
        get_stat_info(worker_id)->write_ERR[errno]++;
        if (EAGAIN == errno
            || EINTR == errno
            || EWOULDBLOCK == errno) {
            /* do nothing */
        } else {
            clear_res(tmp_conn);
            reconn(tmp_conn);
        }
        return;
    }
    
    tmp_conn->wlen += len;
    if (tmp_conn->wlen == GET_msg_len) {
        get_stat_info(worker_id)->get_send++;
                    
        tmp_conn->wlen = 0;
        ev_io_stop(EV_A_ w);
    }
}

static int init_conn()
{
    SOCK_CONN *tmp_conn = NULL;
    
    /* 建立连接的间隔 */
    struct timespec sleep_interval;  /* 睡眠间隔 */
    sleep_interval.tv_sec = 0;
    if (1 == g_opt.is_concurrent) {
        sleep_interval.tv_nsec = 1000000000/g_opt.client;
        if (sleep_interval.tv_nsec > 999999999) {
            sleep_interval.tv_nsec = 999999999;
        }
    } else {
        sleep_interval.tv_nsec = 100;
    }
    LOG_INFO("%s [%ld]ns", "sleep interval", sleep_interval.tv_nsec);

    /* 构建客户端连接 */
    for (int i=0; i<conn_num; i++) {
        nanosleep(&sleep_interval, NULL);

        tmp_conn = &conn[i];
        tmp_conn->loop = loop;
        tmp_conn->read_w.data = tmp_conn;
        tmp_conn->write_w.data = tmp_conn;
        reconn(tmp_conn);
    }

    return 0;
}


/* 工作进程入口
   @param: int, 工作进程的启动索引号，用于检索共享内存
   @ret: void
   
   @TAKECARE:
   1)此函数不应该返回
*/
static void worker_process()
{
    /* 当对端read插口关闭时，write()会触发SIGPIPE信号； 如果屏蔽掉，
       则会返回错误EPIPE，被错误判断捕捉；如果不屏蔽，则默认导致子
       进程退出 */
    signal(SIGPIPE, SIG_IGN);
    
    /* 初始化服务器端IP */
    server_ip.sin_family = AF_INET;
    server_ip.sin_port = htons(g_opt.dport);
    if(inet_pton(AF_INET, g_opt.dip, &server_ip.sin_addr) <= 0) {
        LOG_ERR_EXIT(strerror(errno));
    }

    /* 初始化待发送的GET请求 */
    snprintf(GET_msg, MSG_MAX_LEN, "%s%s%s\r\n\r\n",
             "GET /test/performance HTTP/1.1\r\n",
             "Host: ",
             g_opt.domain);
    GET_msg_len = strlen(GET_msg);

    /* 构建libev信息结构 */
    loop = EV_DEFAULT;
    conn_num = (g_opt.child==1)? (g_opt.client): ((g_opt.client + g_opt.child)/g_opt.child);
    conn = malloc(sizeof(SOCK_CONN) * conn_num);
    if (NULL == conn) {
        LOG_ERR_EXIT(strerror(errno));
    }
    memset(conn, 0, sizeof(SOCK_CONN) * conn_num);
    LOG_INFO("socket num of worker[%d] is %d", worker_id, conn_num);

    /* 初始化源IP绑定 */
    struct sockaddr_in client_ip;
    if (g_opt.bind_sip
        && 0 != strcmp(g_opt.sip_min, "")
        && 0 != strcmp(g_opt.sip_max, "")) {
        if(inet_pton(AF_INET, g_opt.sip_min, &client_ip.sin_addr) <= 0) {
            LOG_ERR(strerror(errno));
            LOG_INFO("%s", "DISABLE srcIP bind!!!");
                    
            g_opt.bind_sip = 0;    /* TAKECARE: 仅修改本子进程的内存 */
        } else {
            sip_min = ntohl(client_ip.sin_addr.s_addr);
        }

        if(inet_pton(AF_INET, g_opt.sip_max, &client_ip.sin_addr) <= 0) {
            LOG_ERR(strerror(errno));
            LOG_INFO("%s", "DISABLE srcIP bind!!!");
                    
            g_opt.bind_sip = 0;    /* TAKECARE: 仅修改本子进程的内存 */
        } else {
            sip_max = ntohl(client_ip.sin_addr.s_addr);
        }
    } else {
        LOG_INFO("%s", "srcIP bind OFF");
        g_opt.bind_sip = 0;
    }
    sip = sip_min;
    LOG_INFO("after CHANGE, sip_min=%d.%d.%d.%d, sip_max=%d.%d.%d.%d\n",
             (sip_min>>24)&0xff, (sip_min>>16)&0xff,
             (sip_min>>8)&0xff, (sip_min)&0xff,
             (sip_max>>24)&0xff, (sip_max>>16)&0xff,
             (sip_max>>8)&0xff, (sip_max)&0xff);
    

    /* 构建与服务器的TCP连接 */
    init_conn();
    
    /* 开启主循环 */
    ev_run(loop, 0);

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
            worker_id = i + 1;
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

    /* 工作线程主处理入口; 索引ID 0预留给主进程 */
    worker_process();
    LOG_ERR_EXIT("SHOULD NOT HERE!!!");
}




