#include "string.h"
#include "mm.h"
#include "syscall.h"
#include "libmem.h"
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

// Removed unused mmvm_lock
// static pthread_mutex_t mmvm_lock = PTHREAD_MUTEX_INITIALIZER;

int enlist_vm_freerg_list(struct mm_struct *mm, struct vm_rg_struct *rg_elmt) {
    struct vm_rg_struct *rg_node = mm->mmap->vm_freerg_list;

    if (rg_elmt->rg_start >= rg_elmt->rg_end)
        return -1;

    if (rg_node != NULL)
        rg_elmt->rg_next = rg_node;

    mm->mmap->vm_freerg_list = rg_elmt;
    return 0;
}

struct vm_rg_struct *get_symrg_byid(struct mm_struct *mm, int rgid) {
    if (rgid < 0 || rgid > PAGING_MAX_SYMTBL_SZ)
        return NULL;
    return &mm->symrgtbl[rgid];
}

int __alloc(struct pcb_t *caller, int vmaid, int rgid, int size, int *alloc_addr) {
    struct vm_rg_struct rgnode;

    if (rgid < 0 || rgid > PAGING_MAX_SYMTBL_SZ) {
        printf("Process %d alloc error: Invalid region\n", caller->pid);
        return -1;
    } else if (caller->mm->symrgtbl[rgid].rg_start > caller->mm->symrgtbl[rgid].rg_end) {
        printf("Process %d alloc error: Region was alloc before\n", caller->pid);
        return -1;
    }

    if (get_free_vmrg_area(caller, vmaid, size, &rgnode) == 0) {
        caller->mm->symrgtbl[rgid].rg_start = rgnode.rg_start;
        caller->mm->symrgtbl[rgid].rg_end = rgnode.rg_end;
        *alloc_addr = rgnode.rg_start;

        for (int i = rgnode.rg_start / 256; i <= rgnode.rg_end / 256; i++) {
            uint32_t current_pte = caller->mm->pgd[i];
            if (rgnode.rg_end % 256 == 0 && rgnode.rg_end - rgnode.rg_start > 0) {
                break;
            }

            if (!PAGING_PAGE_PRESENT(current_pte)) {
                printf("FREE REG in SWAP \n\n");
                int targetfpn = GETVAL(current_pte, GENMASK(20, 0), 5);
                uint32_t *vicpte = &caller->mm->pgd[find_victim_page(caller->mm, &targetfpn)]; // Fix: Use PTE pointer
                int vicfpn = GETVAL(*vicpte, PAGING_PTE_FPN_MASK, PAGING_PTE_FPN_LOBIT);
                int swpfpn;

                if (MEMPHY_get_freefp(caller->active_mswp, &swpfpn) < 0) {
                    printf("Out of SWAP");
                    return -3000;
                }

                __swap_cp_page(caller->mram, vicfpn, caller->active_mswp, swpfpn);
                pte_set_swap(vicpte, 0, swpfpn);
                __swap_cp_page(caller->active_mswp, targetfpn, caller->mram, vicfpn);
                pte_set_fpn(&caller->mm->pgd[i], vicfpn);
                MEMPHY_put_freefp(caller->active_mswp, targetfpn);
            }
        }
        return 0;
    }

    struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
    int inc_sz = PAGING_PAGE_ALIGNSZ(size);
    int old_sbrk = cur_vma->sbrk;

    inc_vma_limit(caller, vmaid, inc_sz);

    caller->mm->symrgtbl[rgid].rg_start = old_sbrk;
    caller->mm->symrgtbl[rgid].rg_end = old_sbrk + size;
    *alloc_addr = old_sbrk;

    struct vm_rg_struct *rgnode_temp = malloc(sizeof(struct vm_rg_struct));
    rgnode_temp->rg_start = old_sbrk + size;
    rgnode_temp->rg_end = cur_vma->sbrk;
    enlist_vm_freerg_list(caller->mm, rgnode_temp);

    return 0;
}

