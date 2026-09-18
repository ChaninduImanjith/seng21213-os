#ifndef DEADLOCK_H
#define DEADLOCK_H

#include "process.h"

#define DL_MAX_RESOURCES 8

/* Opt-in resource-allocation graph tracker. Nothing in mutex.c/
 * semaphore.c calls these automatically -- a caller that wants a
 * particular lock watched for deadlock registers it explicitly and
 * reports acquire/wait/release events around its own lock calls. */
void deadlock_register_resource(void *resource_id);
void deadlock_note_owner(void *resource_id, pcb_t *owner);   /* owner == 0 means "free" */
void deadlock_note_waiting(pcb_t *proc, void *resource_id);
void deadlock_note_done_waiting(pcb_t *proc);

/* Walks the graph (process -> resource it wants -> resource's owner ->
 * owner's wanted resource -> ...) looking for a cycle. Returns 1 if a
 * deadlock is present, 0 otherwise. */
int  deadlock_check(void);

#endif
