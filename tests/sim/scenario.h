/* SPDX-FileCopyrightText: 2026 Gustavo Franco <gufranco@users.noreply.github.com> */
/* SPDX-License-Identifier: MIT */

#ifndef FD3206_WRITE_UNLOCK_TEST_SCENARIO_H
#define FD3206_WRITE_UNLOCK_TEST_SCENARIO_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "board.h"

enum {
    HALF_BIT_CELL_NS = 5187,
    FAST_HALF_BIT_CELL_NS = 4715,
    WRITE_PULSE_LOW_NS = 1000,
    SETTLE_NS = 20000,
    GATE_RESPONSE_LIMIT_NS = 10000,
    LOADED_GATE_RESPONSE_LIMIT_NS = 100000,
    LOADED_EDGE_BUDGET = 64,
    STACK_LIMIT_BYTES = 32,
    LOOP_PHASE_SWEEP_CYCLES = 64,
    IDLE_EDGE_COUNT = 100,
    EDGE_OUTPUT_LATENCY_CYCLES = 13,
    GATE_EDGE_SWEEP_CYCLES = 96,
    WRITE_EDGE_COUNT = 1000,
    SOAK_NS = 600000000,
    GLITCH_NS = 250,
    WATCHDOG_ARMED_60MS = 0x0A,
    WATCHDOG_RESET_FLAG = 0x08,
    WATCHDOG_STALL_NS = 100000000,
    WRITE_LINE_PIN_BITS_B = 0x0C,
    READY_PIN_BIT_B = 0x02,
    CONDITION_PIN_BITS_A = 0x03,
    WRITE_DATA_PIN_BIT_D = 0x04
};

typedef struct {
    board_level_t write_gate;
    board_level_t writable_media;
    board_level_t ready;
} write_conditions_t;

extern const write_conditions_t WRITING;
extern const write_conditions_t GATE_CLOSED;
extern const write_conditions_t NOT_WRITABLE;
extern const write_conditions_t NOT_READY;

typedef struct {
    uint32_t released;
    uint32_t one_line_low;
    uint32_t repeats;
    uint32_t invalid;
    uint32_t driven_high;
} line_census_t;

#define CHECK(condition)                                                                                               \
    do {                                                                                                               \
        if (!(condition)) {                                                                                            \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition);                              \
            return false;                                                                                              \
        }                                                                                                              \
    } while (0)

void scenario_configure(const char *mcu, const char *elf_path, uint32_t frequency_hz);
board_t *open_board(void);
void run_ns(board_t *board, uint64_t ns);
uint64_t cycles_to_ns(const board_t *board, uint64_t cycles);
void apply_conditions(board_t *board, write_conditions_t conditions);
void write_pulse(board_t *board, uint64_t period_ns);
bool one_line_low(board_lines_t lines);
line_census_t census_lines(board_t *board, uint32_t edges, uint64_t period_ns);
uint64_t response_ns(board_t *board, board_signal_t signal, board_level_t level);

#endif
