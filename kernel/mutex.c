#include "mutex.h"

void mutex_init(mutex_t *m) {
    m->locked = 0;
    m->owner = 0;
    m->owner_saved_priority = 0;
}

/* Spinlock, correct on a single CPU: cli/sti make the "is it free? ->
 * take it" check atomic w.r.t. the timer interrupt. If it's held,
 * hlt with interrupts re-enabled just waits for the next tick.
 *
 * Extension: priority inheritance. Classic priority inversion: a LOW
 * priority process holds the lock, a HIGH priority process wants it
 * and blocks, and a MEDIUM priority process (which never touches this
 * lock at all) keeps winning the CPU over LOW via normal MLFQ
 * scheduling -- so HIGH ends up waiting on MEDIUM indirectly, with no
 * priority relationship between them at all. The fix: while we wait,
 * if we're higher priority than the current owner, boost the owner to
 * our level right now, so it can't be starved out by anyone at a
 * priority in between. */
void mutex_lock(mutex_t *m) {
    for (;;) {
        __asm__ __volatile__("cli");
        if (!m->locked) {
            m->locked = 1;
            m->owner = current_process;
            m->owner_saved_priority = current_process ? current_process->priority : 0;
            __asm__ __volatile__("sti");
            return;
        }

        if (m->owner && current_process && current_process->priority < m->owner->priority) {
            m->owner->priority = current_process->priority;
        }

        __asm__ __volatile__("sti");
        __asm__ __volatile__("hlt");
    }
}

void mutex_unlock(mutex_t *m) {
    __asm__ __volatile__("cli");
    if (m->owner) {
        m->owner->priority = m->owner_saved_priority;   /* undo any inheritance boost */
    }
    m->owner = 0;
    m->locked = 0;
    __asm__ __volatile__("sti");
}
