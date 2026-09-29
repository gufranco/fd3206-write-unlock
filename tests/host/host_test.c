/* SPDX-FileCopyrightText: 2026 Gustavo Franco <gufranco@users.noreply.github.com> */
/* SPDX-License-Identifier: MIT */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "fdswu/conditions.h"
#include "fdswu/heads.h"
#include "fdswu/pins.h"

#define PORT_VALUES ((uint16_t)256U)

typedef struct {
    bool toggled;
    bool allowed;
    fdswu_heads_plan_t expected;
} plan_case_t;

static const plan_case_t PLAN_CASES[] = {
    {false, false, {0U, 0U, 0U}},
    {true, false, {0U, 0U, 0U}},
    {false, true, {FDSWU_HEAD1_MASK, FDSWU_HEAD2_MASK, FDSWU_HEAD1_MASK}},
    {true, true, {FDSWU_HEAD2_MASK, FDSWU_HEAD1_MASK, FDSWU_HEAD2_MASK}},
};

static uint32_t failures;

static void expect(bool holds, const char *what, uint32_t detail) {
    if (!holds) {
        fprintf(stderr, "host test failed: %s (%lu)\n", what, (unsigned long)detail);
        failures++;
    }
}

static void plan_selects_one_head_per_toggle_state(void) {
    for (size_t index = 0U; index < sizeof PLAN_CASES / sizeof PLAN_CASES[0]; index++) {
        const plan_case_t test = PLAN_CASES[index];

        const fdswu_heads_plan_t plan = fdswu_heads_plan(test.toggled, test.allowed);

        expect(plan.now == test.expected.now, "plan.now", (uint32_t)index);
        expect(plan.next_edge == test.expected.next_edge, "plan.next_edge", (uint32_t)index);
        expect(plan.later_edge == test.expected.later_edge, "plan.later_edge", (uint32_t)index);
    }
}

static bool expected_allow(uint8_t port_a, uint8_t port_b) {
    const bool gate_low = (port_a & 0x02U) == 0U;
    const bool writable_low = (port_a & 0x01U) == 0U;
    const bool ready_low = (port_b & 0x02U) == 0U;
    return gate_low && writable_low && ready_low;
}

static void conditions_allow_only_when_all_three_are_low(void) {
    for (uint16_t port_a = 0U; port_a < PORT_VALUES; port_a++) {
        for (uint16_t port_b = 0U; port_b < PORT_VALUES; port_b++) {
            const bool allowed = fdswu_conditions_allow((uint8_t)port_a, (uint8_t)port_b);

            expect(allowed == expected_allow((uint8_t)port_a, (uint8_t)port_b), "conditions",
                   (uint32_t)((uint32_t)port_a << 8U | port_b));
        }
    }
}

int main(void) {
    plan_selects_one_head_per_toggle_state();
    conditions_allow_only_when_all_three_are_low();
    printf("host tests: %lu failures\n", (unsigned long)failures);
    return failures == 0U ? EXIT_SUCCESS : EXIT_FAILURE;
}
