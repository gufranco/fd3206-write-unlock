/* SPDX-FileCopyrightText: 2026 Gustavo Franco <gufranco@users.noreply.github.com> */
/* SPDX-License-Identifier: MIT */

#include "port/registers.h"

#if FDSWU_TOGGLE_BIT != 0
#error "fdswu_port_begin_update returns the toggle bit as a bool, so it must be bit 0"
#endif

#define rFirst  r24
#define rSecond r22
#define rThird  r20
#define rWork   r25

#define WATCHDOG_CHANGE (_BV(WDCE) | _BV(WDE))
#define WATCHDOG_60MS   (_BV(WDE) | _BV(WDP1))

    .section .text.fdswu_port_start,"ax",@progbits
    .global fdswu_port_start
fdswu_port_start:
    out _SFR_IO_ADDR(MCUSR), r1
    wdr
    ldi rWork, WATCHDOG_CHANGE
    out _SFR_IO_ADDR(FDSWU_WATCHDOG_CONTROL), rWork
    ldi rWork, WATCHDOG_60MS
    out _SFR_IO_ADDR(FDSWU_WATCHDOG_CONTROL), rWork
    ldi rWork, _BV(CLKPCE)
    out _SFR_IO_ADDR(CLKPR), rWork
    out _SFR_IO_ADDR(CLKPR), r1
    out _SFR_IO_ADDR(PORTB), rFirst
    out _SFR_IO_ADDR(PORTD), rSecond
    out _SFR_IO_ADDR(FDSWU_HEAD_DIRECTION), r1
    ldi rWork, _BV(ISC01)
    out _SFR_IO_ADDR(MCUCR), rWork
    ldi rWork, _BV(INTF0)
    out _SFR_IO_ADDR(EIFR), rWork
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

    .section .text.fdswu_port_begin_update,"ax",@progbits
    .global fdswu_port_begin_update
fdswu_port_begin_update:
    cli
    in rFirst, _SFR_IO_ADDR(FDSWU_TOGGLE_STATE)
    andi rFirst, _BV(FDSWU_TOGGLE_BIT)
    ret

    .section .text.fdswu_port_commit_heads,"ax",@progbits
    .global fdswu_port_commit_heads
fdswu_port_commit_heads:
    out _SFR_IO_ADDR(FDSWU_HEAD_DIRECTION), rFirst
    out _SFR_IO_ADDR(FDSWU_NEXT_EDGE_HEADS), rSecond
    out _SFR_IO_ADDR(FDSWU_LATER_EDGE_HEADS), rThird
    sei
    ret

    .section .text.fdswu_port_head_direction,"ax",@progbits
    .global fdswu_port_head_direction
fdswu_port_head_direction:
    in rFirst, _SFR_IO_ADDR(FDSWU_HEAD_DIRECTION)
    ret

    .section .text.fdswu_port_pending_heads,"ax",@progbits
    .global fdswu_port_pending_heads
fdswu_port_pending_heads:
    in rFirst, _SFR_IO_ADDR(FDSWU_NEXT_EDGE_HEADS)
    in rWork, _SFR_IO_ADDR(FDSWU_LATER_EDGE_HEADS)
    or rFirst, rWork
    ret

    .section .text.fdswu_assert_fail,"ax",@progbits
    .global fdswu_assert_fail
fdswu_assert_fail:
    cli
    clr rWork
    out _SFR_IO_ADDR(FDSWU_HEAD_DIRECTION), rWork
    out _SFR_IO_ADDR(MCUSR), rWork
    wdr
    ldi rFirst, WATCHDOG_CHANGE
    out _SFR_IO_ADDR(FDSWU_WATCHDOG_CONTROL), rFirst
    out _SFR_IO_ADDR(FDSWU_WATCHDOG_CONTROL), rWork
.Lhalt:
    rjmp .Lhalt
