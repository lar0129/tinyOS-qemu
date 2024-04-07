#include "klib.h"
#include "vme.h"
#include "proc.h"

static TSS32 tss;

void init_gdt() {
  static SegDesc gdt[NR_SEG];
  gdt[SEG_KCODE] = SEG32(STA_X | STA_R,   0,     0xffffffff, DPL_KERN);
  gdt[SEG_KDATA] = SEG32(STA_W,           0,     0xffffffff, DPL_KERN);
  gdt[SEG_UCODE] = SEG32(STA_X | STA_R,   0,     0xffffffff, DPL_USER);
  gdt[SEG_UDATA] = SEG32(STA_W,           0,     0xffffffff, DPL_USER);
  gdt[SEG_TSS]   = SEG16(STS_T32A,     &tss,  sizeof(tss)-1, DPL_KERN);
  set_gdt(gdt, sizeof(gdt[0]) * NR_SEG);
  set_tr(KSEL(SEG_TSS));
}

void set_tss(uint32_t ss0, uint32_t esp0) {
  tss.ss0 = ss0;
  tss.esp0 = esp0;
}

static PD kpd;
static PT kpt[PHY_MEM / PT_SIZE] __attribute__((used));

typedef union free_page {
  union free_page *next;
  char buf[PGSIZE]; // 页的内容，前4位为指针
} page_t;

page_t *free_page_list;

void init_page() {
  extern char end; // 地址end是链接器脚本生成的，指向内核的结束地址
  panic_on((size_t)(&end) >= KER_MEM - PGSIZE, "Kernel too big (MLE)"); // 内核栈的位置
  static_assert(sizeof(PTE) == 4, "PTE must be 4 bytes");
  static_assert(sizeof(PDE) == 4, "PDE must be 4 bytes");
  static_assert(sizeof(PT) == PGSIZE, "PT must be one page");
  static_assert(sizeof(PD) == PGSIZE, "PD must be one page");
  // Lab1-4: init kpd and kpt, identity mapping of [0 (or 4096), PHY_MEM)
  // 把kpd的第i个PDE对应到kpt[i]这个页表（可以使用MAKE_PDE这个宏，注意等号左边必须是一个PDE的val字段，
  // 宏第一个参数必须是页表的地址），权限设成1或者3都行（毕竟内核态不检查R/W位）
  for(int i = 0; i < PHY_MEM / PT_SIZE ; i++){
    kpd.pde[i].val = MAKE_PDE((uint32_t)&kpt[i], 3);
    //kpt[i]这个页表的第j个PTE（注意一个页表有NR_PTE即1024个PTE），其对应的虚拟页的“页目录号”是i
    //（因为kpd的第i个PDE指向kpt[i]这个页表），“页表号”自然是j，所以虚拟页的地址是(i << DIR_SHIFT) | (j << TBL_SHIFT)，
    //而因为我们要建立的是恒等映射，因此这个PTE要指向的物理页的地址也是这个（可以使用MAKE_PTE这个宏）。
    for(int j=0; j < NR_PTE;j++){
      kpt[i].pte[j].val = MAKE_PTE((i << DIR_SHIFT) | (j << TBL_SHIFT), 3);
    }
  }
 //  changeTODO();


  kpt[0].pte[0].val = 0;
  set_cr3(&kpd);
  set_cr0(get_cr0() | CR0_PG);
  
  // Lab1-4: init free memory at [KER_MEM, PHY_MEM), a heap for kernel
  // changeTODO();
  page_t *start = (page_t*)PAGE_DOWN(KER_MEM);
  free_page_list = start;
  while ((uint32_t)free_page_list < (uint32_t)PHY_MEM - PGSIZE) {
    free_page_list->next = (page_t*)(PAGE_DOWN((uint32_t)free_page_list + PGSIZE));
    free_page_list = free_page_list->next;
  }
  free_page_list->next = NULL;
  free_page_list = start; // 回到页表的起始位置
}

void *kalloc() {
  // Lab1-4: alloc a page from kernel heap, abort when heap empty

  if (free_page_list == NULL) {
    panic("Kernel heap is empty");
  }
  
  page_t *alloc_page = free_page_list;
  free_page_list = free_page_list->next;

  for(int i = 0; i < PGSIZE; i++){
    alloc_page->buf[i] = 0;
  }
  return alloc_page;
}

