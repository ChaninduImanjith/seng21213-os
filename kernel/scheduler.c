#include "../include/types.h"
#include "../include/io.h"
#include "process.h"
#include "idt.h"
#include "scheduler.h"

extern void irq0_handler(void);

#define PIC1_COMMAND 0x20
#define PIC_EOI      0x20

static uint32_t tick_count = 0;

uint32_t scheduler_ticks(void) {
    return tick_count;
}

void scheduler_init(void) {
    idt_init();
    pic_remap();
    pit_init(100);                              /* 100 Hz -> 10ms time slice */
    idt_set_gate(32, (uint32_t)irq0_handler, 0x08, 0x8E);
    __asm__ __volatile__("sti");                /* enable interrupts */
}

/* Called from isr.asm on every IRQ0. old_esp is where the interrupted
 * process's registers were just pushed to. Returns the esp to switch to. */
uint32_t scheduler_switch(uint32_t old_esp) {
    tick_count++;

    if (current_process) {
        current_process->esp = old_esp;

        /* Extension: fork(). old_esp is the parent's just-saved, VALID
         * live stack pointer -- this is the only place it's safe to
         * duplicate the stack from. */
        if (current_process->fork_requested) {
            process_do_fork(old_esp);
        }

        /* Only requeue if it was actually still runnable. A process
         * that just called sleep_ms() set its own state to BLOCKED
         * before triggering this switch -- leave it OUT of the ready
         * queue; process_wake_ready() puts it back once its time is up. */
        if (current_process->state == RUNNING) {
            /* MLFQ: using the FULL time slice (a normal preemption,
             * not a voluntary sleep/block) is CPU-bound behaviour --
             * demote it one level. */
            if (current_process->priority < MLFQ_LEVELS - 1) {
                current_process->priority++;
            }
            process_requeue(current_process);
        }
    }

    process_wake_ready(tick_count);

    /* MLFQ: anti-starvation. Every 500 ticks (5s at 100Hz), reset
     * everyone back to the top priority so a long-running low-priority
     * job can never be starved out forever by a stream of new arrivals. */
    if (tick_count % 500 == 0) {
        process_boost_all();
    }

    pcb_t *next = process_next_ready();
    if (next) {
        current_process = next;
        current_process->state = RUNNING;
    }
    /* if nothing else is ready, keep running whatever current_process is */

    outb(PIC1_COMMAND, PIC_EOI);

    return current_process ? current_process->esp : old_esp;
}

/* Extension: sleep(ms). Marks the CALLING process BLOCKED with a wake
 * time in ticks (100Hz -> 10ms/tick), then forces an immediate context
 * switch via a software interrupt so it stops running right away
 * instead of waiting out its current time slice. */
void sleep_ms(uint32_t ms) {
    if (!current_process) return;
    uint32_t ticks_to_wait = ms / 10;
    if (ticks_to_wait == 0) ticks_to_wait = 1;
    current_process->wake_tick = tick_count + ticks_to_wait;
    current_process->state = BLOCKED;
    __asm__ __volatile__("int $32");
}
