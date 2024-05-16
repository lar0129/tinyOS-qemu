#include "klib.h"
#include "cte.h"
#include "proc.h"

#define PROC_NUM 64

static __attribute__((used)) int next_pid = 1;

proc_t pcb[PROC_NUM]; //操作系统所有可用的PCB
static proc_t *curr = &pcb[0];

void init_proc() {
  // Lab2-1, set status and pgdir
  pcb[0].status = RUNNING;
  pcb[0].pgdir = vm_curr();
  // 本身就在内核，不会遇到用户态到内核态的中断，所以用不到kstack这个“内核栈“，
  // 也可以设置成现在栈所在的那一页（(void*)(KER_MEM-PGSIZE)）
  pcb[0].kstack = (kstack_t*)((void*)(KER_MEM-PGSIZE));
  // Lab2-4, init zombie_sem
  sem_init(&pcb[0].zombie_sem, 0);
  // Lab3-2, set cwd
  pcb[0].cwd = iopen("/", TYPE_NONE);
}

proc_t *proc_alloc() {
  // Lab2-1: find a unused pcb from pcb[1..PROC_NUM-1], return NULL if no such one
  // // TODO();
  // 从pcb[1]开始遍历，如果没有空闲的PCB，直接返回NULL。
  for (int i = 1; i < PROC_NUM; i++) {
    if (pcb[i].status == UNUSED) {
      // init ALL attributes of the pcb
      pcb[i].pid = next_pid;
      next_pid++;
      pcb[i].status = UNINIT; //代表这个PCB不是空的，但因为没有初始化完毕
      pcb[i].pgdir = vm_alloc();
      pcb[i].brk = 0;
      pcb[i].kstack = (kstack_t*)kalloc();
      pcb[i].ctx = &(pcb[i].kstack->ctx);
      pcb[i].parent = NULL;
      pcb[i].child_num = 0;
      sem_init(&pcb[i].zombie_sem, 0);
      pcb[i].cwd = NULL;
      for (int j = 0; j < MAX_USEM; j++) {
        pcb[i].usems[j] = NULL;
      }
      for (int j = 0; j < MAX_UFILE; j++){
        pcb[i].files[j] = NULL;
      }
      // to be continued
      return &pcb[i];
    }
  }
  // panic("No available PCB");
  return NULL;
}

void proc_free(proc_t *proc) {
  // Lab2-1: free proc's pgdir and kstack and mark it UNUSED
  // // TODO();
  //使用vm_teardown和kfree回收proc指向的进程的页目录和内核栈，然后标记该PCB的状态为UNUSED来回收进程。
  if(proc->status == RUNNING){
    return;
  }
  vm_teardown(proc->pgdir);
  kfree(proc->kstack);
  proc->status = UNUSED;
  
}

proc_t *proc_curr() {
  return curr;
}

void proc_run(proc_t *proc) {
  proc->status = RUNNING;
  curr = proc;
  // 和init_user_and_go的后半部分很像，都是设置页目录，设置内核栈，然后通过irq_iret正式开始执行。
  set_cr3(proc->pgdir);
  set_tss(KSEL(SEG_KDATA), (uint32_t)STACK_TOP(proc->kstack));
  // ctx指向这个进程的”入口“——能让这个进程开始执行的中断上下文
  irq_iret(proc->ctx);
}

void proc_addready(proc_t *proc) {
  // Lab2-1: mark proc READY
  proc->status = READY;
}

void proc_yield() {
  // Lab2-1: mark curr proc READY, then int $0x81
  curr->status = READY;
  INT(0x81); // 触发进程切换的软件中断
}

// 复制当前进程的地址空间和上下文的状态到proc这个进程中。
void proc_copycurr(proc_t *proc) {
  // Lab2-2: copy curr proc
  proc_t *curr = proc_curr();
  // 复制当前进程的页目录到proc的页目录
  vm_copycurr(proc->pgdir);
  // 把当前进程内核栈栈顶的中断上下文复制给proc的内核栈栈顶的中断上下文
  // 不需要修改proc->ctx（？）因为proc->ctx指向的是proc->kstack->ctx，所以proc->ctx已经指向了proc的内核栈栈顶的中断上下文
  proc->kstack->ctx = curr->kstack->ctx;
  // 把proc的中断上下文中的EAX改为0，这是它系统调用的返回值
  proc->ctx->eax = 0;
  proc->kstack->ctx.eax = 0;
  // 把proc的父进程设为当前进程，以及自增当前进程PCB里记录的子进程数量。
  proc->parent = curr;
  ++curr->child_num;

  // Lab2-5: dup opened usems 复制usems
  for(int i = 0; i < MAX_USEM; i++){
    if(curr->usems[i] != NULL){
      proc->usems[i] = usem_dup(curr->usems[i]);
    }
  }
  // Lab3-1: dup opened files
  for(int i = 0; i < MAX_UFILE; i++){
    if(curr->files[i] != NULL){
      proc->files[i] = fdup(curr->files[i]);
    }
  }
  // Lab3-2: dup cwd
  // // TODO();
  proc->cwd = idup(curr->cwd);
}

