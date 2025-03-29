/*
 * Copyright (C) 2025 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* Sierra release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */

#include "common.h"
#include "syscall.h"
#include "stdio.h"
#include "libmem.h"
#include "queue.h"
#include "sched.h"
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
static pthread_mutex_t queue_lock;

int __sys_killall(struct pcb_t *caller, struct sc_regs *regs)
{
    char proc_name[100];
    uint32_t memrg = regs->a1;
    uint32_t data;
    int i = 0;
    do
    {
        if (libread(caller, memrg, i, &data) != 0)
            break;
        proc_name[i] = (char)data;
        i++;
    } while ((BYTE)data != (BYTE)-1 && i < sizeof(proc_name) - 1);
    if (i > 0)
        proc_name[i - 1] = '\0';
    else
        proc_name[0] = '\0';
    printf("The procname retrieved from memregionid %d is \"%s\"\n", memrg, proc_name);

    /* TODO: Traverse proclist to terminate the proc
     *       stcmp to check the process match proc_name
     */
    // caller->running_list
    // caller->mlq_ready_queue

    /* TODO Maching and terminating
     *       all processes with given
     *        name in var proc_name
     */
    pthread_mutex_lock(&queue_lock);

    for (int j = 0; j < caller->running_list->size; j++)
    {
        struct pcb_t *proc = caller->running_list->proc[j];

        const char *filename = strrchr(proc->path, '/');
        if (filename)
            filename++;
        else
            filename = proc->path;
        if (strcmp(filename, proc_name) == 0)
        {
            printf("Terminating running process PID: %d, Name: %s\n", proc->pid, filename);
            remove_from_queue(caller->running_list, proc);
            proc->pc = -1;
        }
    }

#ifdef MLQ_SCHED
    for (int prio = 0; prio < MAX_PRIO; prio++)
    {
        struct queue_t *q = &caller->mlq_ready_queue[prio];

        for (int k = 0; k < q->size; k++)
        {
            struct pcb_t *proc = q->proc[k];

            const char *filename = strrchr(proc->path, '/');
            if (filename)
                filename++;
            else
                filename = proc->path;

            printf("mlq: %s\n", filename);

            if (strcmp(filename, proc_name) == 0)
            {
                printf("Removing process PID: %d, Name: %s from MLQ queue\n", proc->pid, filename);
                remove_from_queue(q, proc);
                proc->pc = -1;
            }
        }
    }
#else
    for (int j = 0; j < caller->ready_queue->size; j++)
    {
        struct pcb_t *proc = caller->ready_queue->proc[j];

        const char *filename = strrchr(proc->path, '/');
        if (filename)
            filename++;
        else
            filename = proc->path;
        if (strcmp(filename, proc_name) == 0)
        {
            printf("Terminating process in ready queue PID: %d, Name: %s\n", proc->pid, filename);
            remove_from_queue(caller->running_list, proc);
            proc->pc = -1;
        }
    }
#endif
    pthread_mutex_unlock(&queue_lock);

    return 0;
}
