#ifndef PROCESS_H
#define PROCESS_H

#include "../include/types.h"

#define MAX_PROCESSES    16
#define STACK_SIZE     4096

typedef enum { READY, RUNNING, BLOCKED, TERMINATED } proc_state_t;

typedef struct pcb {
    uint32_t      pid;
    proc_state_t  state;
    uint32_t      esp;          /* Saved stack pointer */
    uint32_t      eip;          /* Saved instruction pointer */
    uint32_t      stack[STACK_SIZE / 4];
    struct pcb   *next;         /* For linked-list ready queue */

    /* Lecture 10: set only for kernel threads, launched via the
     * thread_trampoline so an argument can be passed in. */
    void        (*thread_fn)(void *);
    void         *thread_arg;

    /* Extension: sleep(ms) -- valid only while state == BLOCKED due to
     * a sleep_ms() call. The scheduler wakes this process once
     * scheduler_ticks() reaches wake_tick. */
    uint32_t      wake_tick;

    /* Extension: fork(). fork_requested is set by fork() right before
     * forcing a context switch; process_do_fork() (called from inside
     * scheduler_switch, where the parent's live esp is available)
     * clears it and does the actual duplication. fork_return_value is
     * what fork() hands back once this PCB resumes -- 0 for a child,
     * the new PID for the parent. */
    bool          fork_requested;
    int           fork_return_value;
} pcb_t;

void   process_init(void);
pcb_t *process_create(void (*entry)(void));
void   process_exit(void);

/* Shared low-level allocator used by both process_create() and
 * thread_create() (Lecture 10) — finds a free PCB slot, builds the
 * fake interrupt frame pointing at entry_eip, and enqueues it. */
pcb_t *process_alloc(uint32_t entry_eip);

extern pcb_t *current_process;

pcb_t *process_next_ready(void);
void   process_requeue(pcb_t *p);
pcb_t *process_get(int index);
void   process_wake_ready(uint32_t now);   /* requeue any BLOCKED process whose wake_tick has passed */

/* Extension: fork(). process_do_fork() is called by scheduler_switch,
 * not directly by user code -- call fork() instead. */
void   process_do_fork(uint32_t parent_esp);
int    fork(void);

#endif
