#ifndef FDSWU_HEADS_H
#define FDSWU_HEADS_H

#include <stdint.h>

typedef struct {
    uint8_t now;
    uint8_t next_edge;
    uint8_t later_edge;
} fdswu_heads_plan_t;

[[nodiscard]] fdswu_heads_plan_t fdswu_heads_plan(bool toggled, bool allowed);

#endif
