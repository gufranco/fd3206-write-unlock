/* SPDX-FileCopyrightText: 2026 Gustavo Franco <gufranco@users.noreply.github.com> */
/* SPDX-License-Identifier: MIT */

#include "fdswu/heads.h"

#include "fdswu/assert.h"
#include "fdswu/pins.h"

fdswu_heads_plan_t fdswu_heads_plan(bool toggled, bool allowed) {
    const uint8_t current = toggled ? FDSWU_HEAD2_MASK : FDSWU_HEAD1_MASK;
    const uint8_t other = toggled ? FDSWU_HEAD1_MASK : FDSWU_HEAD2_MASK;
    const uint8_t enable = allowed ? FDSWU_HEADS_MASK : 0U;
    const fdswu_heads_plan_t plan = {
        .now = (uint8_t)(current & enable),
        .next_edge = (uint8_t)(other & enable),
        .later_edge = (uint8_t)(current & enable),
    };
    FDSWU_ASSERT((plan.now & plan.next_edge) == 0U);
    FDSWU_ASSERT((uint8_t)(plan.now | plan.next_edge | plan.later_edge) == (uint8_t)(FDSWU_HEADS_MASK & enable));
    return plan;
}
