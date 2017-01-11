#ifndef __GLOBAL_H__
#define __GLOBAL_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include "option.h"

#define MSG_MAX_LEN  8192         /* 收发报文缓存的最大长度 */

#define LOG_INFO(fmt, ...) do {                                         \
        printf("[%s|%d] "fmt"\n", __FILE__, __LINE__, ##__VA_ARGS__);   \
    } while(0)
#define LOG_ERR(msg) do {                                   \
        printf("[%s|%d] %s\n", __FILE__, __LINE__, msg);    \
    } while(0)
#define LOG_ERR_EXIT(msg) do {                              \
        printf("[%s|%d] %s\n", __FILE__, __LINE__, msg);    \
        exit(EXIT_FAILURE);                                 \
    } while(0)

          
#endif
