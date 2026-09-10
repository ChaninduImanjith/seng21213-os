#include "../include/types.h"
#include "process.h"

#define KERNEL_CS   0x08
#define EFLAGS_IF   0x202

static pcb_t process_table[MAX_PROCESSES];
static uint32_t next_pid = 1;

static pcb_t *ready_head = 0;
static pcb_t *ready_tail = 0;

pcb_t *current_process = 0;

static void enqueue(pcb_t *p) {
    p->next = 0;
    if (ready_tail) {
        ready_tail->next = p;
    } else {
        ready_head = p;
    }
    ready_tail = p;
}

static pcb_t *dequeue(void) {
    if (!ready_head) return 0;
    pcb_t *p = ready_head;
    ready_head = ready_head->next;
    if (!ready_head) ready_tail = 0;
    p->next = 0;
    return p;
}

void process_init(void) {
    int i;
    for (i = 0; i < MAX_PROCESSES; i++) {
        process_table[i].state = TERMINATED;
    }
    ready_head = 0;
    ready_tail = 0;
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

    *(--sp) = EFLAGS_IF;   /* EFLAGS */
    *(--sp) = KERNEL_CS;   /* CS     */
    *(--sp) = entry_eip;   /* EIP    */
    *(--sp) = 0;           /* EAX */
    *(--sp) = 0;           /* ECX */
    *(--sp) = 0;           /* EDX */
    *(--sp) = 0;           /* EBX */
    *(--sp) = 0;           /* ESP (dummy) */
    *(--sp) = 0;           /* EBP */
    *(--sp) = 0;           /* ESI */
    *(--sp) = 0;           /* EDI <- esp points here */

    p->esp = (uint32_t)sp;

    enqueue(p);
    return p;
}

pcb_t *process_create(void (*entry)(void)) {
    return process_alloc((uint32_t)entry);
}

pcb_t *process_next_ready(void) {
    return dequeue();
}

void process_requeue(pcb_t *p) {
    if (p->state != TERMINATED) {
        p->state = READY;
        enqueue(p);
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
