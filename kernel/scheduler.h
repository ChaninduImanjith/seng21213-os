#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "../include/types.h"

void     scheduler_init(void);
uint32_t scheduler_ticks(void);
void     sleep_ms(uint32_t ms);

#endif
