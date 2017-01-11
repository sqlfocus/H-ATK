#ifndef __WORKER_H__
#define __WORKER_H__

/* 创建子进/线程
   @param: int, 子进/线程个数
   @param: int, cpu核心数
   @ret: void
 */
void spawn_child_process(int, int);

#endif
