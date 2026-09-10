#ifndef THREAD_H
#define THREAD_H

#include "process.h"

/* Create a kernel thread: same scheduling unit as a process, but takes
 * an argument. Runs entirely inside the kernel's address space. */
pcb_t *thread_create(void (*fn)(void *), void *arg);

#endif
