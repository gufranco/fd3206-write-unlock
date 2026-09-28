/* SPDX-FileCopyrightText: 2026 Gustavo Franco <gufranco@users.noreply.github.com> */
/* SPDX-License-Identifier: MIT */

#include "fdswu/conditions.h"

#include "fdswu/pins.h"

bool fdswu_conditions_allow(uint8_t port_a_pins, uint8_t port_b_pins) {
    return ((port_a_pins & FDSWU_GATE_AND_PROTECT_MASK) == 0U) && ((port_b_pins & FDSWU_READY_MASK) == 0U);
}
