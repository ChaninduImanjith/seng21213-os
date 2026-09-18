#include "../include/types.h"
#include "deadlock.h"

static void *resource_ids[DL_MAX_RESOURCES];
static pcb_t *resource_owner[DL_MAX_RESOURCES];
static int    resource_count = 0;

/* wait_for[i] = the resource process-table-slot i is currently
 * blocked waiting for, or 0 if it isn't waiting on anything tracked. */
static void *wait_for[MAX_PROCESSES];

static int find_resource_slot(void *id) {
    int i;
    for (i = 0; i < resource_count; i++) {
        if (resource_ids[i] == id) return i;
    }
    return -1;
}

void deadlock_register_resource(void *id) {
    if (resource_count < DL_MAX_RESOURCES && find_resource_slot(id) < 0) {
        resource_ids[resource_count] = id;
        resource_owner[resource_count] = 0;
        resource_count++;
    }
}

void deadlock_note_owner(void *id, pcb_t *owner) {
    int slot = find_resource_slot(id);
    if (slot >= 0) resource_owner[slot] = owner;
}

void deadlock_note_waiting(pcb_t *proc, void *id) {
    int idx = process_index_of(proc);
    if (idx >= 0 && idx < MAX_PROCESSES) wait_for[idx] = id;
}

void deadlock_note_done_waiting(pcb_t *proc) {
    int idx = process_index_of(proc);
    if (idx >= 0 && idx < MAX_PROCESSES) wait_for[idx] = 0;
}

/* From every waiting process, follow the chain: this process -> the
 * resource it wants -> that resource's current owner -> the resource
 * THAT owner wants -> ... If the chain ever loops back to where it
 * started, that is a genuine cycle in the resource-allocation graph:
 * a deadlock. Bounded to MAX_PROCESSES hops since a real cycle can't
 * be any longer than the number of processes involved. */
int deadlock_check(void) {
    int start_idx;
    for (start_idx = 0; start_idx < MAX_PROCESSES; start_idx++) {
        if (!wait_for[start_idx]) continue;

        int cur_idx = start_idx;
        int hops;
        for (hops = 0; hops < MAX_PROCESSES; hops++) {
            void *res = wait_for[cur_idx];
            if (!res) break;

            int slot = find_resource_slot(res);
            if (slot < 0) break;

            pcb_t *owner = resource_owner[slot];
            if (!owner) break;

            int owner_idx = process_index_of(owner);
            if (owner_idx == start_idx) return 1;   /* cycle back to start -- deadlock */
            if (owner_idx < 0) break;

            cur_idx = owner_idx;
        }
    }
    return 0;
}
