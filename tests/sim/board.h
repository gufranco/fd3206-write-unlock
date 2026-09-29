/* SPDX-FileCopyrightText: 2026 Gustavo Franco <gufranco@users.noreply.github.com> */
/* SPDX-License-Identifier: MIT */

#ifndef FD3206_WRITE_UNLOCK_TEST_BOARD_H
#define FD3206_WRITE_UNLOCK_TEST_BOARD_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    SIGNAL_WRITE_GATE,
    SIGNAL_WRITABLE_MEDIA,
    SIGNAL_READY,
    SIGNAL_WRITE_DATA,
    SIGNAL_COUNT
} board_signal_t;

typedef enum {
    LEVEL_LOW = 0,
    LEVEL_HIGH = 1
} board_level_t;

typedef enum {
    LINES_RELEASED,
    LINES_LINE1_LOW,
    LINES_LINE2_LOW,
    LINES_BOTH_LOW,
    LINES_DRIVEN_HIGH
} board_lines_t;

typedef struct board board_t;

board_t *board_open(const char *mcu, const char *elf_path, uint32_t frequency_hz);
void board_close(board_t *board);
void board_set(board_t *board, board_signal_t signal, board_level_t level);
void board_run_cycles(board_t *board, uint64_t cycles);
uint64_t board_frequency_hz(const board_t *board);
uint64_t board_cycle(const board_t *board);
board_lines_t board_lines(const board_t *board);
uint64_t board_last_line_change_cycle(const board_t *board);
uint8_t board_port_directions(const board_t *board, char port);
uint8_t board_port_levels(const board_t *board, char port);
uint8_t board_clock_prescaler(const board_t *board);
uint8_t board_watchdog_control(const board_t *board);
uint32_t board_reset_count(const board_t *board);
uint8_t board_reset_flags(const board_t *board);
void board_inject_hang(board_t *board);
void board_reset(board_t *board);
void board_wait_startup(board_t *board);
uint16_t board_deepest_stack(void);
uint16_t board_ram_bytes(const board_t *board);

bool coverage_load(const char *instruction_list_path);
uint32_t coverage_report_missing(void);

#endif
