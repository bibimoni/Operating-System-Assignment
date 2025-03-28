/*
 * Copyright (C) 2025 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* Sierra release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */

// #ifdef MM_PAGING
/*
 * System Library
 * Memory Module Library libmem.c
 */

#include "string.h"
#include "mm.h"
#include "syscall.h"
#include "libmem.h"
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

static pthread_mutex_t mmvm_lock = PTHREAD_MUTEX_INITIALIZER;

/*enlist_vm_freerg_list - add new rg to freerg_list
 *@mm: memory region
 *@rg_elmt: new region
 *
 */
int enlist_vm_freerg_list(struct mm_struct *mm, struct vm_rg_struct *rg_elmt)
{
  struct vm_rg_struct *rg_node = mm->mmap->vm_freerg_list;

  if (rg_elmt->rg_start >= rg_elmt->rg_end)
    return -1;

  if (rg_node != NULL)
    rg_elmt->rg_next = rg_node;

  /* Enlist the new region */
  mm->mmap->vm_freerg_list = rg_elmt;

  return 0;
}

/*get_symrg_byid - get mem region by region ID
 *@mm: memory region
 *@rgid: region ID act as symbol index of variable
 *
 */
struct vm_rg_struct *get_symrg_byid(struct mm_struct *mm, int rgid)
{
  if (rgid < 0 || rgid > PAGING_MAX_SYMTBL_SZ)
    return NULL;

  return &mm->symrgtbl[rgid];
}

/*__alloc - allocate a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *@alloc_addr: address of allocated memory region
 *
 */
int __alloc(struct pcb_t *caller, int vmaid, int rgid, int size, int *alloc_addr)
{
  /*Allocate at the toproof */
  struct vm_rg_struct rgnode;

  /* TODO: commit the vmaid */
  // rgnode.vmaid

  if (get_free_vmrg_area(caller, vmaid, size, &rgnode) == 0)
  {
    pthread_mutex_lock(&mmvm_lock);
    caller->mm->symrgtbl[rgid].rg_start = rgnode.rg_start;
    caller->mm->symrgtbl[rgid].rg_end = rgnode.rg_end;

    *alloc_addr = rgnode.rg_start;

    pthread_mutex_unlock(&mmvm_lock);
    return 0;
  }

  /* TODO get_free_vmrg_area FAILED handle the region management (Fig.6)*/

  /* TODO retrive current vma if needed, current comment out due to compiler redundant warning*/
  /*Attempt to increate limit to get space */
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

  int inc_sz = PAGING_PAGE_ALIGNSZ(size);
  int inc_limit_ret;

  /* TODO retrive old_sbrk if needed, current comment out due to compiler redundant warning*/
  int old_sbrk = cur_vma->sbrk;

  /* TODO INCREASE THE LIMIT as inovking systemcall
   * sys_memap with SYSMEM_INC_OP
   */
  struct sc_regs regs;
  regs.a1 = SYSMEM_INC_OP;
  regs.a2 = vmaid;
  regs.a3 = inc_sz;

  /* SYSCALL 17 sys_memmap */
  inc_limit_ret = syscall(caller, 17, &regs);
  if (inc_limit_ret != 0)
    return -1;
  /* TODO: commit the limit increment */
  cur_vma->sbrk = old_sbrk + inc_sz;
  /* TODO: commit the allocation address
  // *alloc_addr = ...
  */
  pthread_mutex_lock(&mmvm_lock);
  caller->mm->symrgtbl[rgid].rg_start = old_sbrk;
  caller->mm->symrgtbl[rgid].rg_end = old_sbrk + inc_sz;
  *alloc_addr = old_sbrk;
  pthread_mutex_unlock(&mmvm_lock);
  return 0;
}

/*__free - remove a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *
 */
