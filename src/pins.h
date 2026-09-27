#ifndef FDSWRITEUNLOCK_PINS_H
#define FDSWRITEUNLOCK_PINS_H

#include <avr/io.h>

#define WRITE_DATA_BIT PD2
#define HEAD1_BIT PB3
#define HEAD2_BIT PB2

#if defined(__AVR_ATtiny2313A__) || defined(__AVR_ATtiny4313__)

#define WRITE_GATE_PIN PINA
#define WRITE_GATE_BIT PA1
#define WRITE_PROTECT_PIN PINA
#define WRITE_PROTECT_BIT PA0
#define READY_PIN PINB
#define READY_BIT PB1

#define UNUSED_PULLUPS_A 0
#define UNUSED_PULLUPS_B (_BV(PB0) | _BV(PB4) | _BV(PB5) | _BV(PB6) | _BV(PB7))
#define UNUSED_PULLUPS_D (_BV(PD0) | _BV(PD1) | _BV(PD3) | _BV(PD4) | _BV(PD5) | _BV(PD6))

#define INT0_SENSE_REG MCUCR
#define INT0_ENABLE_REG GIMSK
#define WATCHDOG_REG WDTCR

#elif defined(__AVR_ATmega328P__)

#define WRITE_GATE_PIN PINB
#define WRITE_GATE_BIT PB4
#define WRITE_PROTECT_PIN PINB
#define WRITE_PROTECT_BIT PB0
#define READY_PIN PINB
#define READY_BIT PB1

#define UNUSED_PULLUPS_B 0
#define UNUSED_PULLUPS_C (_BV(PC0) | _BV(PC1) | _BV(PC2) | _BV(PC3) | _BV(PC4) | _BV(PC5))
#define UNUSED_PULLUPS_D (_BV(PD0) | _BV(PD1) | _BV(PD3) | _BV(PD4) | _BV(PD5) | _BV(PD6) | _BV(PD7))

#define INT0_SENSE_REG EICRA
#define INT0_ENABLE_REG EIMSK
#define WATCHDOG_REG WDTCSR

#else
#error "Unsupported MCU: build for attiny2313a, attiny4313 or atmega328p"
#endif

#define HEAD1_MASK _BV(HEAD1_BIT)
#define HEAD2_MASK _BV(HEAD2_BIT)

#endif
