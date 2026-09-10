#include "../include/types.h"
#include "process.h"
#include "thread.h"

/* Every thread's fake interrupt frame points its EIP here, not at the
 * user's function directly -- iretd can't pass an argument, so this
 * "trampoline" reads fn/arg off the PCB (already current_process by
 * the time this runs) and calls the real function normally. */
static void thread_trampoline(void) {
    void (*fn)(void *) = current_process->thread_fn;
    void  *arg         = current_process->thread_arg;
    fn(arg);
    process_exit();
}

pcb_t *thread_create(void (*fn)(void *), void *arg) {
    pcb_t *p = process_alloc((uint32_t)thread_trampoline);
    if (p) {
        p->thread_fn  = fn;
        p->thread_arg = arg;
    }
    return p;
}
