#include "../include/types.h"
#include "kmalloc.h"
#include "pmm.h"

/* Extension: kmalloc/kfree, a variable-size kernel heap allocator
 * sitting entirely ON TOP of the existing frame allocator -- it never
 * touches pmm.c. Each "arena" is one 4KB frame from pmm_alloc_frame(),
 * carved into a singly-linked list of free-list blocks; kmalloc()
 * first-fits within existing arenas before requesting a new one. */

typedef struct block_header {
    uint32_t size;                 /* usable size, excluding this header */
    int      free;
    struct block_header *next;     /* next block within the SAME arena */
} block_header_t;

typedef struct arena {
    struct arena  *next;
    block_header_t *first_block;
} arena_t;

static arena_t *arenas = 0;

#define HEADER_SIZE ((uint32_t)sizeof(block_header_t))

static arena_t *new_arena(void) {
    uint32_t frame = pmm_alloc_frame();
    if (!frame) return 0;   /* out of physical memory */

    arena_t *a = (arena_t *)frame;
    a->next = arenas;

    block_header_t *b = (block_header_t *)(frame + sizeof(arena_t));
    b->size = FRAME_SIZE - (uint32_t)sizeof(arena_t) - HEADER_SIZE;
    b->free = 1;
    b->next = 0;
    a->first_block = b;

    arenas = a;
    return a;
}

void kmalloc_init(void) {
    arenas = 0;
}

void *kmalloc(uint32_t size) {
    if (size == 0) return 0;
    size = (size + 3u) & ~3u;   /* 4-byte align */

    arena_t *a = arenas;
    while (a) {
        block_header_t *b = a->first_block;
        while (b) {
            if (b->free && b->size >= size) {
                /* Split off the remainder as a new free block if
                 * there's enough room left to bother. */
                if (b->size >= size + HEADER_SIZE + 8) {
                    block_header_t *rem =
                        (block_header_t *)((uint8_t *)b + HEADER_SIZE + size);
                    rem->size = b->size - size - HEADER_SIZE;
                    rem->free = 1;
                    rem->next = b->next;
                    b->next = rem;
                    b->size = size;
                }
                b->free = 0;
                return (void *)((uint8_t *)b + HEADER_SIZE);
            }
            b = b->next;
        }
        a = a->next;
    }

    /* Nothing fits anywhere -- grab a fresh page and retry once. */
    if (!new_arena()) return 0;
    return kmalloc(size);
}

void kfree(void *ptr) {
    if (!ptr) return;
    block_header_t *b = (block_header_t *)((uint8_t *)ptr - HEADER_SIZE);
    b->free = 1;
    /* No coalescing with neighbours -- kept simple. A freed block is
     * still reusable by a later kmalloc() of equal or smaller size;
     * the tradeoff is some fragmentation over time. */
}
