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

// 从buf写count字节到fd表示的文件，返回写的字节数（或-1如果失败）
int sys_write(int fd, const void *buf, size_t count) {
  // // TODO: rewrite me at Lab3-1
  // return serial_write(buf, count);
  // 从当前进程中取出对应文件描述符的file_t*，如果是NULL即没有对应的文件，就返回-1，
  // 有的话就调用虚拟文件系统那块的对应API。
  file_t *file = proc_getfile(proc_curr(), fd);
  if (file == NULL) {
    return -1;
  }
  return fwrite(file, buf, count);
}

// 从fd表示的文件读count字节到buf，返回读的字节数（或-1如果失败）
int sys_read(int fd, void *buf, size_t count) {
  // // TODO: rewrite me at Lab3-1
  // return serial_read(buf, count);
  // 从当前进程中取出对应文件描述符的file_t*，如果是NULL即没有对应的文件，就返回-1，
  // 有的话就调用虚拟文件系统那块的对应API。
  file_t *file = proc_getfile(proc_curr(), fd);
  if (file == NULL) {
    return -1;
  }
  return fread(file, buf, count);
}

int sys_brk(void *addr) {
  // TODO: Lab1-5
  // static size_t brk = 0; // use brk of proc instead of this in Lab2-1
  size_t brk = proc_curr()->brk;
  size_t new_brk = PAGE_UP(addr);  // 保证我们操作系统认为的program break一定不小于用户程序实际的program break
  if (brk == 0) {
    proc_curr()->brk = new_brk; // 修改时必须用当前进程的指针！！！不能用brk
  } else if (new_brk > brk) { // 说明用户程序需要增长堆区
    PD *pd_curr =vm_curr();
    // 创建[brk, new_brk)这段虚拟内存的映射
    vm_map(pd_curr, brk, new_brk - brk, 0x7);
    proc_curr()->brk = new_brk;// 修改时必须用当前进程的指针！！！不能用brk
  } else if (new_brk < brk) {
    // can just do nothing
    PD *pd_curr =vm_curr();
    vm_unmap(pd_curr, new_brk, brk - new_brk);
    proc_curr()->brk = new_brk;
  }
  return 0;
}

// TODO: 线程调度
void sys_sleep(int ticks) {
  // 在操作系统睡ticks刻时间，然后再回到用户程序。
  // // TODO(); // Lab1-7
  uint32_t cur = get_tick();
  while (get_tick() - cur < ticks)
  {
    // sti(); hlt(); cli(); // 啥也不干
    proc_yield(); // 进程切换
  }
  
}

// 把当前程序替换成要执行的程序, path是程序名，argv是参数列表
// 这两个程序还是一个进程，这一个进程在时间先后上对应不同的用户程序，所以新程序仍然继续使用这个进程的PCB。
int sys_exec(const char *path, char *const argv[]) {
  // // TODO(); // Lab1-8
  PD *pgdir = vm_alloc(); //不用现在的页目录是因为path和argv还指向用户程序那部分的虚拟内存，并且还有继续执行原程序的可能
  proc_curr()->pgdir = pgdir; // 要切换到新的用户程序时，虚拟地址空间映射的物理空间发生了变化,要修改当前进程PCB里记录的页目录pgdir。
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
  // // TODO(); // Lab2-1
  return proc_curr()->pid;
}

void sys_yield() {
  // 用户态进程主动让出CPU，调用proc_yield，然后触发进程切换的软中断。
  proc_yield();
}

int sys_fork() {
  // // TODO(); // Lab2-2
  proc_t *child = proc_alloc();
  if (child == NULL) {
    return -1;
  }
  proc_copycurr(child);
  proc_addready(child);
  return child->pid;
}

// 系统调用退出当前进程，并记录退出状态为status，父进程可以通过wait系统调用获取子进程的退出状态
// TODO: 线程调度
void sys_exit(int status) {
  // // TODO(); // Lab2-3
  // while (1) proc_yield();
  proc_makezombie(proc_curr(), status);

  // 退出当前进程后切换进程
  INT(0x81); 
  assert(0);
}

