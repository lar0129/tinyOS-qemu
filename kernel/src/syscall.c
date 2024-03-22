#include "klib.h"
#include "cte.h"
#include "sysnum.h"
#include "vme.h"
#include "serial.h"
#include "loader.h"
#include "proc.h"
#include "timer.h"
#include "file.h"

typedef int (*syshandle_t)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);

extern void *syscall_handle[NR_SYS];

void do_syscall(Context *ctx) {
  // TODO: Lab1-5 call specific syscall handle and set ctx register
  int sysnum = ctx->eax;
  // printf("\nsyscall num: %d\n", sysnum);
  uint32_t arg1 = ctx->ebx;
  uint32_t arg2 = ctx->ecx;
  uint32_t arg3 = ctx->edx;
  uint32_t arg4 = ctx->esi;
  uint32_t arg5 = ctx->edi;
  int res;
  if (sysnum < 0 || sysnum >= NR_SYS) {
    res = -1;
  } else {
    // 函数指针，返回int。数组syscall_handle中存放了所有系统调用的处理函数
    res = ((syshandle_t)(syscall_handle[sysnum]))(arg1, arg2, arg3, arg4, arg5);
  }
  ctx->eax = res;
}

// buf是虚拟地址，count是字节数
// 页目录PD里不仅有给内核用的PHY_MEM以下的恒等映射，还有用户程序的那块虚拟内存，
// 因此在操作系统里也可以解指向用户程序地址的指针buf。
int sys_write(int fd, const void *buf, size_t count) {
  // TODO: rewrite me at Lab3-1
  return serial_write(buf, count);
}

int sys_read(int fd, void *buf, size_t count) {
  // TODO: rewrite me at Lab3-1
  return serial_read(buf, count);
}

int sys_brk(void *addr) {
  // TODO: Lab1-5
  static size_t brk = 0; // use brk of proc instead of this in Lab2-1
  size_t new_brk = PAGE_UP(addr);  // 保证我们操作系统认为的program break一定不小于用户程序实际的program break
  if (brk == 0) {
    brk = new_brk;
  } else if (new_brk > brk) { // 说明用户程序需要增长堆区
    PD *pd_curr =vm_curr();
    // 创建[brk, new_brk)这段虚拟内存的映射
    vm_map(pd_curr, brk, new_brk - brk, 0x7);
    brk = new_brk;
  } else if (new_brk < brk) {
    // can just do nothing
    PD *pd_curr =vm_curr();
    vm_unmap(pd_curr, new_brk, brk - new_brk);
    brk = new_brk;
  }
  return 0;
}

void sys_sleep(int ticks) {
  // 在操作系统睡ticks刻时间，然后再回到用户程序。
  // // TODO(); // Lab1-7
  uint32_t cur = get_tick();
  while (get_tick() - cur < ticks)
  {
    sti(); hlt(); cli();
  }
  
}

// 把当前程序替换成要执行的程序, path是程序名，argv是参数列表
int sys_exec(const char *path, char *const argv[]) {
  // // TODO(); // Lab1-8
  PD *pgdir = vm_alloc(); //不用现在的页目录是因为path和argv还指向用户程序那部分的虚拟内存，并且还有继续执行原程序的可能
  Context ctx;
  // 接着调用load_user加载新用户程序到新页目录并初始化中断上下文，
  // 如果load_user返回非0代表加载新程序失败，把新页目录释放掉然后直接返回-1，代表exec失败，继续执行原程序。
  int ret = load_user(pgdir, &ctx, path, argv);
  if(ret != 0) {
    kfree(pgdir);
    return -1;
  }

  // 如果load_user返回0，那么代表加载成功，接下来要执行的是新的程序，因此先调用vm_curr记录一下现在的页目录，
  PD *pd_curr = vm_curr();
  // 然后用set_cr3切换到新页目录，然后回收旧页目录，再用irq_iret和前面定义的上下文通过返回中断来进入新用户程序即可。
  set_cr3(pgdir);
  kfree(pd_curr);
  irq_iret(&ctx);
  // 内核栈处理：因为此时旧程序不会再被执行了，所以内核栈我们可以继续用现在的这个，不必更改。

  // Lab2-1
}

