#include "boot.h"

// DO NOT DEFINE ANY NON-LOCAL VARIBLE!

void load_kernel() {
  // char hello[] = {'\n', 'h', 'e', 'l', 'l', 'o', '\n', 0};
  // putstr(hello);
  // while (1) ;

  // remove both lines above before write codes below
  Elf32_Ehdr *elf = (void *)0x8000;
  copy_from_disk(elf, 255 * SECTSIZE, SECTSIZE);
  Elf32_Phdr *ph, *eph;
  ph = (void*)((uint32_t)elf + elf->e_phoff); // 定位程序头表
  eph = ph + elf->e_phnum; // elf头表结束，elf文件起始
  for (; ph < eph; ph++) {
    if (ph->p_type == PT_LOAD) {
      // TODO: Lab1-2, Load kernel and jump
      memcpy((void *)ph->p_vaddr, (void *)((uint32_t)elf+ph->p_offset), ph->p_filesz);
      memset((void *)((uint32_t)elf+ph->p_offset+ph->p_filesz), 0, ph->p_memsz - ph->p_filesz);
    }
  }
  uint32_t entry = elf->e_entry; // change me
  ((void(*)())entry)(); // 将 entry 强制转换为函数指针，并调用这个函数
}
