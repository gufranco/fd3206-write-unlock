#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/power.h>
#include <avr/wdt.h>

#include "fdswu/assert.h"
#include "fdswu/conditions.h"
#include "fdswu/heads.h"
#include "fdswu/pins.h"
#include "port/registers.h"

static constexpr uint8_t TOGGLE_MASK = (uint8_t)(1U << FDSWU_TOGGLE_BIT);
static constexpr uint8_t FALLING_EDGE_SENSE = (uint8_t)(1U << ISC01);
static constexpr uint8_t EDGE_FLAG = (uint8_t)(1U << INTF0);
static constexpr uint8_t EDGE_INTERRUPT = (uint8_t)(1U << INT0);

static bool toggled(void) {
    return (FDSWU_TOGGLE_STATE & TOGGLE_MASK) != 0U;
}

static void apply_heads(bool allowed) {
    const fdswu_heads_plan_t plan = fdswu_heads_plan(toggled(), allowed);
    FDSWU_HEAD_DIRECTION = plan.now;
    FDSWU_NEXT_EDGE_HEADS = plan.next_edge;
    FDSWU_LATER_EDGE_HEADS = plan.later_edge;
    FDSWU_ASSERT((FDSWU_HEAD_DIRECTION & (uint8_t)~FDSWU_HEADS_MASK) == 0U);
    FDSWU_ASSERT(allowed || (FDSWU_HEAD_DIRECTION == 0U));
}

static void start(void) {
    MCUSR = 0U;
    wdt_enable(WDTO_60MS);
    clock_prescale_set(clock_div_1);
    PORTB = FDSWU_UNUSED_PULLUPS_B;
    PORTD = FDSWU_UNUSED_PULLUPS_D;
    apply_heads(false);
    MCUCR = FALLING_EDGE_SENSE;
    EIFR = EDGE_FLAG;
    GIMSK = EDGE_INTERRUPT;
    FDSWU_ASSERT(FDSWU_HEAD_DIRECTION == 0U);
    FDSWU_ASSERT((GIMSK & EDGE_INTERRUPT) != 0U);
    sei();
}

int main(void) {
    start();
    bool was_allowed = false;
    for (;;) {
        wdt_reset();
        const bool allowed = fdswu_conditions_allow(PINA, PINB);
        if (allowed != was_allowed) {
            cli();
            apply_heads(allowed);
            sei();
            was_allowed = allowed;
        }
    }
}