void proc_makezombie(proc_t *proc, int exitcode) {
  // Lab2-3: mark proc ZOMBIE and record exitcode, set children's parent to NULL
  proc->status = ZOMBIE;
  proc->exit_code = exitcode;
  // Lab2-4 告诉爹你有一个子僵尸进程了
  if(proc->parent != NULL){
    sem_v(&proc->parent->zombie_sem);
  }
  for (int i = 0; i < PROC_NUM; i++) {
    if (pcb[i].parent == proc) {
      pcb[i].parent = NULL;
      // 你爹已经似了，没人给你收尸了
    }
  }

  // Lab2-5: close opened usem
  for(int i = 0; i < MAX_USEM; i++){
    if(proc->usems[i] != NULL){
      usem_close(proc->usems[i]);
      // 不需要再设置为NULL，因为可能还有其他进程在使用这个信号量
    }
  }
  // Lab3-1: close opened files
  for(int i = 0; i < MAX_UFILE; i++){
    if(proc->files[i] != NULL){
      fclose(proc->files[i]);
    }
  }
  // Lab3-2: close cwd
  // // TODO();
  if(proc->cwd != NULL){
    iclose(proc->cwd);
  }
}

proc_t *proc_findzombie(proc_t *proc) {
  // Lab2-3: find a ZOMBIE whose parent is proc, return NULL if none
  // // TODO();
  for (int i = 0; i < PROC_NUM; i++) {
    if (pcb[i].parent == proc && pcb[i].status == ZOMBIE) {
      return &pcb[i];
    }
  }
  return NULL;
}

void proc_block() {
  // Lab2-4: mark curr proc BLOCKED, then int $0x81
  curr->status = BLOCKED;
  INT(0x81);
}

int proc_allocusem(proc_t *proc) {
  // Lab2-5: find a free slot in proc->usems, return its index, or -1 if none
  // // TODO();
  for(int i = 0; i < MAX_USEM; i++){
    if(proc->usems[i] == NULL){
      return i;
    }
  }
  return -1;
}

usem_t *proc_getusem(proc_t *proc, int sem_id) {
  // Lab2-5: return proc->usems[sem_id], or NULL if sem_id out of bound
  // // TODO();
  if(sem_id < 0 || sem_id >= MAX_USEM){
    return NULL;
  }
  return proc->usems[sem_id];
}

int proc_allocfile(proc_t *proc) {
  // Lab3-1: find a free slot in proc->files, return its index, or -1 if none
  // // TODO();
  for(int i = 0; i < MAX_UFILE; i++){
    if(proc->files[i] == NULL){
      return i;
    }
  }
  return -1;
}

file_t *proc_getfile(proc_t *proc, int fd) {
  // Lab3-1: return proc->files[fd], or NULL if fd out of bound
  // // TODO();
  if(fd < 0 || fd >= MAX_UFILE){
    return NULL;
  }
  return proc->files[fd];
}

// ctx参数的含义：每次中断的时候OS都会将当前状态作为一个中断上下文压栈保存在当前进程的内核栈里（写在Trap.S里）
void schedule(Context *ctx) {
  // Lab2-1: save ctx to curr->ctx, then find a READY proc and run it
  // // TODO();
  proc_curr()->ctx = ctx; // 当前进程要schedule到其他，所以记录保存当前进程状态的中断上下文的位置, 返回目标是int $0x81这条指令的下一条指令
  // 一段时间之后，另一个进程打算进程切换的时候，再用这个进程之前保存的上下文进行中断返回，也就相当于切换回来了

  // 错误示范：for(int i = proc_curr()->pid + 1 ;;i++)
  // pid会无限递增，与pcb无关，我找了3小时！！
  for(proc_t *proc = proc_curr();;)
  {
    //从当前进程在pcb的下一个位置开始循环遍历整个数组
    if(proc == &pcb[PROC_NUM-1]) proc=&pcb[0]; //循环遍历
    else proc++;

    if(proc->status == READY){
      // 用另一个进程之前保存的上下文进行中断返回，进入另一个进程
      proc_run(proc);
      break;
    }
  }
  
}