int __free(struct pcb_t *caller, int vmaid, int rgid)
{
  struct vm_rg_struct rgnode;
  struct mm_struct *mm = caller->mm;
  // Dummy initialization for avoding compiler dummay warning
  // in incompleted TODO code rgnode will overwrite through implementing
  // the manipulation of rgid later

  if (rgid < 0 || rgid > PAGING_MAX_SYMTBL_SZ)
    return -1;

  /* TODO: Manage the collect freed region to freerg_list */
  rgnode.rg_start = mm->symrgtbl[rgid].rg_start;
  rgnode.rg_end = mm->symrgtbl[rgid].rg_end;
  if (rgnode.rg_start == -1 || rgnode.rg_end == -1)
    return -1;

  pthread_mutex_lock(&mmvm_lock); // Lock trước khi cập nhật

  mm->symrgtbl[rgid].rg_start = -1;
  mm->symrgtbl[rgid].rg_end = -1;

  pthread_mutex_unlock(&mmvm_lock); // Unlock sau khi cập nhật

  /*enlist the obsoleted memory region */
  enlist_vm_freerg_list(mm, &rgnode);

  return 0;
}

/*liballoc - PAGING-based allocate a region memory
 *@proc:  Process executing the instruction
 *@size: allocated size
 *@reg_index: memory region ID (used to identify variable in symbole table)
 */
int liballoc(struct pcb_t *proc, uint32_t size, uint32_t reg_index)
{
  /* TODO Implement allocation on vm area 0 */
  int addr;

  /* By default using vmaid = 0 */
  return __alloc(proc, 0, reg_index, size, &addr);
}

/*libfree - PAGING-based free a region memory
 *@proc: Process executing the instruction
 *@size: allocated size
 *@reg_index: memory region ID (used to identify variable in symbole table)
 */

int libfree(struct pcb_t *proc, uint32_t reg_index)
{
  /* TODO Implement free region */

  /* By default using vmaid = 0 */
  return __free(proc, 0, reg_index);
}

/*pg_getpage - get the page in ram
 *@mm: memory region
 *@pagenum: PGN
 *@framenum: return FPN
 *@caller: caller
 *
 */
int pg_getpage(struct mm_struct *mm, int pgn, int *fpn, struct pcb_t *caller)
{
  uint32_t pte = mm->pgd[pgn];

  if (!PAGING_PAGE_PRESENT(pte))
  { /* Page is not online, make it actively living */
    if (mm->fifo_pgn == NULL)
    { // Chưa có trang nào
      if (MEMPHY_get_freefp(caller->mram, fpn) < 0)
      {
        printf("No free frame in RAM\n");
        return -1;
      }
      pte_set_fpn(&mm->pgd[pgn], *fpn);
      SETBIT(mm->pgd[pgn], PAGING_PTE_PRESENT_MASK);
      enlist_pgn_node(&mm->fifo_pgn, pgn);
    }
    else
    {
      int vicpgn, swpfpn, vicfpn;
      uint32_t vicpte;

      /*  Tìm victim page */
      find_victim_page(caller->mm, &vicpgn);
      vicpte = mm->pgd[vicpgn];    // Lấy PTE của victim page
      vicfpn = PAGING_FPN(vicpte); // Frame chứa victim page

      /*  Lấy frame trống trong swap */
      if (MEMPHY_get_freefp(caller->active_mswp, &swpfpn) < 0)
      {
        printf("ERROR: Không còn frame trống trong swap!\n");
        return -1;
      }

      /*  Swap victim page ra swap memory */
      struct sc_regs regs1;
      regs1.a1 = SYSMEM_SWP_OP;
      regs1.a2 = vicfpn;
      regs1.a3 = swpfpn;
      syscall(caller, 17, &regs1);

      /* Cập nhật bảng trang (PTE) cho victim page */
      pte_set_swap(&mm->pgd[vicpgn], 0, swpfpn); // Đánh dấu trang bị swap ra ngoài

      /*  Swap trang cần truy cập vào RAM */
      int tgtfpn = PAGING_PTE_SWP(pte); // Frame chứa trang trong swap
      struct sc_regs regs2;
      regs2.a1 = SYSMEM_SWP_OP;
      regs2.a2 = tgtfpn;
      regs2.a3 = vicfpn;
      syscall(caller, 17, &regs2);

      /*  Cập nhật bảng trang cho trang mới */
      pte_set_fpn(&mm->pgd[pgn], vicfpn);            // Gán frame mới cho trang
      SETBIT(mm->pgd[pgn], PAGING_PTE_PRESENT_MASK); // Đánh dấu trang đã có trong RAM

      /*  Thêm trang vào hàng đợi FIFO */
      enlist_pgn_node(&caller->mm->fifo_pgn, pgn);
    }
  }

  *fpn = PAGING_FPN(mm->pgd[pgn]); // Lấy frame chứa trang

  return 0;
}

