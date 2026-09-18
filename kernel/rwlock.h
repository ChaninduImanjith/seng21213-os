#ifndef RWLOCK_H
#define RWLOCK_H

typedef struct {
    volatile int reader_count;
    volatile int writer_active;
    volatile int writer_waiting;   /* extension: prevents writer starvation */
} rwlock_t;

void rwlock_init(rwlock_t *rw);
void rwlock_read_lock(rwlock_t *rw);
void rwlock_read_unlock(rwlock_t *rw);
void rwlock_write_lock(rwlock_t *rw);
void rwlock_write_unlock(rwlock_t *rw);
int  rwlock_active_readers(rwlock_t *rw);   /* for demo/debug: current reader count */

#endif
