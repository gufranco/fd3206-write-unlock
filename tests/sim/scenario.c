/* SPDX-FileCopyrightText: 2026 Gustavo Franco <gufranco@users.noreply.github.com> */
/* SPDX-License-Identifier: MIT */

#include "scenario.h"

#include <stdlib.h>

const write_conditions_t WRITING = {LEVEL_LOW, LEVEL_LOW, LEVEL_LOW};
const write_conditions_t GATE_CLOSED = {LEVEL_HIGH, LEVEL_LOW, LEVEL_LOW};
const write_conditions_t NOT_WRITABLE = {LEVEL_LOW, LEVEL_HIGH, LEVEL_LOW};
const write_conditions_t NOT_READY = {LEVEL_LOW, LEVEL_LOW, LEVEL_HIGH};

static const char *target_mcu;
static const char *target_elf;
static uint32_t target_frequency_hz;

static const uint64_t NS_PER_S = 1000000000U;

void scenario_configure(const char *mcu, const char *elf_path, uint32_t frequency_hz) {
    target_mcu = mcu;
    target_elf = elf_path;
    target_frequency_hz = frequency_hz;
}

board_t *open_board(void) {
    board_t *board = board_open(target_mcu, target_elf, target_frequency_hz);
    if (board == NULL) {
        exit(EXIT_FAILURE);
    }
    return board;
}

void run_ns(board_t *board, uint64_t ns) {
    board_run_cycles(board, ns * board_frequency_hz(board) / NS_PER_S);
}

uint64_t cycles_to_ns(const board_t *board, uint64_t cycles) {
    return cycles * NS_PER_S / board_frequency_hz(board);
}

void apply_conditions(board_t *board, write_conditions_t conditions) {
    board_set(board, SIGNAL_WRITE_GATE, conditions.write_gate);
    board_set(board, SIGNAL_WRITABLE_MEDIA, conditions.writable_media);
    board_set(board, SIGNAL_READY, conditions.ready);
    run_ns(board, SETTLE_NS);
}

void write_pulse(board_t *board, uint64_t period_ns) {
    board_set(board, SIGNAL_WRITE_DATA, LEVEL_LOW);
    run_ns(board, WRITE_PULSE_LOW_NS);
    board_set(board, SIGNAL_WRITE_DATA, LEVEL_HIGH);
    run_ns(board, period_ns - WRITE_PULSE_LOW_NS);
}

bool one_head_low(board_heads_t heads) {
    return heads == HEADS_HEAD1_LOW || heads == HEADS_HEAD2_LOW;
}

head_census_t census_heads(board_t *board, uint32_t edges, uint64_t period_ns) {
    head_census_t census = {0};
    board_heads_t previous = board_heads(board);
    for (uint32_t edge = 0; edge < edges; edge++) {
        write_pulse(board, period_ns);
        const board_heads_t heads = board_heads(board);
        census.released += heads == HEADS_RELEASED;
        census.one_head_low += one_head_low(heads);
        census.repeats += one_head_low(heads) && heads == previous;
        census.invalid += heads != HEADS_RELEASED && !one_head_low(heads);
        census.driven_high +=
            (board_port_directions(board, 'B') & board_port_levels(board, 'B') & HEAD_PIN_BITS_B) != 0;
        previous = heads;
    }
    return census;
}

uint64_t response_ns(board_t *board, board_signal_t signal, board_level_t level) {
    const uint64_t start = board_cycle(board);
    board_set(board, signal, level);
    run_ns(board, SETTLE_NS);
    return cycles_to_ns(board, board_last_head_change_cycle(board) - start);
}
