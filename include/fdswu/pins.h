/* SPDX-FileCopyrightText: 2026 Gustavo Franco <gufranco@users.noreply.github.com> */
/* SPDX-License-Identifier: MIT */

#ifndef FDSWU_PINS_H
#define FDSWU_PINS_H

#include <stdint.h>

#define FDSWU_HEAD1_MASK             ((uint8_t)0x08U)
#define FDSWU_HEAD2_MASK             ((uint8_t)0x04U)
#define FDSWU_HEADS_MASK             ((uint8_t)0x0CU)
#define FDSWU_READY_MASK             ((uint8_t)0x02U)
#define FDSWU_GATE_AND_WRITABLE_MASK ((uint8_t)0x03U)
#define FDSWU_PULLUPS_A              ((uint8_t)0x03U)
#define FDSWU_PULLUPS_B              ((uint8_t)0xF3U)
#define FDSWU_PULLUPS_D              ((uint8_t)0x7FU)

_Static_assert((FDSWU_HEAD1_MASK | FDSWU_HEAD2_MASK) == FDSWU_HEADS_MASK, "head masks make up the heads mask");
_Static_assert((FDSWU_HEAD1_MASK & FDSWU_HEAD2_MASK) == 0U, "head masks are disjoint");
_Static_assert((FDSWU_PULLUPS_B & FDSWU_HEADS_MASK) == 0U, "a head pin keeps a 0 latch so enabling it pulls low");
_Static_assert((FDSWU_PULLUPS_B & FDSWU_READY_MASK) == FDSWU_READY_MASK, "the ready input is pulled up");
_Static_assert((FDSWU_PULLUPS_A & FDSWU_GATE_AND_WRITABLE_MASK) == FDSWU_GATE_AND_WRITABLE_MASK,
               "the gate and writable inputs are pulled up");

#endif
