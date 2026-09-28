/* SPDX-FileCopyrightText: 2026 Gustavo Franco <gufranco@users.noreply.github.com> */
/* SPDX-License-Identifier: MIT */

#include <stdbool.h>
#include <stdint.h>

#include "fdswu/assert.h"
#include "fdswu/conditions.h"
#include "fdswu/heads.h"
#include "fdswu/pins.h"
#include "port/port.h"

static void apply_heads(bool allowed) {
    const bool toggled = fdswu_port_begin_update();
    const fdswu_heads_plan_t plan = fdswu_heads_plan(toggled, allowed);
    fdswu_port_commit_heads(plan.now, plan.next_edge, plan.later_edge);
    FDSWU_ASSERT((fdswu_port_head_direction() & FDSWU_NOT_HEADS_MASK) == 0U);
    FDSWU_ASSERT(allowed || (fdswu_port_head_direction() == 0U));
}

static void start(void) {
    fdswu_port_start(FDSWU_UNUSED_PULLUPS_B, FDSWU_UNUSED_PULLUPS_D);
    apply_heads(false);
    FDSWU_ASSERT(fdswu_port_head_direction() == 0U);
    FDSWU_ASSERT(fdswu_port_pending_heads() == 0U);
}

int main(void) {
    start();
    bool was_allowed = false;
    for (;;) {
        const uint16_t pins = fdswu_port_poll();
        const uint8_t port_a_pins = (uint8_t)(pins & 0x00FFU);
        const uint8_t port_b_pins = (uint8_t)(pins >> 8U);
        const bool allowed = fdswu_conditions_allow(port_a_pins, port_b_pins);
        if (allowed != was_allowed) {
            apply_heads(allowed);
            was_allowed = allowed;
        }
    }
}
