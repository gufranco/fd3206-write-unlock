#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/power.h>
#include <avr/wdt.h>

#include "board.h"
#include "write_stage.h"

static void release_every_pin(void) {
    DDRA = 0;
    PORTA = UNUSED_PULLUPS_A;
    DDRB = 0;
    PORTB = UNUSED_PULLUPS_B;
    DDRD = 0;
    PORTD = UNUSED_PULLUPS_D;
}

int main(void) {
    MCUSR = 0;
    wdt_enable(WDTO_60MS);
    clock_prescale_set(clock_div_1);
    release_every_pin();
    write_stage_start();
    sei();
    for (;;) {
        wdt_reset();
        write_stage_reconcile();
    }
}
