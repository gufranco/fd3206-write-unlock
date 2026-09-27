#ifndef FDSWRITEUNLOCK_EDGE_COUNTER_H
#define FDSWRITEUNLOCK_EDGE_COUNTER_H

#include <stdint.h>

#include "board.h"
#include "hardware/structs/pwm.h"

#define EDGE_COUNTER_SLICE ((PIN_WRITE_DATA >> 1) & 7u)

static inline uint16_t edge_counter_raw(void) {
    return (uint16_t)pwm_hw->slice[EDGE_COUNTER_SLICE].ctr;
}

void edge_counter_start(void);
void edge_counter_poll(void);
uint32_t edge_counter_total(void);
uint32_t edge_counter_resolve(uint16_t raw_snapshot);

#endif
