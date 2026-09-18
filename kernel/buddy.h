#ifndef BUDDY_H
#define BUDDY_H

#include "../include/types.h"
#include "pmm.h"

/*
 * 1 MiB teaching buddy pool:
 *
 * order 0 =   1 page  =   4 KiB
 * order 1 =   2 pages =   8 KiB
 * ...
 * order 8 = 256 pages =   1 MiB
 */
#define BUDDY_MAX_ORDER  8
#define BUDDY_POOL_PAGES (1U << BUDDY_MAX_ORDER)

void buddy_init(void);

/* Allocate/free 2^order physically contiguous pages. */
void *buddy_alloc(uint32_t order);
int   buddy_free(void *ptr, uint32_t order);

uint32_t buddy_pool_base(void);
uint32_t buddy_free_pages(void);
uint32_t buddy_free_block_count(uint32_t order);

#endif
