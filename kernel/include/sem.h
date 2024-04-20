#ifndef __SEM_H__
#define __SEM_H__

#include "klib.h"

typedef struct sem {
  int value; // value为正表示当前资源还有这么多个，为负表示当前没有资源且还有这么多进程在等。
  list_t wait_list;
} sem_t;

void sem_init(sem_t *sem, int value);
void sem_p(sem_t *sem);
void sem_v(sem_t *sem);

typedef struct usem {
  sem_t sem;
  int ref; // 代表当前有几个用户进程持有这个信号量
} usem_t;

usem_t *usem_alloc(int value);
usem_t *usem_dup(usem_t *usem);
void usem_close(usem_t *usem);

#endif
