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
        process_requeue(current_process);
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