int __free(struct pcb_t *caller, int vmaid, int rgid) {
    struct vm_rg_struct *rgnode;

    if (rgid < 0 || rgid > PAGING_MAX_SYMTBL_SZ)
        return -1;

    rgnode = get_symrg_byid(caller->mm, rgid);
    if (rgnode->rg_start == rgnode->rg_end) {
        printf("Process %d FREE Error: Region wasn't alloc or was freed before\n", caller->pid);
        return -1;
    }

    struct vm_rg_struct *rgnode_temp = malloc(sizeof(struct vm_rg_struct));
    rgnode_temp->rg_start = rgnode->rg_start;
    rgnode_temp->rg_end = rgnode->rg_end;
    rgnode->rg_start = rgnode->rg_end = 0;

    enlist_vm_freerg_list(caller->mm, rgnode_temp);
    return 0;
}

int liballoc(struct pcb_t *proc, uint32_t size, uint32_t reg_index) {
    int addr;
    return __alloc(proc, 0, reg_index, size, &addr);
}

int libfree(struct pcb_t *proc, uint32_t reg_index) {
    return __free(proc, 0, reg_index);
}

int pg_getpage(struct mm_struct *mm, int pgn, int *fpn, struct pcb_t *caller) {
    uint32_t pte = mm->pgd[pgn];

    if (!PAGING_PAGE_PRESENT(pte)) {
        int fpn_temp = -1;
        if (MEMPHY_get_freefp(caller->mram, &fpn_temp) == 0) {
            int tgtfpn = GETVAL(pte, GENMASK(20, 0), 5); // Fix: Declare tgtfpn here
            __swap_cp_page(caller->active_mswp, tgtfpn, caller->mram, fpn_temp);
            pte_set_fpn(&mm->pgd[pgn], fpn_temp);
        } else {
            int swpfpn, tgtfpn = GETVAL(pte, GENMASK(20, 0), 5); // Fix: Declare tgtfpn here
            uint32_t *vicpte = &mm->pgd[find_victim_page(mm, fpn)]; // Fix: Use PTE pointer

            int vicfpn = GETVAL(*vicpte, PAGING_PTE_FPN_MASK, PAGING_PTE_FPN_LOBIT);

            if (MEMPHY_get_freefp(caller->active_mswp, &swpfpn) < 0) {
                printf("Out of SWAP");
                return -3000;
            }

            __swap_cp_page(caller->mram, vicfpn, caller->active_mswp, swpfpn);
            __swap_cp_page(caller->active_mswp, tgtfpn, caller->mram, vicfpn);
            pte_set_swap(vicpte, 0, swpfpn);
            pte_set_fpn(&mm->pgd[pgn], vicfpn);

            MEMPHY_put_freefp(caller->active_mswp, tgtfpn);
        }
    }
    *fpn = GETVAL(mm->pgd[pgn], PAGING_PTE_FPN_MASK, PAGING_PTE_FPN_LOBIT);
    return 0;
}

int pg_setval(struct mm_struct *mm, int addr, BYTE value, struct pcb_t *caller) {
    int pgn = PAGING_PGN(addr);
    int off = PAGING_OFFST(addr);
    int fpn;

    if (pg_getpage(mm, pgn, &fpn, caller) != 0)
        return -1;

    int phyaddr = (fpn << PAGING_ADDR_FPN_LOBIT) + off;
    MEMPHY_write(caller->mram, phyaddr, value);
    return 0;
}
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
  int phyaddr = (fpn << PAGING_ADDR_FPN_LOBIT) + off;

  MEMPHY_read(caller->mram,phyaddr, data);
  //struct sc_regs regs;
  //regs.a1 = ...
  //regs.a2 = ...
  //regs.a3 = ...

  /* SYSCALL 17 sys_memmap */
  // syscall(caller, 17, &regs);
  // Update data
  // *data = (BYTE)regs.a3;

  return 0;
}

