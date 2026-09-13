#ifndef RAMDISK_H
#define RAMDISK_H

#include "../include/types.h"

#define RD_BLOCK_SIZE   4096
#define RD_TOTAL_BLOCKS 256          /* 256 * 4096 = 1 MB */

/* Fixed physical address for the RAM disk: 2MB, safely above both the
 * 1MB line and the 0xA0000-0xFFFFF VGA/BIOS hole. A plain .bss array
 * pushed the kernel's own footprint (0x10000 + text+data+bss) past
 * 0xA0000, colliding with the VGA text buffer at 0xB8000 -- this fixed
 * address sidesteps that entirely. */
#define RD_PHYS_BASE    0x200000

void  ramdisk_init(void);
void *ramdisk_block_ptr(uint32_t block_num);

#endif
