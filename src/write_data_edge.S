/* SPDX-FileCopyrightText: 2026 Gustavo Franco <gufranco@users.noreply.github.com> */
/* SPDX-License-Identifier: MIT */

#include "port/registers.h"

#define rNext r16
#define rAfter r17

    .section .text

    .global INT0_vect
INT0_vect:
    push rNext
    in rNext, _SFR_IO_ADDR(FDSWU_NEXT_EDGE_LINES)
    out _SFR_IO_ADDR(FDSWU_WRITE_LINE_DIRECTION), rNext
    push rAfter
    in rAfter, _SFR_IO_ADDR(FDSWU_LATER_EDGE_LINES)
    out _SFR_IO_ADDR(FDSWU_NEXT_EDGE_LINES), rAfter
    out _SFR_IO_ADDR(FDSWU_LATER_EDGE_LINES), rNext
    sbis _SFR_IO_ADDR(FDSWU_TOGGLE_STATE), FDSWU_TOGGLE_BIT
    rjmp .Lset_toggle
    cbi _SFR_IO_ADDR(FDSWU_TOGGLE_STATE), FDSWU_TOGGLE_BIT
    rjmp .Ldone
.Lset_toggle:
    sbi _SFR_IO_ADDR(FDSWU_TOGGLE_STATE), FDSWU_TOGGLE_BIT
.Ldone:
    pop rAfter
    pop rNext
    reti
