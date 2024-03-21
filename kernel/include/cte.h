#ifndef __CTE_H__
#define __CTE_H__

#include <stdint.h>

// TODO: Lab1-5 adjust the struct to the correct order
// TODO: Lab1-6 add esp and ss
typedef struct Context {
  uint32_t 
          ds, ebp, edi, esi, edx, ecx, ebx, eax,
          irq, errcode,eip, cs, eflags,
          esp,ss;
           // 栈中从上到下/从高地址到低地址（push的顺序）：EFLAGS、CS、EIP（这仨是硬件压栈的）、错误码（或“伪错误码”）、中断号、
           // EAX、EBX、ECX、EDX、ESI、EDI、EBP、DS
           // 压栈后，调用irq_handle，参数即这个结构体（因此顺序相反，从低到高），然后调用irq_iret，弹栈
} Context;

void init_cte();
void irq_iret(Context *ctx) __attribute__((noreturn));

void do_syscall(Context *ctx);
void exception_debug_handler(Context *ctx);

#endif