int __read(struct pcb_t *caller, int vmaid, int rgid, int offset, BYTE *data) {
    struct vm_rg_struct *currg = get_symrg_byid(caller->mm, rgid);
    struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

    if (currg == NULL || cur_vma == NULL)
        return -1;

    return pg_getval(caller->mm, currg->rg_start + offset, data, caller); // Fix: Call defined function
}

int libread(struct pcb_t *proc, uint32_t source, uint32_t offset, uint32_t *destination) {
    BYTE data;
    int val = __read(proc, 0, source, offset, &data);
    *destination = (uint32_t)data;
#ifdef IODUMP
    printf("read region=%d offset=%d value=%d\n", source, offset, data);
#ifdef PAGETBL_DUMP
    print_pgtbl(proc, 0, -1);
#endif
    MEMPHY_dump(proc->mram);
#endif
    return val;
}

int __write(struct pcb_t *caller, int vmaid, int rgid, int offset, BYTE value) {
    struct vm_rg_struct *currg = get_symrg_byid(caller->mm, rgid);
    struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

    if (currg == NULL || cur_vma == NULL)
        return -1;

    return pg_setval(caller->mm, currg->rg_start + offset, value, caller);
}

int libwrite(struct pcb_t *proc, BYTE data, uint32_t destination, uint32_t offset) {
#ifdef IODUMP
    printf("write region=%d offset=%d value=%d\n", destination, offset, data);
#ifdef PAGETBL_DUMP
    print_pgtbl(proc, 0, -1);
#endif
    MEMPHY_dump(proc->mram);
#endif
    return __write(proc, 0, destination, offset, data);
}

int free_pcb_memph(struct pcb_t *caller) {
    int pagenum, fpn;
    uint32_t pte;

    for (pagenum = 0; pagenum < PAGING_MAX_PGN; pagenum++) {
        pte = caller->mm->pgd[pagenum];

        if (!PAGING_PAGE_PRESENT(pte)) {
            fpn = PAGING_PTE_FPN(pte);
            MEMPHY_put_freefp(caller->mram, fpn);
        } else {
            fpn = PAGING_PTE_SWP(pte);
            MEMPHY_put_freefp(caller->active_mswp, fpn);
        }
    }
    return 0;
}

int find_victim_page(struct mm_struct *mm, int *retpgn) {
    struct pgn_t *pg = mm->fifo_pgn;

    // Placeholder: Should implement a proper victim selection mechanism
    if (pg != NULL) {
        *retpgn = pg->pgn;
        mm->fifo_pgn = pg->pg_next;
        free(pg);
        return *retpgn; // Return page number for now
    }
    return 0; // Default case, should be improved
}

int get_free_vmrg_area(struct pcb_t *caller, int vmaid, int size, struct vm_rg_struct *newrg) {
    struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
    struct vm_rg_struct *rgit = cur_vma->vm_freerg_list;

    if (rgit == NULL)
        return -1;

    newrg->rg_start = newrg->rg_end = -1;

    while (rgit != NULL) {
        if (rgit->rg_start + size <= rgit->rg_end) {
            newrg->rg_start = rgit->rg_start;
            newrg->rg_end = rgit->rg_start + size;

            if (rgit->rg_start + size < rgit->rg_end) {
                rgit->rg_start = rgit->rg_start + size;
            } else {
                struct vm_rg_struct *nextrg = rgit->rg_next;
                if (nextrg != NULL) {
                    rgit->rg_start = nextrg->rg_start;
                    rgit->rg_end = nextrg->rg_end;
                    rgit->rg_next = nextrg->rg_next;
                    free(nextrg);
                } else {
                    rgit->rg_start = rgit->rg_end;
                    rgit->rg_next = NULL;
                }
            }
            break;
        }
        rgit = rgit->rg_next;
    }
    if (newrg->rg_start == -1)
        return -1;

    return 0;
}
