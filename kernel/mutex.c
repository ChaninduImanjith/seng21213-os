#include "mutex.h"

void mutex_init(mutex_t *m) {
    m->locked = 0;
}

/* Spinlock, correct on a single CPU: cli/sti make the "is it free? ->
 * take it" check atomic w.r.t. the timer interrupt, so two processes
 * can never both believe they got the lock. If it's held, hlt with
 * interrupts re-enabled just waits for the next tick (which may
 * switch to the process that will eventually unlock it). */
void mutex_lock(mutex_t *m) {
    for (;;) {
        __asm__ __volatile__("cli");
        if (!m->locked) {
            m->locked = 1;
            __asm__ __volatile__("sti");
            return;
        }
        __asm__ __volatile__("sti");
        __asm__ __volatile__("hlt");
    }
}

void mutex_unlock(mutex_t *m) {
    __asm__ __volatile__("cli");
    m->locked = 0;
    __asm__ __volatile__("sti");
}