/*pg_getval - read value at given offset
 *@mm: memory region
 *@addr: virtual address to acess
 *@value: value
 *
 */
int pg_getval(struct mm_struct *mm, int addr, BYTE *data, struct pcb_t *caller)
{
  int pgn = PAGING_PGN(addr);
  int off = PAGING_OFFST(addr);
  int fpn;

  /* Get the page to MEMRAM, swap from MEMSWAP if needed */
  if (pg_getpage(mm, pgn, &fpn, caller) != 0)
    return -1; /* invalid page access */

  /* TODO
   *  MEMPHY_read(caller->mram, phyaddr, data);
   *  MEMPHY READ
   *  SYSCALL 17 sys_memmap with SYSMEM_IO_READ
   */
  int phyaddr = fpn * PAGING_PAGESZ + off;
  struct sc_regs regs;
  regs.a1 = SYSMEM_IO_READ;
  regs.a2 = phyaddr;
  regs.a3 = (uint32_t)(*data);

  /* SYSCALL 17 sys_memmap */
  syscall(caller, 17, &regs);
  // Update data
  *data = (BYTE)regs.a3;

  return 0;
}

/*pg_setval - write value to given offset
 *@mm: memory region
 *@addr: virtual address to acess
 *@value: value
 *
 */
int pg_setval(struct mm_struct *mm, int addr, BYTE value, struct pcb_t *caller)
{
  int pgn = PAGING_PGN(addr);
  int off = PAGING_OFFST(addr);
  int fpn;

  /* Get the page to MEMRAM, swap from MEMSWAP if needed */
  if (pg_getpage(mm, pgn, &fpn, caller) != 0)
    return -1; /* invalid page access */

  /* TODO
   *  MEMPHY_write(caller->mram, phyaddr, value);
   *  MEMPHY WRITE
   *  SYSCALL 17 sys_memmap with SYSMEM_IO_WRITE
   */
  int phyaddr = fpn * PAGING_PAGESZ + off;
  struct sc_regs regs;
  regs.a1 = SYSMEM_IO_WRITE;
  regs.a2 = phyaddr;
  regs.a3 = (uint32_t)value;

  /* SYSCALL 17 sys_memmap */
  printf("Writing to phyaddr=%d, value=%d\n", phyaddr, value);
  syscall(caller, 17, &regs);
  return 0;
}

/*__read - read value in region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@offset: offset to acess in memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *
 */
int __read(struct pcb_t *caller, int vmaid, int rgid, int offset, BYTE *data)
{
  struct vm_rg_struct *currg = get_symrg_byid(caller->mm, rgid);
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

  if (currg == NULL || cur_vma == NULL) /* Invalid memory identify */
    return -1;

  pg_getval(caller->mm, currg->rg_start + offset, data, caller);

  return 0;
}

/*libread - PAGING-based read a region memory */
int libread(
    struct pcb_t *proc, // Process executing the instruction
    uint32_t source,    // Index of source register
    uint32_t offset,    // Source address = [source] + [offset]
    uint32_t *destination)
{
  BYTE data;
  int val = __read(proc, 0, source, offset, &data);

  /* TODO update result of reading action*/
  // destination
#ifdef IODUMP
  // printf("read region=%d offset=%d value=%d\n", source, offset, data);
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1); // print max TBL
#endif
  MEMPHY_dump(proc->mram);
