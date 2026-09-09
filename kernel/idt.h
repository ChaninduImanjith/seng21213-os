#ifndef IDT_H
#define IDT_H

#include "../include/types.h"

/* One IDT entry — CPU-defined layout, do not reorder fields */
typedef struct {
    uint16_t base_low;
    uint16_t selector;
    uint8_t  zero;
    uint8_t  flags;
    uint16_t base_high;
} __attribute__((packed)) idt_entry_t;

typedef struct {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed)) idt_ptr_t;

#define IDT_ENTRIES 256

void idt_set_gate(uint8_t num, uint32_t base, uint16_t selector, uint8_t flags);
void idt_init(void);
void pic_remap(void);
void pit_init(uint32_t frequency_hz);

#endif
