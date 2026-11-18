/* SPDX-FileCopyrightText: 2026 Gustavo Franco <gufranco@users.noreply.github.com> */
/* SPDX-License-Identifier: MIT */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "fdswu/conditions.h"
#include "fdswu/write_plan.h"

#define INPUT_BYTES ((size_t)3U)
#define NO_LINE     ((uint8_t)0x00U)
#define LINE1       ((uint8_t)0x08U)
#define LINE2       ((uint8_t)0x04U)

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

static void require(bool holds) {
    if (!holds) {
        abort();
    }
}

static bool all_three_low(uint8_t port_a, uint8_t port_b) {
    const bool gate_and_writable_low = (port_a & 0x03U) == 0U;
    const bool ready_low = (port_b & 0x02U) == 0U;
    return gate_and_writable_low && ready_low;
}

static void require_one_line_per_state(fdswu_write_plan_t plan, bool toggled, bool allowed) {
    const uint8_t driven_now = toggled ? LINE2 : LINE1;
    const uint8_t driven_next = toggled ? LINE1 : LINE2;

    require(plan.now == (allowed ? driven_now : NO_LINE));
    require(plan.next_edge == (allowed ? driven_next : NO_LINE));
    require(plan.later_edge == plan.now);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size >= INPUT_BYTES) {
        const bool toggled = (data[2] & 0x01U) != 0U;

        const bool allowed = fdswu_conditions_allow(data[0], data[1]);
        const fdswu_write_plan_t plan = fdswu_write_plan(toggled, allowed);

        require(allowed == all_three_low(data[0], data[1]));
        require_one_line_per_state(plan, toggled, allowed);
    }
    return 0;
}
