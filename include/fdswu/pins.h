/* SPDX-FileCopyrightText: 2026 Gustavo Franco <gufranco@users.noreply.github.com> */
/* SPDX-License-Identifier: MIT */

#ifndef FDSWU_PINS_H
#define FDSWU_PINS_H

#include <stdint.h>

#define FDSWU_HEAD1_MASK             ((uint8_t)0x08U)
#define FDSWU_HEAD2_MASK             ((uint8_t)0x04U)
#define FDSWU_HEADS_MASK             ((uint8_t)0x0CU)
#define FDSWU_NOT_HEADS_MASK         ((uint8_t)0xF3U)
#define FDSWU_READY_MASK             ((uint8_t)0x02U)
#define FDSWU_GATE_AND_WRITABLE_MASK ((uint8_t)0x03U)
#define FDSWU_UNUSED_PULLUPS_B       ((uint8_t)0xF1U)
#define FDSWU_UNUSED_PULLUPS_D       ((uint8_t)0x7BU)

_Static_assert((FDSWU_HEAD1_MASK | FDSWU_HEAD2_MASK) == FDSWU_HEADS_MASK, "head masks make up the heads mask");
_Static_assert((FDSWU_HEAD1_MASK & FDSWU_HEAD2_MASK) == 0U, "head masks are disjoint");
_Static_assert((FDSWU_HEADS_MASK ^ FDSWU_NOT_HEADS_MASK) == 0xFFU, "not-heads mask is the complement");
_Static_assert((FDSWU_UNUSED_PULLUPS_B & (FDSWU_HEADS_MASK | FDSWU_READY_MASK)) == 0U,
               "no pull-up on a soldered port B pin");

#endif
