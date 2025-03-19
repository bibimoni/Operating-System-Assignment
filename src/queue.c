#include <stdio.h>
#include <stdlib.h>
#include "queue.h"

int empty(struct queue_t * q) {
        if (q == NULL) return 1;
	return (q->size == 0);
}

void enqueue(struct queue_t * q, struct pcb_t * proc) {
        /* TODO: put a new process to queue [q] */
        if (proc == NULL || q == NULL || q->size == MAX_QUEUE_SIZE) {
                return;
        }
        for (int i = 0; i < MAX_QUEUE_SIZE; i++) {
                if (q->proc[i] == NULL) {
                        q->proc[i] = proc;
                        return;
                }
        }
}

struct pcb_t * dequeue(struct queue_t * q) {
        /* TODO: return a pcb whose prioprity is the highest
         * in the queue [q] and remember to remove it from q
         * */
        if (q == NULL || q->size == 0) {
                return NULL;
        }
        struct pcb_t * ans = NULL;
        uint32_t current_priority = 0;
        uint32_t index = 0;
        for (int i = 0; i < MAX_QUEUE_SIZE; i++) {
                if (q->proc[i] == NULL) {
                        continue;
                }
                if (ans == NULL || (ans != NULL && current_priority > q->proc[i]->priority)) {
                        ans = q->proc[i];
                        index = i;
                }
                current_priority = ans->priority;
        }
        q->proc[index] = NULL;
        q->size -= 1;
        return ans;
}

