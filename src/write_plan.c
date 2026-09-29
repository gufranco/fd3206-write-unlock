/* SPDX-FileCopyrightText: 2026 Gustavo Franco <gufranco@users.noreply.github.com> */
/* SPDX-License-Identifier: MIT */

#include "fdswu/write_plan.h"

#include "fdswu/assert.h"
#include "fdswu/pins.h"

fdswu_write_plan_t fdswu_write_plan(bool toggled, bool allowed) {
    const uint8_t current = toggled ? FDSWU_WRITE_LINE2_MASK : FDSWU_WRITE_LINE1_MASK;
    const uint8_t other = toggled ? FDSWU_WRITE_LINE1_MASK : FDSWU_WRITE_LINE2_MASK;
    const uint8_t enable = allowed ? FDSWU_WRITE_LINES_MASK : 0U;
    const fdswu_write_plan_t plan = {
        .now = (uint8_t)(current & enable),
        .next_edge = (uint8_t)(other & enable),
        .later_edge = (uint8_t)(current & enable),
    };
    FDSWU_ASSERT((plan.now & plan.next_edge) == 0U);
    FDSWU_ASSERT((uint8_t)(plan.now | plan.next_edge | plan.later_edge) == (uint8_t)(FDSWU_WRITE_LINES_MASK & enable));
    return plan;
}
