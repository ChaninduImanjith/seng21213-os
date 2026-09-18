#include "../include/types.h"
#include "multiboot.h"

#define MB_FLAG_MEMORY  (1U << 0)
#define MB_FLAG_MMAP    (1U << 6)

#define E820_BUFFER_COUNT  ((volatile uint16_t *)0x8000)
#define E820_BUFFER_BASE   0x8004U
#define E820_MAX_ENTRIES   128

typedef struct {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
    uint32_t syms[4];
    uint32_t mmap_length;
    uint32_t mmap_addr;
} __attribute__((packed)) multiboot_info_t;

typedef struct {
    uint32_t size;
    uint64_t addr;
    uint64_t len;
    uint32_t type;
} __attribute__((packed)) multiboot_mmap_t;

/* Layout expected by the existing pmm.c E820 parser. */
typedef struct {
    uint64_t base;
    uint64_t length;
    uint32_t type;
    uint32_t acpi_attrs;
} __attribute__((packed)) e820_output_t;

static void output_entry(e820_output_t *out,
                         uint64_t base,
                         uint64_t length,
                         uint32_t type) {
    out->base = base;
    out->length = length;
    out->type = type;
    out->acpi_attrs = 0;
}

int multiboot_prepare_memory_map(uint32_t magic, uint32_t info_addr) {
    multiboot_info_t *info;
    e820_output_t *out;
    uint16_t count = 0;

    if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
        return 0;
    }

    if (!info_addr) {
        return -1;
    }

    info = (multiboot_info_t *)info_addr;
    out = (e820_output_t *)E820_BUFFER_BASE;

    if (info->flags & MB_FLAG_MMAP) {
        uint32_t pos = info->mmap_addr;
        uint32_t end = info->mmap_addr + info->mmap_length;

        while (pos < end && count < E820_MAX_ENTRIES) {
            multiboot_mmap_t *entry =
                (multiboot_mmap_t *)pos;

            /* Multiboot mmap entry payload must contain
             * addr + len + type = at least 20 bytes. */
            if (entry->size < 20U) {
                break;
            }

            output_entry(&out[count],
                         entry->addr,
                         entry->len,
                         entry->type);

            count++;

            /* size excludes the size field itself. */
            pos += entry->size + sizeof(uint32_t);
        }
    } else if (info->flags & MB_FLAG_MEMORY) {
        /*
         * Fallback when GRUB provides only mem_lower/mem_upper:
         * lower memory starts at 0, upper memory starts at 1 MiB.
         */
        if (info->mem_lower && count < E820_MAX_ENTRIES) {
            output_entry(&out[count++],
                         0,
                         (uint64_t)info->mem_lower * 1024ULL,
                         1);
        }

        if (info->mem_upper && count < E820_MAX_ENTRIES) {
            output_entry(&out[count++],
                         0x100000ULL,
                         (uint64_t)info->mem_upper * 1024ULL,
                         1);
        }
    }

    *E820_BUFFER_COUNT = count;

    return count ? 1 : -1;
}
