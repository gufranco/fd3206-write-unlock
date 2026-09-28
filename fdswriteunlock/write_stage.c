#include "write_stage.h"

#include <avr/interrupt.h>
#include <stdbool.h>
#include <util/atomic.h>

#include "board.h"

#define TOGGLE_BOTH_OUTPUTS_ON_MATCH (_BV(COM0A0) | _BV(COM0B0))
#define CLEAR_ON_MATCH_MODE _BV(WGM01)
#define CLOCK_FROM_T0_FALLING_EDGE (_BV(CS02) | _BV(CS01))
#define MATCH_EVERY_EDGE 0

static bool write_conditions_met(void) {
    return (PINB & WRITE_CONDITION_PINS) == 0;
}

static void apply_write_conditions(void) {
    if (write_conditions_met()) {
        DDRB |= _BV(HEAD1_BIT);
        DDRD |= _BV(HEAD2_BIT);
        return;
    }
    DDRB &= (uint8_t)~_BV(HEAD1_BIT);
    DDRD &= (uint8_t)~_BV(HEAD2_BIT);
}

ISR(PCINT0_vect) {
    apply_write_conditions();
}

static void start_head_toggle(void) {
    OCR0A = MATCH_EVERY_EDGE;
    OCR0B = MATCH_EVERY_EDGE;
    TCCR0A = TOGGLE_BOTH_OUTPUTS_ON_MATCH | CLEAR_ON_MATCH_MODE;
    TCCR0B = _BV(FOC0B);
    TCCR0B = CLOCK_FROM_T0_FALLING_EDGE;
}

static void start_condition_watch(void) {
    PCMSK = WRITE_CONDITION_PINS;
    GIFR = _BV(PCIF0);
    GIMSK |= _BV(PCIE0);
}

void write_stage_start(void) {
    start_head_toggle();
    start_condition_watch();
    write_stage_reconcile();
}

void write_stage_reconcile(void) {
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        apply_write_conditions();
    }
}
