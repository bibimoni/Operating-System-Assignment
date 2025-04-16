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
 #include "string.h"
 #include "stdlib.h"
 int __sys_killall(struct pcb_t *caller, struct sc_regs *regs)
 {
     char proc_name[100];
     uint32_t data;
 
     // hardcode for demo only
     uint32_t memrg = regs->a1;
 
     /* TODO: Get name of the target proc */
     // proc_name = libread..
     int i = 0;
     data = 0;
     while (data != (uint32_t)-1)
     {
         libread(caller, memrg, i, &data);
         proc_name[i] = data;
         if (data == -1)
             proc_name[i] = '\0';
         i++;
     }
     printf("The procname retrieved from memregionid %d is \"%s\"\n", memrg, proc_name);
 
     /* TODO: Traverse proclist to terminate the proc
      *       stcmp to check the process match proc_name
      */
 
     // caller->running_list
     int kill_count = 0;
     if (caller->running_list != NULL)
     {
         struct pcb_t *target;
         for (int i = 0; i < caller->running_list->size; i++)
         {
             target = caller->running_list->proc[i];
             if (target && strcmp(target->path, proc_name) == 0)
             {
                 printf("Terminating running process PID=%d named \"%s\"\n", target->pid, target->path);
                 free(target);
                 caller->running_list->proc[i] = NULL;
                 kill_count++;
             }
         }
     }
 // caller->mlq_ready_queue
 #ifdef MLQ_SCHED
     if (caller->mlq_ready_queue != NULL)
     {
         for (int q = 0; q < MAX_QUEUE_SIZE; q++)
         {
             struct queue_t *qptr = &caller->mlq_ready_queue[q];
             for (int i = 0; i < qptr->size; i++)
             {
                 struct pcb_t *target = qptr->proc[i];
                 if (target && strcmp(target->path, proc_name) == 0)
                 {
                     printf("Terminating MLQ ready process PID=%d named \"%s\"\n", target->pid, target->path);
                     free(target);
                     qptr->proc[i] = NULL;
                     kill_count++;
                 }
             }
         }
     }
 #endif
     /* TODO Maching and terminating
      *       all processes with given
      *        name in var proc_name
      */
     if (caller->ready_queue != NULL)
     {
         for (int i = 0; i < caller->ready_queue->size; i++)
         {
             struct pcb_t *target = caller->ready_queue->proc[i];
             if (target && strcmp(target->path, proc_name) == 0)
             {
                 printf("Terminating ready process PID=%d named \"%s\"\n", target->pid, target->path);
                 free(target);
                 caller->ready_queue->proc[i] = NULL;
                 kill_count++;
             }
         }
     }
     return kill_count;
 }
 