void kfree(void *ptr) {
  // Lab1-4: free a page to kernel heap
  // you can just do nothing :)
  if ((uint32_t)ptr < KER_MEM || (uint32_t)ptr >= PHY_MEM)
  {
    panic("kfree: ptr not in kernel heap");
  }
  assert(PAGE_DOWN((uint32_t)free_page_list) >= KER_MEM && PAGE_DOWN((uint32_t)free_page_list) < PHY_MEM);
  assert(PAGE_DOWN((uint32_t)ptr) >= KER_MEM && PAGE_DOWN((uint32_t)ptr) < PHY_MEM);

  page_t *page = (page_t*)(PAGE_DOWN(ptr));
  for(int i = 0; i < PGSIZE; i++){
    page->buf[i] = 0;
  }
  page->next = free_page_list;
  free_page_list = page;

}

PD *vm_alloc() {
  // Lab1-4: alloc a new pgdir, map memory under PHY_MEM identityly
  // 先用kalloc申请一页作为页目录，然后可以利用已经做好映射的kpt，把前32个PDE对应到kpt的32个页表上
  // 对内核（0-31项），一一映射
  PD *pgdir = kalloc();
  for(int i = 0; i < PHY_MEM / PT_SIZE ; i++){
    pgdir->pde[i].val = MAKE_PDE((uint32_t)&kpt[i], 3);
  }
  // 其他的PDE都置0
  for (int i = PHY_MEM / PT_SIZE; i < NR_PDE; i++)
  {
    pgdir->pde[i].val = 0;
  }
  
  return pgdir;
}

void vm_teardown(PD *pgdir) {
  // Lab1-4: free all pages mapping above PHY_MEM in pgdir, then free itself
  // you can just do nothing :)
  // 把pgdir这一页目录下下辖的所有页表，和所映射到的所有物理页全部kfree。
  // 但是：前32个PDE对应的页表（即kpt）及其映射的物理页（即[0, PHY_MEM)）不要kfree（可以认为除了这些页表和物理页之外，其余页表和物理页都是kalloc出来的）
  for(int i =  PHY_MEM / PT_SIZE; i < NR_PDE ; i++){
    if (pgdir->pde[i].present == 1)
    {
      PT *pt = PDE2PT((PDE)(pgdir->pde[i]));
      for(int j = 0; j < NR_PTE; j++){
        if (pt->pte[j].present == 1)
        {
          kfree((void*)(PTE2PG((PTE)(pt->pte[j]))));
        }
      }
      kfree(pt);
    }
  }

  kfree(pgdir);
}

PD *vm_curr() {
  return (PD*)PAGE_DOWN(get_cr3());
}

// prot=0是查找pte，prot!=0是构造pte
PTE *vm_walkpte(PD *pgdir, size_t va, int prot) {
  // Lab1-4: return the pointer of PTE which match va
  int pd_index = ADDR2DIR(va); // 计算“页目录号PDE”
  PDE *pde = &(pgdir->pde[pd_index]); // 找到对应的页目录项PDE
  PT *pt;

  // if not exist (PDE of va is empty) and prot&1, alloc PT and fill the PDE
  // remember to let pde's prot |= prot, but not pte
  // 权限足，但PDE不存在，那么分配一个页表PT，并填充PDE
  if (!(pde->present & 1) && (prot & 1))
  {
    pt = kalloc();
    pde->val = MAKE_PDE((uint32_t)pt, prot);
    int pt_index = ADDR2TBL(va); // 计算“页表号”PTE
    PTE *pte = &(pt->pte[pt_index]); // 找到对应的页表项PTE
    assert((prot & ~7) == 0);
    return pte;
  }
  // if not exist (PDE of va is empty) and !(prot&1), return NULL
  // 权限不足
  else if (!(pde->present & 1) && !(prot & 1))
  {
    // panic("vm_walkpte: pde is not present and !prot&1");
    return NULL;
  }
  // 存在PDE，返回指向对应PTE的指针。有可能修改prot
  else{
    if (prot != 0)
    {
      pde->val |= prot;
    }
    pt = PDE2PT(*pde); // 根据PDE找页表PT的地址
    // 返回指向对应PTE的指针,不用再设置PTE
    int pt_index = ADDR2TBL(va); // 计算“页表号”PTE
    PTE *pte = &(pt->pte[pt_index]); // 找到对应的页表项PTE
    
    assert((prot & ~7) == 0);
    return pte;
  }
}