#endif
  *destination = data;
  return val;
}

/*__write - write a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@offset: offset to acess in memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *
 */
int __write(struct pcb_t *caller, int vmaid, int rgid, int offset, BYTE value)
{
  struct vm_rg_struct *currg = get_symrg_byid(caller->mm, rgid);
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

  if (currg == NULL || cur_vma == NULL) /* Invalid memory identify */
    return -1;

  pg_setval(caller->mm, currg->rg_start + offset, value, caller);

  return 0;
}

/*libwrite - PAGING-based write a region memory */
int libwrite(
    struct pcb_t *proc,   // Process executing the instruction
    BYTE data,            // Data to be wrttien into memory
    uint32_t destination, // Index of destination register
    uint32_t offset)
{
#ifdef IODUMP
  printf("write region=%d offset=%d value=%d\n", destination, offset, data);
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1); // print max TBL
#endif
  MEMPHY_dump(proc->mram);
#endif

  return __write(proc, 0, destination, offset, data);
}

/*free_pcb_memphy - collect all memphy of pcb
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@incpgnum: number of page
 */
int free_pcb_memph(struct pcb_t *caller)
{
  int pagenum, fpn;
  uint32_t pte;

  for (pagenum = 0; pagenum < PAGING_MAX_PGN; pagenum++)
  {
    pte = caller->mm->pgd[pagenum];

    if (!PAGING_PAGE_PRESENT(pte))
    {
      fpn = PAGING_PTE_FPN(pte);
      MEMPHY_put_freefp(caller->mram, fpn);
    }
    else
    {
      fpn = PAGING_PTE_SWP(pte);
      MEMPHY_put_freefp(caller->active_mswp, fpn);
    }
  }

  return 0;
}

/*find_victim_page - find victim page
 *@caller: caller
 *@pgn: return page number
 *
 */
int find_victim_page(struct mm_struct *mm, int *retpgn)
{
  if (mm->fifo_pgn == NULL)
  {
    return -1; // Không có trang nào để thay thế
  }

  struct pgn_t *pg = mm->fifo_pgn; // Lấy trang đầu tiên (FIFO)

  *retpgn = pg->pgn; // Trả về số trang bị chọn

  mm->fifo_pgn = pg->pg_next; // Cập nhật FIFO Queue

  free(pg); // Giải phóng bộ nhớ của trang nạn nhân

  return 0;
}

/*get_free_vmrg_area - get a free vm region
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@size: allocated size
 *
 */
int get_free_vmrg_area(struct pcb_t *caller, int vmaid, int size, struct vm_rg_struct *newrg)
{
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
  if (cur_vma == NULL)
  {
    return -1; // Không tìm thấy vùng nhớ ảo
  }

  struct vm_rg_struct *prev = NULL;
  struct vm_rg_struct *rgit = cur_vma->vm_freerg_list;

  while (rgit != NULL)
  {
    int available_size = rgit->rg_end - rgit->rg_start + 1;

    if (available_size >= size)
    { // Tìm thấy vùng nhớ trống đủ lớn
      newrg->rg_start = rgit->rg_start;
      newrg->rg_end = newrg->rg_start + size - 1;

      // Cập nhật lại vùng nhớ trống còn lại
      if (available_size > size)
      {
        rgit->rg_start += size; // Dịch start lên
      }
      else
      { // Xóa vùng này khỏi danh sách nếu vừa khít
        if (prev == NULL)
        {
          cur_vma->vm_freerg_list = rgit->rg_next;
        }
        else
        {
          prev->rg_next = rgit->rg_next;
        }
        free(rgit);
      }
      return 0; // Thành công
    }

    prev = rgit;
    rgit = rgit->rg_next;
  }

  return -1; // Không tìm thấy vùng nhớ đủ lớn
}

// #endif
