/* SPDX-FileCopyrightText: 2026 Gustavo Franco <gufranco@users.noreply.github.com> */
/* SPDX-License-Identifier: MIT */

#ifndef FDSWU_WRITE_PLAN_H
#define FDSWU_WRITE_PLAN_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t now;
    uint8_t next_edge;
    uint8_t later_edge;
} fdswu_write_plan_t;

fdswu_write_plan_t fdswu_write_plan(bool toggled, bool allowed);

#endif