void *vm_walk(PD *pgdir, size_t va, int prot) {
  // Lab1-4: translate va to pa  
  PTE *pte = vm_walkpte(pgdir, va, prot);
  // if prot&1 and prot voilation ((pte->val & prot & 7) != prot), call vm_pgfault
  if ((prot & 1) && (pte->val & prot & 7) != prot)
  {
    vm_pgfault(va, 0);
  }
  assert(va>=PHY_MEM);
  // if va is not mapped and !(prot&1), return NULL 没找到映射好的物理地址
  if ((int32_t)PTE2PG(*pte) == 0 && !(prot&1))
  {
    // panic("vm_walk: pte is not mapped and !prot&1");
    return NULL;
  }

  // @lar: if va is mapped, return pa
  void *page = (void*)PTE2PG(*pte);
  void *pa = (void*)((uint32_t)page | ADDR2OFF(va));
  return pa;
  
}

void vm_map(PD *pgdir, size_t va, size_t len, int prot) {
  // Lab1-4: map [PAGE_DOWN(va), PAGE_UP(va+len)) at pgdir, with prot
  // if have already mapped pages, just let pte->prot |= prot
  assert(prot & PTE_P);
  assert((prot & ~7) == 0);
  size_t start = PAGE_DOWN(va);
  size_t end = PAGE_UP(va + len);
  assert(start >= PHY_MEM);
  assert(end >= start);
  // 添加虚拟地址[PAGE_DOWN(va), PAGE_UP(va+len))这一范围的映射
  // 对应物理页通过调用kalloc分配，要求设置这些页的权限为prot。
  for (size_t va = start; va < end; va += PGSIZE)
  {
    PTE *pte = vm_walkpte(pgdir, va, prot);
    // pde权限不足时，pte为NULL
    if (pte == NULL)
    {
      panic("vm_map: pte is NULL");
    }
    // pde权限足，但pte未mapped pages
    else if ((pte->val & PTE_P) == 0)
    {
      void *page = kalloc();
      if (page == NULL)
      {
        panic("vm_map: kalloc failed");
      }
      // make虚拟地址的pte，指向物理地址的page
      pte->val = MAKE_PTE(page, prot);
    }
    // pde权限足，pte已mapped pages
    else
    {
      pte->val |= prot;
    }
  }
}

void vm_unmap(PD *pgdir, size_t va, size_t len) {

  assert(ADDR2OFF(va) == 0);
  assert(ADDR2OFF(len) == 0);
  // Lab1-4: unmap and free [va, va+len) at pgdir
  size_t start = PAGE_DOWN(va);
  size_t end = PAGE_UP(va + len);
  for (size_t va = start; va < end; va += PGSIZE)
  {
    PTE *pte = vm_walkpte(pgdir, va, 0);
    if (pte == NULL)
    {
      continue;
    }
    // 如果PTE_P位为1，说明这个PTE指向的物理页已经被映射了，那么就释放这个物理页
    if (pte->val & PTE_P)
    {
      kfree((void*)PTE2PG(*pte));
      pte->present = 0;
    }
  }

  // 冲刷TLB
  if(pgdir == vm_curr()){
    set_cr3(vm_curr());
  }
  // you can just do nothing :)
}

// 复制当前的虚拟地址空间到pgdir这个页目录里。
void vm_copycurr(PD *pgdir) {
  // Lab2-2: copy memory mapped in curr pd to pgdir
  // 遍历[PHY_MEM, USR_MEM)范围内的虚拟页，用vm_walkpte尝试在当前页目录中找这个虚拟页对应的PTE，
  // 如果PTE存在且有效的话（即这个虚拟地址在当前页目录中存在映射），代表这一页需要复制并在pgdir中添加映射
  for(size_t v_addr = PAGE_DOWN(PHY_MEM); v_addr < USR_MEM; v_addr += PGSIZE){
    PTE *pte = vm_walkpte(vm_curr(), v_addr, 0);
    if(pte != NULL && pte->val & PTE_P){
      // 复制的时候，首先调用vm_map添加这一虚拟页在pgdir中的映射（注意这一页在当前页目录中的权限和在pgdir中的权限应该一致），
      // 然后用vm_walk或vm_walkpte查出其在pgdir中映射到的物理页，
      // 接着调用memcpy把原来虚拟页的内容复制到这一新物理页即可
      vm_map(pgdir, v_addr, PGSIZE, pte->val & 7);
      void *pa = vm_walk(vm_curr(), v_addr, 0);
      void *new_pa = vm_walk(pgdir, v_addr, 0);
      memcpy(new_pa, pa, PGSIZE);
    }
  }
  // // TODO();
}

void vm_pgfault(size_t va, int errcode) {
  printf("pagefault @ 0x%p, errcode = %d\n", va, errcode);
  panic("pgfault");
}
