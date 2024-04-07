#include "klib.h"
#include "serial.h"
#include "vme.h"
#include "cte.h"
#include "loader.h"
#include "fs.h"
#include "proc.h"
#include "timer.h"
#include "dev.h"

void init_user_and_go();

int main() {
  init_gdt();
  init_serial();
  init_fs();
  init_page(); // uncomment me at Lab1-4
  init_cte(); // uncomment me at Lab1-5
  init_timer(); // uncomment me at Lab1-7
  init_proc(); // uncomment me at Lab2-1
  //init_dev(); // uncomment me at Lab3-1

  printf("Hello from OS!\n");
  init_user_and_go();
  panic("should never come back");
}

void init_user_and_go() {
  // Lab1-2: ((void(*)())eip)();
  // uint32_t eip = load_elf(NULL, "loaduser"); // 加载用户程序
  // assert(eip != -1);
  // ((void(*)())eip)(); // 跳转入口地址

  // Lab1-4: pdgir, stack_switch_call
  // PD *pgdir = vm_alloc();
  // uint32_t eip = load_elf(pgdir, "systest");
  // assert(eip != -1);
  // set_cr3(pgdir);
  // // 跳转到用户程序，修改ESP到用户栈(USR_MEM - 16)的位置
  // stack_switch_call((void*)(USR_MEM - 16), (void*)eip, 0);

  // Lab1-6: ctx, irq_iret
  // 建了个页目录，建了个用于进入用户程序的中断上下文，加载用户程序，切换页目录，设置内核栈，最后通过中断返回进入用户程序
  // PD *pgdir = vm_alloc();
  // Context ctx; // 中断上下文用于返回用户态
  // char *argv[] = {"sh1", NULL}; // test argv
  // assert(load_user(pgdir, &ctx, "sh1", argv) == 0);
  // set_cr3(pgdir);
  // // 准备好操作系统的内核栈，存在tss里
  // // tss告诉CPU我们的内核栈的栈顶在哪
  // set_tss(KSEL(SEG_KDATA), (uint32_t)kalloc() + PGSIZE); //kalloc一个内核栈,用于用户态到内核态的中断
  // // 利用构造的中断上下文“假装”中断返回，以此进入用户程序
  // irq_iret(&ctx);

  // Lab1-8: argv
  // Lab2-1: proc
  proc_t *proc = proc_alloc();
  assert(proc);
  char *argv[] = {"sh1", NULL};
  assert(load_user(proc->pgdir, proc->ctx, "sh1", argv) == 0);
  // 调度当前进程
  proc_addready(proc);
  sti(); // 让内核进程最后把中断打开，这样如果别的进程都在关中断的内核态时，只要切换到内核进程时就可以处理中断了
  while (1);

  // Lab3-2: add cwd
}
