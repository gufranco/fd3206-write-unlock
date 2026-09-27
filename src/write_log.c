#include "write_log.h"

#include <stdio.h>

#include "edge_counter.h"
#include "hardware/sync.h"
#include "pico/platform.h"

enum {
    MICROSECONDS_PER_SECOND = 1000000,
    TIMER_RESOLUTION_US = 1
};

typedef struct {
    uint32_t sequence;
    bool writing;
    uint16_t start_raw;
    uint16_t end_raw;
    uint32_t start_us;
    uint32_t end_us;
} session_record_t;

static volatile session_record_t record;

static uint32_t tracked_sequence;
static uint32_t tracked_start_edges;
static bool tracked_reported = true;

void __time_critical_func(write_log_mark)(bool writing, uint16_t edge_raw, uint32_t now_us) {
    if (writing == record.writing) {
        return;
    }
    record.writing = writing;
    if (writing) {
        record.sequence = record.sequence + 1;
        record.start_raw = edge_raw;
        record.start_us = now_us;
        return;
    }
    record.end_raw = edge_raw;
    record.end_us = now_us;
}

static session_record_t snapshot(void) {
    const uint32_t interrupts = save_and_disable_interrupts();
    const session_record_t copy = record;
    restore_interrupts(interrupts);
    return copy;
}

static uint32_t edges_per_second(uint32_t edges, uint32_t duration_us) {
    return (uint32_t)(((uint64_t)edges * MICROSECONDS_PER_SECOND) / duration_us);
}

static void print_session(const session_record_t *session, uint32_t edges) {
    const uint32_t duration_us = session->end_us - session->start_us + TIMER_RESOLUTION_US;
    printf("write %lu: %lu edges in %lu us, %lu edges/s\n", (unsigned long)session->sequence,
           (unsigned long)edges, (unsigned long)duration_us,
           (unsigned long)edges_per_second(edges, duration_us));
}

void write_log_report(void) {
    const session_record_t session = snapshot();
    if (session.sequence != tracked_sequence) {
        tracked_sequence = session.sequence;
        tracked_start_edges = edge_counter_resolve(session.start_raw);
        tracked_reported = false;
    }
    if (session.writing || tracked_reported) {
        return;
    }
    tracked_reported = true;
    print_session(&session, edge_counter_resolve(session.end_raw) - tracked_start_edges);
}

uint32_t write_log_session_count(void) {
    return snapshot().sequence;
}

bool write_log_writing(void) {
    return snapshot().writing;
}
