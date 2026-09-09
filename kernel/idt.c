#include "../include/types.h"
#include "../include/io.h"
#include "idt.h"

static idt_entry_t idt[IDT_ENTRIES];
static idt_ptr_t   idt_ptr;

void idt_set_gate(uint8_t num, uint32_t base, uint16_t selector, uint8_t flags) {
    idt[num].base_low  = base & 0xFFFF;
    idt[num].base_high = (base >> 16) & 0xFFFF;
    idt[num].selector  = selector;
    idt[num].zero      = 0;
    idt[num].flags     = flags;
}

void idt_init(void) {
    int i;
    for (i = 0; i < IDT_ENTRIES; i++) {
        idt_set_gate(i, 0, 0, 0);
    }

    idt_ptr.limit = (sizeof(idt_entry_t) * IDT_ENTRIES) - 1;
    idt_ptr.base  = (uint32_t)&idt;

    __asm__ __volatile__("lidt (%0)" : : "r"(&idt_ptr));
}

/* Remap PIC: IRQ0-7 -> vectors 32-39, IRQ8-15 -> vectors 40-47 */
void pic_remap(void) {
    outb(0x20, 0x11); io_wait();   /* ICW1: master, edge-triggered, cascade */
    outb(0xA0, 0x11); io_wait();   /* ICW1: slave */

    outb(0x21, 0x20); io_wait();   /* ICW2: master offset -> 32 */
    outb(0xA1, 0x28); io_wait();   /* ICW2: slave offset -> 40 */

    outb(0x21, 0x04); io_wait();   /* ICW3: tell master about slave on IRQ2 */
    outb(0xA1, 0x02); io_wait();   /* ICW3: tell slave its cascade identity */

    outb(0x21, 0x01); io_wait();   /* ICW4: 8086 mode */
    outb(0xA1, 0x01); io_wait();

    outb(0x21, 0x00); io_wait();   /* unmask all IRQs on master */
    outb(0xA1, 0x00); io_wait();   /* unmask all IRQs on slave */
}

/* Program PIT channel 0 to fire at the given frequency (Hz) */
void pit_init(uint32_t frequency_hz) {
    uint32_t divisor = 1193182 / frequency_hz;

    outb(0x43, 0x36);                      /* channel 0, lo/hi byte, square wave */
    outb(0x40, (uint8_t)(divisor & 0xFF));         /* low byte */
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));  /* high byte */
}
