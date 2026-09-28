#include <avr/io.h>

#define rNext r16
#define rAfter r17

    .section .text

    .global INT0_vect
INT0_vect:
    push rNext
    in rNext, _SFR_IO_ADDR(GPIOR0)
    out _SFR_IO_ADDR(DDRB), rNext
    push rAfter
    in rAfter, _SFR_IO_ADDR(GPIOR1)
    out _SFR_IO_ADDR(GPIOR0), rAfter
    out _SFR_IO_ADDR(GPIOR1), rNext
    sbis _SFR_IO_ADDR(GPIOR2), 0
    rjmp set_toggle
    cbi _SFR_IO_ADDR(GPIOR2), 0
    rjmp done
set_toggle:
    sbi _SFR_IO_ADDR(GPIOR2), 0
done:
    pop rAfter
    pop rNext
    reti
