#include "syscall.h"

int __sys_xxxhandler(struct pcb_t *caller, struct sc_regs *reg)
{
    /* TODO: implement syscall job */
    printf("The first system call parameter % d\n", reg->a1);
    return 0;
}
