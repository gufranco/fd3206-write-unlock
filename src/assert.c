#include "fdswu/assert.h"

#include <avr/interrupt.h>
#include <avr/wdt.h>

#include "port/registers.h"

void fdswu_assert_fail(void) {
    cli();
    FDSWU_HEAD_DIRECTION = 0U;
    wdt_disable();
    for (;;) {
    }
}
