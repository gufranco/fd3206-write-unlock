/* SPDX-FileCopyrightText: 2026 Gustavo Franco <gufranco@users.noreply.github.com> */
/* SPDX-License-Identifier: MIT */

#ifndef FDSWU_PORT_REGISTERS_H
#define FDSWU_PORT_REGISTERS_H

#if !defined(__AVR_ATtiny2313A__) && !defined(__AVR_ATtiny4313__)
#error "unsupported MCU: the firmware targets the ATtiny2313A and the ATtiny4313 only"
#endif

#include <avr/io.h>

#define FDSWU_NEXT_EDGE_LINES      GPIOR0
#define FDSWU_LATER_EDGE_LINES     GPIOR1
#define FDSWU_TOGGLE_STATE         GPIOR2
#define FDSWU_TOGGLE_BIT           0
#define FDSWU_WRITE_LINE_DIRECTION DDRB
#define FDSWU_WATCHDOG_CONTROL     WDTCR

#endif
