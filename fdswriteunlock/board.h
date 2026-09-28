#ifndef FDSWRITEUNLOCK_BOARD_H
#define FDSWRITEUNLOCK_BOARD_H

#include <avr/io.h>

#if !defined(__AVR_ATtiny2313A__) && !defined(__AVR_ATtiny4313__)
#error "fdswriteunlock supports the ATtiny2313A and ATtiny4313 only"
#endif

#define HEAD1_BIT PB2
#define HEAD2_BIT PD5
#define WRITE_CONDITION_PINS (_BV(PB0) | _BV(PB1) | _BV(PB3))

#define UNUSED_PULLUPS_A (_BV(PA0) | _BV(PA1))
#define UNUSED_PULLUPS_B (_BV(PB4) | _BV(PB5) | _BV(PB6) | _BV(PB7))
#define UNUSED_PULLUPS_D (_BV(PD0) | _BV(PD1) | _BV(PD2) | _BV(PD3) | _BV(PD6))

#endif
