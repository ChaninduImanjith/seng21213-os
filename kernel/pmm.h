#ifndef PMM_H
#define PMM_H

#include "../include/types.h"

#define FRAME_SIZE 4096

void     pmm_init(void);
uint32_t pmm_alloc_frame(void);   /* returns physical address, 0 = out of memory */
void     pmm_free_frame(uint32_t phys_addr);
uint32_t pmm_total_frames(void);
uint32_t pmm_free_frames(void);
uint32_t pmm_used_frames(void);

#endif
