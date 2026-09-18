#include "rwlock.h"

void rwlock_init(rwlock_t *rw) {
    rw->reader_count  = 0;
    rw->writer_active = 0;
    rw->writer_waiting = 0;
}

/* Concurrent readers, exclusive writer, cli/sti spinlock pattern
 * throughout (same reasoning as mutex.c). Extension: a NEW reader is
 * refused the moment a writer starts WAITING (not just once one is
 * active) -- otherwise a steady stream of readers could keep
 * reader_count above zero forever and the writer would never see it
 * reach 0. Readers already in when the writer arrives are still let
 * through to finish normally. */
void rwlock_read_lock(rwlock_t *rw) {
    for (;;) {
        __asm__ __volatile__("cli");
        if (!rw->writer_active && !rw->writer_waiting) {
            rw->reader_count++;
            __asm__ __volatile__("sti");
            return;
        }
        __asm__ __volatile__("sti");
        __asm__ __volatile__("hlt");
    }
}

void rwlock_read_unlock(rwlock_t *rw) {
    __asm__ __volatile__("cli");
    rw->reader_count--;
    __asm__ __volatile__("sti");
}

void rwlock_write_lock(rwlock_t *rw) {
    __asm__ __volatile__("cli");
    rw->writer_waiting = 1;   /* immediately blocks new readers */
    __asm__ __volatile__("sti");

    for (;;) {
        __asm__ __volatile__("cli");
        if (!rw->writer_active && rw->reader_count == 0) {
            rw->writer_active = 1;
            rw->writer_waiting = 0;
            __asm__ __volatile__("sti");
            return;
        }
        __asm__ __volatile__("sti");
        __asm__ __volatile__("hlt");
    }
}

void rwlock_write_unlock(rwlock_t *rw) {
    __asm__ __volatile__("cli");
    rw->writer_active = 0;
    __asm__ __volatile__("sti");
}

int rwlock_active_readers(rwlock_t *rw) {
    return rw->reader_count;
}
