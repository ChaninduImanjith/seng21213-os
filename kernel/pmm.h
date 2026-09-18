#ifndef PMM_H
#define PMM_H

#include "../include/types.h"

#define FRAME_SIZE 4096

void     pmm_init(void);
uint32_t pmm_alloc_frame(void);   /* returns physical address, 0 = out of memory */

/* Reserve a physically contiguous run of frames.
 * alignment_frames is expressed in frames (1 = no special alignment). */
uint32_t pmm_alloc_contiguous(uint32_t frame_count,
                              uint32_t alignment_frames);

void     pmm_free_frame(uint32_t phys_addr);
void     pmm_reserve_range(uint32_t start_addr, uint32_t length);
uint32_t pmm_total_frames(void);
uint32_t pmm_free_frames(void);
uint32_t pmm_used_frames(void);

#endif
