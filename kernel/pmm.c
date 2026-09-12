#include "../include/types.h"
#include "pmm.h"

/* Defined by linker.ld: the address just past the kernel's own image.
 * It has no value of its own -- its ADDRESS *is* the value we want,
 * so we always read it as &kernel_end, never kernel_end. */
extern uint32_t kernel_end;

/* Raw BIOS E820 entry layout, exactly as boot.asm's detect_memory wrote
 * it starting at 0x8004 (24 bytes per entry, count as a word at 0x8000). */
typedef struct {
    uint64_t base;
    uint64_t length;
    uint32_t type;        /* 1 = usable RAM, everything else = reserved */
    uint32_t acpi_attrs;
} __attribute__((packed)) e820_entry_t;

/* We manage the first 32MB of physical RAM (matches -m 32M in the
 * Makefile's QEMUFLAGS) as 4KB frames, 1 bit per frame. */
#define MAX_FRAMES    (32 * 1024 * 1024 / FRAME_SIZE)   /* 8192 frames */
#define BITMAP_WORDS  (MAX_FRAMES / 32)                  /* 256 words = 1KB */

static uint32_t bitmap[BITMAP_WORDS];
static uint32_t total_frames = 0;
static uint32_t free_count   = 0;

static inline void bitmap_set(uint32_t f)   { bitmap[f / 32] |=  (1u << (f % 32)); }
static inline void bitmap_clear(uint32_t f) { bitmap[f / 32] &= ~(1u << (f % 32)); }
static inline int  bitmap_test(uint32_t f)  { return (bitmap[f / 32] >> (f % 32)) & 1; }

void pmm_init(void) {
    uint32_t i;
    uint32_t kend = (uint32_t)&kernel_end;

    /* Start pessimistic: everything used. Only regions the BIOS reports
     * as usable RAM, above kernel_end, get freed below. */
    for (i = 0; i < BITMAP_WORDS; i++) bitmap[i] = 0xFFFFFFFF;
    total_frames = MAX_FRAMES;
    free_count   = 0;

    uint16_t       count   = *(uint16_t *)0x8000;
    e820_entry_t  *entries = (e820_entry_t *)0x8004;

    for (i = 0; i < count; i++) {
        if (entries[i].type != 1) continue;   /* skip reserved/ACPI/bad regions */

        uint64_t start = entries[i].base;
        uint64_t end   = start + entries[i].length;
        if (start > 0xFFFFFFFFULL) continue;   /* ignore memory above 4GB (n/a on 32-bit anyway) */
        if (end   > 0xFFFFFFFFULL) end = 0xFFFFFFFFULL;

        uint32_t s = (uint32_t)start;
        uint32_t e = (uint32_t)end;

        if (s < kend) s = kend;   /* never hand out the kernel's own memory */
        if (s >= e) continue;

        uint32_t addr;
        for (addr = s; addr + FRAME_SIZE <= e; addr += FRAME_SIZE) {
            uint32_t frame = addr / FRAME_SIZE;
            if (frame >= MAX_FRAMES) break;
            if (bitmap_test(frame)) {
                bitmap_clear(frame);
                free_count++;
            }
        }
    }
}

uint32_t pmm_alloc_frame(void) {
    uint32_t i;
    for (i = 0; i < total_frames; i++) {
        if (!bitmap_test(i)) {
            bitmap_set(i);
            free_count--;
            return i * FRAME_SIZE;
        }
    }
    return 0;   /* out of memory -- frame 0 is always reserved, so 0 is a safe "failure" sentinel */
}

void pmm_free_frame(uint32_t phys_addr) {
    uint32_t frame = phys_addr / FRAME_SIZE;
    if (frame >= total_frames) return;
    if (bitmap_test(frame)) {
        bitmap_clear(frame);
        free_count++;
    }
}

uint32_t pmm_total_frames(void) { return total_frames; }
uint32_t pmm_free_frames(void)  { return free_count; }
uint32_t pmm_used_frames(void)  { return total_frames - free_count; }
