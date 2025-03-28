#include <stdio.h>
#include <stdlib.h>
#include "queue.h"

int empty(struct queue_t *q)
{
        if (q == NULL)
                return 1;
        return (q->size == 0);
}

void enqueue(struct queue_t *q, struct pcb_t *proc)
{
        /* TODO: put a new process to queue [q] */
        if (q == NULL || proc == NULL)
                return;
        q->proc[q->size] = proc;
        q->size++;
}

struct pcb_t *dequeue(struct queue_t *q)
{
        /* TODO: return a pcb whose prioprity is the highest
         * in the queue [q] and remember to remove it from q
         * */
        if (empty(q))
                return NULL;
        struct pcb_t *returnProc = q->proc[0];
        int q_size = q->size;
        for (int i = 0; i < q_size - 1; i++)
        {
                q->proc[i] = q->proc[i + 1];
        }
        q->proc[q_size - 1] = NULL;
        q->size--;
        return returnProc;
}

void remove_from_queue(struct queue_t *q, struct pcb_t *proc)
{
        if (q == NULL || empty(q))
                return;

        int found = -1;
        for (int i = 0; i < q->size; i++)
        {
                if (q->proc[i] == proc)
                {
                        found = i;
                        break;
                }
        }

        if (found == -1)
                return; // Không tìm thấy tiến trình trong hàng đợi

        // Dịch chuyển các phần tử phía sau lên trước một vị trí
        for (int i = found; i < q->size - 1; i++)
        {
                q->proc[i] = q->proc[i + 1];
        }

        q->proc[q->size - 1] = NULL; // Xóa phần tử cuối
        q->size--;                   // Giảm kích thước hàng đợi
}
