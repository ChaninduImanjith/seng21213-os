#ifndef MUTEX_H
#define MUTEX_H

#include "process.h"

typedef struct {
    volatile int locked;
    pcb_t        *owner;                /* who currently holds it -- 0 if free */
    int           owner_saved_priority; /* owner's priority before any inheritance boost */
} mutex_t;

void mutex_init(mutex_t *m);
void mutex_lock(mutex_t *m);
void mutex_unlock(mutex_t *m);

#endif
