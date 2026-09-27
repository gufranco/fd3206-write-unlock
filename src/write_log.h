#ifndef FDSWRITEUNLOCK_WRITE_LOG_H
#define FDSWRITEUNLOCK_WRITE_LOG_H

#include <stdbool.h>
#include <stdint.h>

void write_log_mark(bool writing, uint16_t edge_raw, uint32_t now_us);
void write_log_report(void);
uint32_t write_log_session_count(void);
bool write_log_writing(void);

#endif
