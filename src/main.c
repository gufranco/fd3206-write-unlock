#include "board.h"
#include "console.h"
#include "edge_counter.h"
#include "hardware/pio.h"
#include "hardware/watchdog.h"
#include "pico/stdlib.h"
#include "write_gate.h"
#include "write_log.h"
#include "write_stage.pio.h"

enum {
    WATCHDOG_TIMEOUT_MS = 250,
    WRITE_STAGE_STATE_MACHINE = 0
};

int main(void) {
    write_gate_start();
    edge_counter_start();
    write_stage_start(WRITE_STAGE_PIO, WRITE_STAGE_STATE_MACHINE, PIN_WRITE_DATA, PIN_HEAD1);
    stdio_init_all();
    console_announce(watchdog_caused_reboot());
    watchdog_enable(WATCHDOG_TIMEOUT_MS, true);
    for (;;) {
        watchdog_update();
        write_gate_reconcile();
        edge_counter_poll();
        write_log_report();
        console_poll();
    }
}
