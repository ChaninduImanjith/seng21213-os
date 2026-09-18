#include "../include/types.h"
#include "pmm.h"
#include "buddy.h"

typedef struct buddy_node {
    struct buddy_node *next;
} buddy_node_t;

static buddy_node_t *free_lists[BUDDY_MAX_ORDER + 1];

static uint32_t pool_base = 0;
static uint32_t free_pages = 0;
static int initialized = 0;

static uint32_t pages_for_order(uint32_t order) {
    return 1U << order;
}

static uint32_t bytes_for_order(uint32_t order) {
    return pages_for_order(order) * FRAME_SIZE;
}

static void push_block(uint32_t order, uint32_t addr) {
    buddy_node_t *node = (buddy_node_t *)addr;

    node->next = free_lists[order];
    free_lists[order] = node;
}

static uint32_t pop_block(uint32_t order) {
    buddy_node_t *node = free_lists[order];

    if (!node) {
        return 0;
    }

    free_lists[order] = node->next;
    node->next = 0;

    return (uint32_t)node;
}

/* Remove a particular free block from an order's linked list.
 * Returns 1 when found, 0 otherwise.
 */
static int remove_block(uint32_t order, uint32_t addr) {
    buddy_node_t *prev = 0;
    buddy_node_t *cur = free_lists[order];

    while (cur) {
        if ((uint32_t)cur == addr) {
            if (prev) {
                prev->next = cur->next;
            } else {
                free_lists[order] = cur->next;
            }

            cur->next = 0;
            return 1;
        }

        prev = cur;
        cur = cur->next;
    }

    return 0;
}

void buddy_init(void) {
    uint32_t i;

    if (initialized) {
        return;
    }

    for (i = 0; i <= BUDDY_MAX_ORDER; i++) {
        free_lists[i] = 0;
    }

    /*
     * Require the pool to be aligned to its full 1 MiB size.
     * 256 frames * 4096 bytes = 1 MiB.
     */
    pool_base =
        pmm_alloc_contiguous(BUDDY_POOL_PAGES, BUDDY_POOL_PAGES);

    if (!pool_base) {
        return;
    }

    push_block(BUDDY_MAX_ORDER, pool_base);

    free_pages = BUDDY_POOL_PAGES;
    initialized = 1;
}

void *buddy_alloc(uint32_t order) {
    uint32_t current;
    uint32_t block;

    if (!initialized || order > BUDDY_MAX_ORDER) {
        return 0;
    }

    current = order;

    while (current <= BUDDY_MAX_ORDER &&
           free_lists[current] == 0) {
        current++;
    }

    if (current > BUDDY_MAX_ORDER) {
        return 0;
    }

    block = pop_block(current);

    /*
     * Repeatedly split:
     *
     *      one order-N block
     *             |
     *       ---------------
     *       |             |
     *     keep          free buddy
     */
    while (current > order) {
        uint32_t buddy_addr;

        current--;

        buddy_addr =
            block + bytes_for_order(current);

        push_block(current, buddy_addr);
    }

    free_pages -= pages_for_order(order);

    return (void *)block;
}

int buddy_free(void *ptr, uint32_t order) {
    uint32_t addr;
    uint32_t offset;

    if (!initialized || !ptr || order > BUDDY_MAX_ORDER) {
        return -1;
    }

    addr = (uint32_t)ptr;

    if (addr < pool_base ||
        addr >= pool_base + BUDDY_POOL_PAGES * FRAME_SIZE) {
        return -1;
    }

    offset = addr - pool_base;

    /* A valid order-N block must be aligned to its own size. */
    if ((offset % bytes_for_order(order)) != 0) {
        return -1;
    }

    free_pages += pages_for_order(order);

    /*
     * Buddy calculation:
     *
     * buddy_offset = block_offset XOR block_size
     *
     * If that buddy is currently free at the same order, remove it and
     * merge the pair into their order+1 parent.
     */
    while (order < BUDDY_MAX_ORDER) {
        uint32_t block_size = bytes_for_order(order);
        uint32_t buddy_offset = offset ^ block_size;
        uint32_t buddy_addr = pool_base + buddy_offset;

        if (!remove_block(order, buddy_addr)) {
            break;
        }

        if (buddy_offset < offset) {
            offset = buddy_offset;
        }

        addr = pool_base + offset;
        order++;
    }

    push_block(order, addr);

    return 0;
}

uint32_t buddy_pool_base(void) {
    return pool_base;
}

uint32_t buddy_free_pages(void) {
    return free_pages;
}

uint32_t buddy_free_block_count(uint32_t order) {
    buddy_node_t *node;
    uint32_t count = 0;

    if (order > BUDDY_MAX_ORDER) {
        return 0;
    }

    node = free_lists[order];

    while (node) {
        count++;
        node = node->next;
    }

    return count;
}