// 返回一个退出的子进程的PID。如果status不为NULL，再把子进程的退出状态记录在那，最后释放这个子进程的PCB。
// TODO: 线程调度
int sys_wait(int *status) {
  // // TODO(); // Lab2-3,
  // sys_sleep(250);
  // return 0;
  if(proc_curr()->child_num == 0) {
    return -1;
  }
  proc_t *zombie;
  // while((zombie = proc_findzombie(proc_curr())) == NULL) {
  //   proc_yield();
  // }
  sem_p(&proc_curr()->zombie_sem); // 找不到僵尸进程就阻塞
  zombie = proc_findzombie(proc_curr());
  assert(zombie != NULL);

  if (status!=NULL){
    *status = zombie->exit_code;
  }
  int zombie_pid = zombie->pid;
  proc_free(zombie);
  (proc_curr()->child_num)--;
  return zombie_pid;

  
  // Lab2-4
}

// 打开一个初值为value的用户信号量，成功返回其编号，失败返回-1
int sys_sem_open(int value) {
  // // TODO(); // Lab2-5
  int usemIdx = proc_allocusem(proc_curr());
  if(usemIdx == -1){
    return -1;
  }

  usem_t *usem = usem_alloc(value); //从总的信号量数组中找到一个空闲的信号量，初始化并返回给当前进程的表
  if (usem == NULL) {
    return -1;
  }

  proc_curr()->usems[usemIdx] = usem;
  return usemIdx;
}

int sys_sem_p(int sem_id) {
  // // TODO(); // Lab2-5
  usem_t *usem = proc_getusem(proc_curr(), sem_id);
  if (usem == NULL)
  {
    return -1;
  }
  
  sem_p(&usem->sem);
  return 0;
}

int sys_sem_v(int sem_id) {
  // // TODO(); // Lab2-5
  usem_t *usem = proc_getusem(proc_curr(), sem_id);
  if (usem == NULL)
  {
    return -1;
  }
  
  sem_v(&usem->sem);
  return 0;
}

int sys_sem_close(int sem_id) {
  // // TODO(); // Lab2-5
  usem_t *usem = proc_getusem(proc_curr(), sem_id);
  if (usem == NULL)
  {
    return -1;
  }
  
  usem_close(usem); // ref--
  proc_curr()->usems[sem_id] = NULL; // 释放进程信号量表为NULL
  return 0;
}

// 打开path代表的文件，返回其文件描述符（要求为可用中最小的），失败返回-1，mode的意义同前面介绍的fopen
int sys_open(const char *path, int mode) {
  // // TODO(); // Lab3-1
  // 首先调用proc_allocfile找一个空的文件描述符，如果没有返回-1，
  // 再调用fopen打开文件，打开失败也返回-1，都成功的话就把打开的file_t的指针放在用户打开文件表的对应位置。
  int fd = proc_allocfile(proc_curr());
  if (fd == -1) {
    return -1;
  }
  file_t *file = fopen(path, mode);
  if (file == NULL) {
    return -1;
  }
  proc_curr()->files[fd] = file;
  return fd;
}

// 关闭fd表示的文件，成功返回0，失败返回-1
int sys_close(int fd) {
  // // TODO(); // Lab3-1
  // 首先调用proc_getfile从当前进程中取出对应文件描述符的file_t*，如果是NULL即没有对应的文件，就返回-1，
  // 有的话就调用虚拟文件系统那块的对应API，不过close最后还要把当前进程用户打开文件表中的对应项设为NULL。
  file_t *file = proc_getfile(proc_curr(), fd);
  if (file == NULL) {
    return -1;
  }
  fclose(file);
  proc_curr()->files[fd] = NULL;
  return 0;
}

// dup系统调用用来复制文件描述符到当前进程，这样可以让不同的文件描述符对应到相同的file_t*
// 复制fd表示的file_t指针到新的文件描述符并返回（要求为可用中最小的），失败返回-1
int sys_dup(int fd) {
  //  // TODO(); // Lab3-1
  // sys_dup既要调用proc_allocfile找一个空的文件描述符，也要调用proc_getfile从当前进程中取出对应文件描述符的file_t*，任一失败都会返回-1，
  // 都成功的话就把这个file_t*放在用户打开文件表的对应位置，不过别忘记调用fdup增加引用计数
  int new_fd = proc_allocfile(proc_curr());
  if (new_fd == -1) {
    return -1;
  }
  file_t *file = proc_getfile(proc_curr(), fd);
  if (file == NULL) {
    return -1;
  }
  file_t *new_file = fdup(file);
  proc_curr()->files[new_fd] = new_file;
  return new_fd;
}

