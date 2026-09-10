#ifndef SEMAPHORE_H
#define SEMAPHORE_H

typedef struct {
    volatile int count;
} semaphore_t;

void sem_init(semaphore_t *s, int initial_count);
void sem_wait(semaphore_t *s);     /* P() -- decrement, block if 0 */
void sem_signal(semaphore_t *s);   /* V() -- increment, wake a waiter */

#endif
