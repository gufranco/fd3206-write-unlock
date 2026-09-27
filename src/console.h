#ifndef FDSWRITEUNLOCK_CONSOLE_H
#define FDSWRITEUNLOCK_CONSOLE_H

#include <stdbool.h>

void console_announce(bool watchdog_reset);
void console_poll(void);

#endif
