#include "../include/types.h"
#include "process.h"

#define KERNEL_CS   0x08
#define EFLAGS_IF   0x202

static pcb_t process_table[MAX_PROCESSES];
static uint32_t next_pid = 1;

/* Extension: MLFQ -- one ready queue PER priority level instead of a
 * single list. Level 0 is always drained first. */
static pcb_t *ready_head[MLFQ_LEVELS];
static pcb_t *ready_tail[MLFQ_LEVELS];

pcb_t *current_process = 0;

static void enqueue_at(pcb_t *p, int level) {
    if (level < 0) level = 0;
    if (level >= MLFQ_LEVELS) level = MLFQ_LEVELS - 1;
    p->priority = level;
    p->next = 0;
    if (ready_tail[level]) {
        ready_tail[level]->next = p;
    } else {
        ready_head[level] = p;
    }
    ready_tail[level] = p;
}

static pcb_t *dequeue_highest(void) {
    int level;
    for (level = 0; level < MLFQ_LEVELS; level++) {
        if (ready_head[level]) {
            pcb_t *p = ready_head[level];
            ready_head[level] = p->next;
            if (!ready_head[level]) ready_tail[level] = 0;
            p->next = 0;
            return p;
        }
    }
    return 0;
}

void process_init(void) {
    int i;
    for (i = 0; i < MAX_PROCESSES; i++) {
        process_table[i].state = TERMINATED;
    }
    for (i = 0; i < MLFQ_LEVELS; i++) {
        ready_head[i] = 0;
        ready_tail[i] = 0;
    }
    current_process = 0;
    next_pid = 1;
}

pcb_t *process_alloc(uint32_t entry_eip) {
    pcb_t *p = 0;
    int i;
    for (i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].state == TERMINATED) {
            p = &process_table[i];
            break;
        }
    }
    if (!p) return 0;

    p->pid        = next_pid++;
    p->state      = READY;
    p->eip        = entry_eip;
    p->next       = 0;
    p->thread_fn  = 0;
    p->thread_arg = 0;

    uint32_t *sp = &p->stack[STACK_SIZE / 4];

    *(--sp) = EFLAGS_IF;
    *(--sp) = KERNEL_CS;
    *(--sp) = entry_eip;
    *(--sp) = 0;
    *(--sp) = 0;
    *(--sp) = 0;
    *(--sp) = 0;
    *(--sp) = 0;
    *(--sp) = 0;
    *(--sp) = 0;
    *(--sp) = 0;

    p->esp = (uint32_t)sp;

    enqueue_at(p, 0);   /* MLFQ: everyone starts at the highest priority */
    return p;
}

pcb_t *process_create(void (*entry)(void)) {
    return process_alloc((uint32_t)entry);
}

pcb_t *process_next_ready(void) {
    return dequeue_highest();
}

void process_requeue(pcb_t *p) {
    if (p->state != TERMINATED) {
        p->state = READY;
        enqueue_at(p, p->priority);
    }
}

pcb_t *process_get(int index) {
    if (index < 0 || index >= MAX_PROCESSES) return 0;
    if (process_table[index].state == TERMINATED) return 0;
    return &process_table[index];
}

void process_exit(void) {
    if (current_process) {
        current_process->state = TERMINATED;
    }
    for (;;) { __asm__ __volatile__("hlt"); }
}

void process_wake_ready(uint32_t now) {
    int i;
    for (i = 0; i < MAX_PROCESSES; i++) {
        pcb_t *p = &process_table[i];
        if (p->state == BLOCKED && now >= p->wake_tick) {
            /* MLFQ: waking from a voluntary block is I/O-bound
             * behaviour -- reward it with a priority boost. */
            if (p->priority > 0) p->priority--;
            process_requeue(p);
        }
    }
}

/* Extension: MLFQ anti-starvation. Reset everyone's priority to 0, and
 * physically move anything sitting in a lower-priority queue up to
 * queue 0 right now (not just whenever it's next (re)queued). */
void process_boost_all(void) {
    int i, level;
    for (i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].state != TERMINATED) process_table[i].priority = 0;
    }
    for (level = 1; level < MLFQ_LEVELS; level++) {
        pcb_t *p = ready_head[level];
        ready_head[level] = 0;
        ready_tail[level] = 0;
        while (p) {
            pcb_t *next = p->next;
            enqueue_at(p, 0);
            p = next;
        }
    }
}

/* Extension: fork(). Called from inside scheduler_switch(), where
 * parent_esp is the parent's just-saved, VALID live stack pointer
 * (current_process->esp is stale while a process is actively running
 * -- this is only correct to read right after a context switch). */
void process_do_fork(uint32_t parent_esp) {
    if (!current_process || !current_process->fork_requested) return;
    current_process->fork_requested = false;

    pcb_t *child = 0;
    int i;
    for (i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].state == TERMINATED) { child = &process_table[i]; break; }
    }
    if (!child) {
        current_process->fork_return_value = -1;
        return;
    }

    for (i = 0; i < STACK_SIZE / 4; i++) child->stack[i] = current_process->stack[i];

    child->pid             = next_pid++;
    child->thread_fn       = current_process->thread_fn;
    child->thread_arg      = current_process->thread_arg;
    child->fork_requested  = false;
    child->fork_return_value = 0;
    child->state           = READY;
    child->next            = 0;
    child->priority        = 0;   /* MLFQ: a fresh process starts at the top */

    uint32_t parent_top = (uint32_t)&current_process->stack[STACK_SIZE / 4];
    uint32_t child_top   = (uint32_t)&child->stack[STACK_SIZE / 4];
    uint32_t depth       = parent_top - parent_esp;
    child->esp = child_top - depth;

    process_requeue(child);

    current_process->fork_return_value = (int)child->pid;
}

int fork(void) {
    if (!current_process) return -1;
    current_process->fork_requested = true;
    __asm__ __volatile__("int $32");
    return current_process->fork_return_value;
}

int process_index_of(pcb_t *p) {
    if (!p) return -1;
    return (int)(p - process_table);
}
