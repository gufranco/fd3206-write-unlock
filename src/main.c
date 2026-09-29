/* SPDX-FileCopyrightText: 2026 Gustavo Franco <gufranco@users.noreply.github.com> */
/* SPDX-License-Identifier: MIT */

#include <stdbool.h>
#include <stdint.h>

#include "fdswu/conditions.h"
#include "fdswu/heads.h"
#include "fdswu/pins.h"
#include "port/port.h"

static void apply_heads(bool allowed) {
    const fdswu_heads_plan_t if_clear = fdswu_heads_plan(false, allowed);
    const fdswu_heads_plan_t if_set = fdswu_heads_plan(true, allowed);
    fdswu_port_commit_heads(if_clear.now, if_clear.next_edge, if_clear.later_edge, if_set.now, if_set.next_edge,
                            if_set.later_edge);
}

static void start(void) {
    fdswu_port_start(FDSWU_PULLUPS_A, FDSWU_PULLUPS_B, FDSWU_PULLUPS_D);
    apply_heads(false);
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
