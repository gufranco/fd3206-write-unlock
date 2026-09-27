#include "console.h"

#include <stdio.h>

#include "edge_counter.h"
#include "pico/stdlib.h"
#include "write_log.h"

enum { STATUS_COMMAND = 's' };

static const char FIRMWARE_NAME[] = "fdswriteunlock";
static const char *const START_REASONS[] = {"power-on start", "restarted by watchdog"};

void console_announce(bool watchdog_reset) {
    printf("%s ready, %s\n", FIRMWARE_NAME, START_REASONS[watchdog_reset]);
}

static void print_status(void) {
    printf("%s status: writing=%s writes=%lu edges=%lu\n", FIRMWARE_NAME, write_log_writing() ? "yes" : "no",
           (unsigned long)write_log_session_count(), (unsigned long)edge_counter_total());
}

void console_poll(void) {
    const int received = getchar_timeout_us(0);
    if (received == STATUS_COMMAND) {
        print_status();
    }
}
