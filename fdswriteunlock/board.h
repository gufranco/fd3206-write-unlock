#ifndef FDSWRITEUNLOCK_BOARD_H
#define FDSWRITEUNLOCK_BOARD_H

#include <avr/io.h>

#if !defined(__AVR_ATtiny2313A__) && !defined(__AVR_ATtiny4313__)
#error "fdswriteunlock supports the ATtiny2313A and ATtiny4313 only"
#endif

#define HEAD1_MASK _BV(PB3)
#define HEAD2_MASK _BV(PB2)
#define READY_MASK _BV(PB1)
#define GATE_AND_PROTECT_MASK (_BV(PA1) | _BV(PA0))

#define UNUSED_PULLUPS_B (_BV(PB0) | _BV(PB4) | _BV(PB5) | _BV(PB6) | _BV(PB7))
#define UNUSED_PULLUPS_D (_BV(PD0) | _BV(PD1) | _BV(PD3) | _BV(PD4) | _BV(PD5) | _BV(PD6))

#endif
