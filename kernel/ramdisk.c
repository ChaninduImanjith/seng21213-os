#include "../include/types.h"
#include "ramdisk.h"
#include "pmm.h"

void ramdisk_init(void) {
    /* Tell the PMM this physical range is off-limits before touching it. */
    pmm_reserve_range(RD_PHYS_BASE, (uint32_t)RD_TOTAL_BLOCKS * RD_BLOCK_SIZE);

    uint8_t *disk = (uint8_t *)RD_PHYS_BASE;
    uint32_t i;
    for (i = 0; i < (uint32_t)RD_TOTAL_BLOCKS * RD_BLOCK_SIZE; i++) disk[i] = 0;
}

void *ramdisk_block_ptr(uint32_t block_num) {
    if (block_num >= RD_TOTAL_BLOCKS) return 0;
    return (void *)(RD_PHYS_BASE + block_num * RD_BLOCK_SIZE);
}
