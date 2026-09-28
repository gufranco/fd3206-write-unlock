#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/power.h>
#include <avr/wdt.h>
#include <stdbool.h>
#include <util/atomic.h>

#include "board.h"

#define NEXT_EDGE_HEADS GPIOR0
#define LATER_EDGE_HEADS GPIOR1
#define TOGGLE_STATE GPIOR2
#define TOGGLE_BIT 0x01

static bool writing_allowed(void) {
    return (PINA & GATE_AND_PROTECT_MASK) == 0 && (PINB & READY_MASK) == 0;
}

static void set_heads(bool allowed) {
    const bool toggled = (TOGGLE_STATE & TOGGLE_BIT) != 0;
    const uint8_t current = toggled ? HEAD2_MASK : HEAD1_MASK;
    const uint8_t other = toggled ? HEAD1_MASK : HEAD2_MASK;
    const uint8_t enable = allowed ? 0xFF : 0x00;
    DDRB = current & enable;
    NEXT_EDGE_HEADS = other & enable;
    LATER_EDGE_HEADS = current & enable;
}

int main(void) {
    MCUSR = 0;
    wdt_enable(WDTO_60MS);
    clock_prescale_set(clock_div_1);
    PORTB = UNUSED_PULLUPS_B;
    PORTD = UNUSED_PULLUPS_D;
    set_heads(false);
    MCUCR = _BV(ISC01);
    EIFR = _BV(INTF0);
    GIMSK = _BV(INT0);
    sei();
    bool was_allowed = false;
    for (;;) {
        wdt_reset();
        const bool allowed = writing_allowed();
        if (allowed != was_allowed) {
            ATOMIC_BLOCK(ATOMIC_FORCEON) {
                set_heads(allowed);
            }
            was_allowed = allowed;
        }
    }
}
