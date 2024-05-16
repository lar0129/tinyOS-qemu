#ifndef __PROC_H__
#define __PROC_H__

#include "klib.h"
#include "vme.h"
#include "cte.h"
#include "sem.h"
#include "file.h"

#define KSTACK_SIZE 4096

typedef union {
  uint8_t stack[KSTACK_SIZE];
  struct {
    uint8_t pad[KSTACK_SIZE - sizeof(Context)];
    Context ctx; //内核栈的栈顶有个中断上下文ctx，是因为用户程序中断后一定会在内核栈顶构造中断上下文
  };
} kstack_t;

#define STACK_TOP(kstack) (&((kstack)->stack[KSTACK_SIZE]))
#define MAX_USEM 32
#define MAX_UFILE 32

typedef struct proc {
  int pid;
  enum {UNUSED, UNINIT, RUNNING, READY, ZOMBIE, BLOCKED} status;
  //UNUSED代表这个PCB是空的，不表示进程；UNINIT代表虽然这个PCB虽然表示一个进程，但这个还没初始化，因此不能执行这个进程；
  // RUNNING代表这个PCB表示的进程正在执行；因为只有一个CPU，所以我们这个操作系统中同时只能有一个RUNNING的进程。READY代表这个PCB表示的进程虽然现在不在执行，但可以被执行，
  PD *pgdir; //维护这个进程虚拟地址空间的页目录
  size_t brk;
  kstack_t *kstack; //进程内核栈的栈底
  Context *ctx; // points to restore context for READY proc 
  // 如果这个PCB表示的进程现在不在RUNNING，但未来将被RUNNING的话，那么ctx指向一个能让这个进程开始执行的中断上下文

  struct proc *parent; // Lab2-2
  // 不是所有进程都有父进程，比如内核进程和由内核进程直接创建的用户进程就没有父进程，所以今后如果需要使用parent的时候记得判NULL。
  int child_num; // Lab2-2
  int exit_code; // Lab2-3
  sem_t zombie_sem; // Lab2-4 管理"僵尸进程资源"
  usem_t *usems[MAX_USEM]; // Lab2-5 //一个进程至多持有MAX_USEM（32）个用户信号量，为NULL表示这个编号没有对应的信号量。
  file_t *files[MAX_UFILE]; // Lab3-1 //为NULL表示这个文件描述符没有对应的文件
  inode_t *cwd; // Lab3-2 表示这个进程的 “当前目录”，这样进程访问文件就是从“当前目录” 开始
} proc_t;

void init_proc();
proc_t *proc_alloc();
void proc_free(proc_t *proc);
proc_t *proc_curr();
void proc_run(proc_t *proc) __attribute__((noreturn));
void proc_addready(proc_t *proc);
void proc_yield();
void proc_copycurr(proc_t *proc);
void proc_makezombie(proc_t *proc, int exitcode);
proc_t *proc_findzombie(proc_t *proc);
void proc_block();
int proc_allocusem(proc_t *proc);
usem_t *proc_getusem(proc_t *proc, int sem_id);
int proc_allocfile(proc_t *proc);
file_t *proc_getfile(proc_t *proc, int fd);

void schedule(Context *ctx);

#endif
