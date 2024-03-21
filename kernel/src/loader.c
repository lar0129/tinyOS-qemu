#include "klib.h"
#include "vme.h"
#include "cte.h"
#include "loader.h"
#include "disk.h"
#include "fs.h"
#include <elf.h>

uint32_t load_elf(PD *pgdir, const char *name) {
  Elf32_Ehdr elf;
  Elf32_Phdr ph;
  // 打开名字为name的文件，返回其对应的inode_t*（可以类比为FILE*，返回NULL如果文件不存在）
  inode_t *inode = iopen(name, TYPE_NONE);
  if (!inode) return -1;
  // 从inode代表的文件的'0'偏移量处，读取len字节到内存的elf里
  iread(inode, 0, &elf, sizeof(elf));
  if (*(uint32_t*)(&elf) != 0x464c457f) { // check ELF magic number
    iclose(inode);
    return -1;
  }
  PD * pd_curr = vm_curr();
  for (int i = 0; i < elf.e_phnum; ++i) {
    iread(inode, elf.e_phoff + i * sizeof(ph), &ph, sizeof(ph));
    if (ph.p_type == PT_LOAD) {
      // TODO: Lab1-2: Load segment to physical memory
      // uint32_t elfAddr = (uint32_t)(&elf);是不对的，elf是临时变量
      // 对于p_type == PT_LOAD的表项，将ELF文件中起始于p_offset，大小为p_filesz字节的数据拷贝到内存中起始于p_vaddr的位置
      // 并将内存中剩余的p_memsz - p_filesz字节的内容清零

      // iread(inode, ph.p_offset, (void *)ph.p_vaddr, ph.p_filesz);
      // memset((void *)((void *)ph.p_vaddr+ph.p_filesz), 0, ph.p_memsz - ph.p_filesz);
    
      // TODO: Lab1-4: Load segment to virtual memory

       // p_flags，如果它的PF_W位为1（即(p_flags & PF_W) != 0），就代表这个段是可写的，否则就是只读
      int prot =  (ph.p_flags & PF_W) ? 0x7 : 0x5;
      // 将这一段所需要的虚拟内存（即[p_vaddr, p_vaddr+p_memsz)）添加到pgdir管理的映射，你可以使用前面实现的vm_map函数
      vm_map(pgdir, ph.p_vaddr, ph.p_memsz, prot);

      // 将虚拟地址转换为实际映射到的物理地址，并将这一段的内容加载到物理地址中.
        // 不论是内核页目录还是用户页目录，都将虚拟内存[0, PHY_MEM)映射到物理内存[0, PHY_MEM)（恒等映射）
        // 对于用户程序，我们改为链接到0x08048000，这个地址高于PHY_MEM，这样就不会和[0, PHY_MEM)这块恒等映射冲突了
        // 至于这块虚拟内存映射到物理内存的哪里，由操作系统说了算。
      set_cr3(pgdir);
      iread(inode, ph.p_offset, (void *)ph.p_vaddr, ph.p_filesz);
      memset((void*)((uint32_t)(ph.p_vaddr) + ph.p_filesz), 0, ph.p_memsz-ph.p_filesz);
    }
  }
  // TODO: Lab1-4 alloc stack memory in pgdir
  // 给pgdir映射上[0xbffff000, 0xc0000000)（即[USR_MEM-PGSIZE, USR_MEM)）这一块虚拟内存作为用户程序的栈。
  set_cr3(pd_curr);
  
  vm_map(pgdir, USR_MEM - PGSIZE, PGSIZE, 0x7);
  iclose(inode);
  return elf.e_entry;
}

#define MAX_ARGS_NUM 31

uint32_t load_arg(PD *pgdir, char *const argv[]) {
  // Lab1-8: Load argv to user stack
  char *stack_top = (char*)vm_walk(pgdir, USR_MEM - PGSIZE, 7) + PGSIZE;
  size_t argv_va[MAX_ARGS_NUM + 1];
  int argc;
  for (argc = 0; argv[argc]; ++argc) {
    assert(argc < MAX_ARGS_NUM);
    // push the string of argv[argc] to stack, record its va to argv_va[argc]
    TODO();
  }
  argv_va[argc] = 0; // set last argv NULL
  stack_top -= ADDR2OFF(stack_top) % 4; // align to 4 bytes
  for (int i = argc; i >= 0; --i) {
    // push the address of argv_va[argc] to stack to make argv array
    stack_top -= sizeof(size_t);
    *(size_t*)stack_top = argv_va[i];
  }
  // push the address of the argv array as argument for _start
  TODO();
  // push argc as argument for _start
  stack_top -= sizeof(size_t);
  *(size_t*)stack_top = argc;
  stack_top -= sizeof(size_t); // a hole for return value (useless but necessary)
  return USR_MEM - PGSIZE + ADDR2OFF(stack_top);
}

// 1.加载用户程序
// 2.保存用户栈的位置在中断上下文。中断返回时要恢复
int load_user(PD *pgdir, Context *ctx, const char *name, char *const argv[]) {
  size_t eip = load_elf(pgdir, name);
  if (eip == -1) return -1;

  ctx->cs = USEL(SEG_UCODE); // 用户态的用户代码段
  ctx->ds = USEL(SEG_UDATA); // 用户态的用户数据段
  ctx->eip = eip; // 用户程序的入口地址
  // TODO: Lab1-6 init ctx->ss and esp
  ctx->ss = USEL(SEG_UDATA); // 用户态的数据段，即用户栈基址
  ctx->esp = USR_MEM-16; // 用户栈栈顶

  ctx->eflags = 0x002; // TODO: Lab1-7 change me to 0x202
  return 0;
}
