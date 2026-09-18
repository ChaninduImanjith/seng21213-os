#ifndef JOURNAL_H
#define JOURNAL_H

#include "../include/types.h"

#define JOURNAL_STATE_CLEAN      0
#define JOURNAL_STATE_PREPARED   1
#define JOURNAL_STATE_COMMITTED  2

void journal_init(void);

/* Begin/commit a metadata transaction. */
int  journal_begin(void);
int  journal_commit(void);
void journal_abort(void);

/* Used by fs.c metadata accessors. */
void *journal_metadata_ptr(uint32_t block_num);

/* Replay a committed redo journal. */
int journal_recover(void);

uint32_t journal_state(void);
uint32_t journal_sequence(void);

/* Testing hook: next commit writes a valid committed log but deliberately
 * skips checkpointing it to the home metadata blocks. */
void journal_test_defer_next_commit(void);

#endif