int sys_getpid() {
  TODO(); // Lab2-1
}

void sys_yield() {
  proc_yield();
}

int sys_fork() {
  TODO(); // Lab2-2
}

void sys_exit(int status) {
  TODO(); // Lab2-3
}

int sys_wait(int *status) {
  TODO(); // Lab2-3, Lab2-4
}

int sys_sem_open(int value) {
  TODO(); // Lab2-5
}

int sys_sem_p(int sem_id) {
  TODO(); // Lab2-5
}

int sys_sem_v(int sem_id) {
  TODO(); // Lab2-5
}

int sys_sem_close(int sem_id) {
  TODO(); // Lab2-5
}

int sys_open(const char *path, int mode) {
  TODO(); // Lab3-1
}

int sys_close(int fd) {
  TODO(); // Lab3-1
}

int sys_dup(int fd) {
  TODO(); // Lab3-1
}

uint32_t sys_lseek(int fd, uint32_t off, int whence) {
  TODO(); // Lab3-1
}

int sys_fstat(int fd, struct stat *st) {
  TODO(); // Lab3-1
}

int sys_chdir(const char *path) {
  TODO(); // Lab3-2
}

int sys_unlink(const char *path) {
  return iremove(path);
}

// optional syscall

void *sys_mmap() {
  TODO();
}

void sys_munmap(void *addr) {
  TODO();
}

int sys_clone(void (*entry)(void*), void *stack, void *arg) {
  TODO();
}

int sys_kill(int pid) {
  TODO();
}

int sys_cv_open() {
  TODO();
}

int sys_cv_wait(int cv_id, int sem_id) {
  TODO();
}

int sys_cv_sig(int cv_id) {
  TODO();
}

int sys_cv_sigall(int cv_id) {
  TODO();
}

int sys_cv_close(int cv_id) {
  TODO();
}

int sys_pipe(int fd[2]) {
  TODO();
}

int sys_link(const char *oldpath, const char *newpath) {
  TODO();
}

int sys_symlink(const char *oldpath, const char *newpath) {
  TODO();
}

void *syscall_handle[NR_SYS] = {
  [SYS_write] = sys_write,
  [SYS_read] = sys_read,
  [SYS_brk] = sys_brk,
  [SYS_sleep] = sys_sleep,
  [SYS_exec] = sys_exec,
  [SYS_getpid] = sys_getpid,
  [SYS_yield] = sys_yield,
  [SYS_fork] = sys_fork,
  [SYS_exit] = sys_exit,
  [SYS_wait] = sys_wait,
  [SYS_sem_open] = sys_sem_open,
  [SYS_sem_p] = sys_sem_p,
  [SYS_sem_v] = sys_sem_v,
  [SYS_sem_close] = sys_sem_close,
  [SYS_open] = sys_open,
  [SYS_close] = sys_close,
  [SYS_dup] = sys_dup,
  [SYS_lseek] = sys_lseek,
  [SYS_fstat] = sys_fstat,
  [SYS_chdir] = sys_chdir,
  [SYS_unlink] = sys_unlink,
  [SYS_mmap] = sys_mmap,
  [SYS_munmap] = sys_munmap,
  [SYS_clone] = sys_clone,
  [SYS_kill] = sys_kill,
  [SYS_cv_open] = sys_cv_open,
  [SYS_cv_wait] = sys_cv_wait,
  [SYS_cv_sig] = sys_cv_sig,
  [SYS_cv_sigall] = sys_cv_sigall,
  [SYS_cv_close] = sys_cv_close,
  [SYS_pipe] = sys_pipe,
  [SYS_link] = sys_link,
  [SYS_symlink] = sys_symlink};
