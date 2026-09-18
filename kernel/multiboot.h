#ifndef MULTIBOOT_H
#define MULTIBOOT_H

#include "../include/types.h"

#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002U

/* Convert GRUB's Multiboot memory map into the E820-style buffer already
 * consumed by pmm_init() at 0x8000/0x8004.
 *
 * Returns:
 *   1  = GRUB map imported
 *   0  = not a Multiboot boot
 *  -1  = Multiboot boot, but no usable memory information
 */
int multiboot_prepare_memory_map(uint32_t magic, uint32_t info_addr);

#endif
