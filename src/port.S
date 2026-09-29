/* SPDX-FileCopyrightText: 2026 Gustavo Franco <gufranco@users.noreply.github.com> */
/* SPDX-License-Identifier: MIT */

#include "port/registers.h"

#define rFirst  r24
#define rSecond r22
#define rThird  r20
#define rFourth r18
#define rFifth  r16
#define rSixth  r14
#define rWork   r25

#define WATCHDOG_CHANGE (_BV(WDCE) | _BV(WDE))
#define WATCHDOG_60MS   (_BV(WDE) | _BV(WDP1))

    .section .text.fdswu_port_start,"ax",@progbits
    .global fdswu_port_start
fdswu_port_start:
    wdr
    ldi rWork, WATCHDOG_CHANGE
    out _SFR_IO_ADDR(FDSWU_WATCHDOG_CONTROL), rWork
    ldi rWork, WATCHDOG_60MS
    out _SFR_IO_ADDR(FDSWU_WATCHDOG_CONTROL), rWork
    ldi rWork, _BV(CLKPCE)
    out _SFR_IO_ADDR(CLKPR), rWork
    out _SFR_IO_ADDR(CLKPR), r1
    out _SFR_IO_ADDR(PORTA), rFirst
    out _SFR_IO_ADDR(PORTB), rSecond
    out _SFR_IO_ADDR(PORTD), rThird
    ldi rWork, _BV(ISC01)
    out _SFR_IO_ADDR(MCUCR), rWork
    ldi rWork, _BV(INT0)
    out _SFR_IO_ADDR(GIMSK), rWork
    ret

    .section .text.fdswu_port_poll,"ax",@progbits
    .global fdswu_port_poll
fdswu_port_poll:
    wdr
    in rFirst, _SFR_IO_ADDR(PINA)
    in rWork, _SFR_IO_ADDR(PINB)
    ret

    .section .text.fdswu_port_commit_write_lines,"ax",@progbits
    .global fdswu_port_commit_write_lines
fdswu_port_commit_write_lines:
    cli
    sbic _SFR_IO_ADDR(FDSWU_TOGGLE_STATE), FDSWU_TOGGLE_BIT
    rjmp .Lcommit_set
    out _SFR_IO_ADDR(FDSWU_WRITE_LINE_DIRECTION), rFirst
    out _SFR_IO_ADDR(FDSWU_NEXT_EDGE_LINES), rSecond
    out _SFR_IO_ADDR(FDSWU_LATER_EDGE_LINES), rThird
    sei
    ret
.Lcommit_set:
    out _SFR_IO_ADDR(FDSWU_WRITE_LINE_DIRECTION), rFourth
    out _SFR_IO_ADDR(FDSWU_NEXT_EDGE_LINES), rFifth
    out _SFR_IO_ADDR(FDSWU_LATER_EDGE_LINES), rSixth
    sei
    ret
