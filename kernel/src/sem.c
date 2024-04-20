#include "klib.h"
#include "sem.h"
#include "proc.h"

void sem_init(sem_t *sem, int value) {
  sem->value = value;
  list_init(&sem->wait_list);
}

void sem_p(sem_t *sem) {
  // Lab2-4: dec sem's value, if value<0, add curr proc to waitlist and block it
  // // TODO();
  sem->value--;
  if(sem->value < 0){
    list_t *l = list_enqueue(&sem->wait_list, (void*)proc_curr());
    assert(l != NULL);
    proc_block();
  }
}

void sem_v(sem_t *sem) {
  // Lab2-4: inc sem's value, if value<=0, dequeue a proc from waitlist and ready it
  // // TODO();
  sem->value++;
  if(sem->value <= 0){
    proc_t *p = (proc_t*)list_dequeue(&sem->wait_list);
    proc_addready(p);
  }
}

#define USER_SEM_NUM 128
static usem_t user_sem[USER_SEM_NUM] __attribute__((used)); // 用户程序使用的信号量

usem_t *usem_alloc(int value) { // 分清ref和value
  // Lab2-5: find a usem whose ref==0, init it, inc ref and return it, return NULL if none
  // // TODO();
  for(int i = 0; i < USER_SEM_NUM; i++){
    if(user_sem[i].ref == 0){
      sem_init(&user_sem[i].sem, value);
      user_sem[i].ref = 1;
      return &user_sem[i];
    }
  }

  return NULL;
}

// 用于复制usem时使用——使用这个信号量的进程多了一个
usem_t *usem_dup(usem_t *usem) {
  // Lab2-5: inc usem's ref
  // // TODO();
  usem->ref++;
  return usem;
}

// 表示当前进程关闭且不再使用该信号量。
void usem_close(usem_t *usem) {
  // Lab2-5: dec usem's ref
  // // TODO();
  usem->ref--;
}


