/* SPDX-FileCopyrightText: 2026 Gustavo Franco <gufranco@users.noreply.github.com> */
/* SPDX-License-Identifier: MIT */

#include <stdbool.h>
#include <stdint.h>

#include "fdswu/conditions.h"
#include "fdswu/pins.h"
#include "fdswu/write_plan.h"
#include "port/port.h"

static void apply_write_lines(bool allowed) {
    const fdswu_write_plan_t if_clear = fdswu_write_plan(false, allowed);
    const fdswu_write_plan_t if_set = fdswu_write_plan(true, allowed);
    fdswu_port_commit_write_lines(if_clear.now, if_clear.next_edge, if_clear.later_edge, if_set.now, if_set.next_edge,
                                  if_set.later_edge);
}

static void start(void) {
    fdswu_port_start(FDSWU_PULLUPS_A, FDSWU_PULLUPS_B, FDSWU_PULLUPS_D);
    apply_write_lines(false);
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
            apply_write_lines(allowed);
            was_allowed = allowed;
        }
    }
}
