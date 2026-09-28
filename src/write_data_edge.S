#include "port/registers.h"

#define rNext r16
#define rAfter r17

    .section .text

    .global INT0_vect
INT0_vect:
    push rNext
    in rNext, _SFR_IO_ADDR(FDSWU_NEXT_EDGE_HEADS)
    out _SFR_IO_ADDR(FDSWU_HEAD_DIRECTION), rNext
    push rAfter
    in rAfter, _SFR_IO_ADDR(FDSWU_LATER_EDGE_HEADS)
    out _SFR_IO_ADDR(FDSWU_NEXT_EDGE_HEADS), rAfter
    out _SFR_IO_ADDR(FDSWU_LATER_EDGE_HEADS), rNext
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