// 调整fd指向的文件的文件的偏移量并返回，whence的意义同前面介绍的fseek，失败返回-1
uint32_t sys_lseek(int fd, uint32_t off, int whence) {
  // // TODO(); // Lab3-1
  // 首先调用proc_getfile从当前进程中取出对应文件描述符的file_t*，如果是NULL即没有对应的文件，就返回-1，
  // 有的话就调用虚拟文件系统那块的对应API。
  file_t *file = proc_getfile(proc_curr(), fd);
  if (file == NULL) {
    return -1;
  }
  return fseek(file, off, whence);
}

// 记录fd指向的文件的信息于st结构体中，成功返回0，失败返回-1
int sys_fstat(int fd, struct stat *st) {
  // // TODO(); // Lab3-1
  // 在proc_getfile从当前进程中取出对应文件描述符的file_t*后，首先判断文件类型，
  // 磁盘文件的话调用磁盘文件系统的API（itype、isize、ino）来获得相关信息，设备文件的话只需要设置type为TYPE_DEV，其余两项均设为0即可。
  file_t *file = proc_getfile(proc_curr(), fd);
  if (file == NULL) {
    return -1;
  }
  if (file->type == TYPE_FILE) {
    st->type = itype(file->inode);
    st->size = isize(file->inode);
    st->node = ino(file->inode);
  } else if (file->type == TYPE_DEV) {
    st->type = TYPE_DEV;
    st->size = 0;
    st->node = 0;
  }
  return 0;
}

// 作用是改变当前进程的cwd为path这一路径指向的目录，成功返回0，失败返回-1
int sys_chdir(const char *path) {
  // // TODO(); // Lab3-2
  inode_t *inode = iopen(path,TYPE_NONE);
  if (inode == NULL) {
    return -1;
  }
  if (itype(inode) != TYPE_DIR) {
    iclose(inode);
    return -1;
  }
  iclose(proc_curr()->cwd);
  proc_curr()->cwd = inode;
  return 0;
}

// 作用就是删除这个文件，所以直接调用iremove即可
int sys_unlink(const char *path) {
  return iremove(path);
}

// optional syscall

// 分别是申请和释放一页共享内存，共享内存在fork时会被共享给子进程，也就是父子进程的这个虚拟地址映射到相同的物理地址。
// 我们规定这种内存的虚拟地址都在USR_MEM以上，这样处理起来可能比较方便，对物理页维护引用计数是解决这个问题的好方法。
void *sys_mmap() {
  TODO();
}

void sys_munmap(void *addr) {
  TODO();
}

/*
  实现的是内核级线程：调度由操作系统负责，可以利用时钟中断来切换
  从entry这个函数开始执行，并传入参数 arg，栈区为[stack-PGSIZE, stack)。
*/
// 线程是同一进程内更小的计算单位。它们之间共享同一进程的地址空间，但是栈不同，寄存器也相互独立。
// 创建一个线程，只需要为它分配一块栈区即可，不需要像fork一样进行地址空间拷贝

//一个进程可能拥有若干个线程，因此你需要在kernel中设计相关的数据结构（线程描述符）来管理进程与线程之间的逻辑。
//线程只有栈需要分配，而代码与全局变量这些静态数据是共享的。你需要正确处理线程与进程在调度与资源分配上的关系。
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
  // return 0 if success, -1 if failed
  // // TODO();
  if (fd[0] == -1 || fd[1] == -1) {
    panic("sys_pipe: proc_allocfile failed");
    return -1;
  }
  file_t *file0 = NULL;
  file_t *file1 = NULL;
  int res = fcreate_pipe(&file0, &file1);
  if (res == -1) {
    panic("sys_pipe: fcreate_pipe failed");
    return -1;
  }

  fd[0] = proc_allocfile(proc_curr());
  proc_curr()->files[fd[0]] = file0;
  fd[1] = proc_allocfile(proc_curr());
  proc_curr()->files[fd[1]] = file1;
  // printf("sys_pipe: fd[0] = %d, fd[1] = %d\n", fd[0], fd[1]);
  // printf("sys_pipe: file0 = %x, file1 = %x\n", file0, file1);
  return 0;
}

int sys_link(const char *oldpath, const char *newpath) {
  // // TODO();
  inode_t *inode = iopen(oldpath, TYPE_NONE);
  if (inode == NULL) {
    return -1;
  }
  int result =  ilink(newpath, inode);
  iclose(inode);
  return result;
}

int sys_symlink(const char *oldpath, const char *newpath) {
  int result =  isymlink(newpath, oldpath);
  return result;
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
