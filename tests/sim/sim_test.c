/* SPDX-FileCopyrightText: 2026 Gustavo Franco <gufranco@users.noreply.github.com> */
/* SPDX-License-Identifier: MIT */

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "scenario.h"

static bool heads_released_under(write_conditions_t conditions) {
    board_t *board = open_board();
    apply_conditions(board, conditions);

    const head_census_t census = census_heads(board, IDLE_EDGE_COUNT, HALF_BIT_CELL_NS);

    board_close(board);
    CHECK(census.released == IDLE_EDGE_COUNT);
    return true;
}

static bool heads_released_while_write_gate_inactive(void) {
    return heads_released_under(GATE_CLOSED);
}

static bool heads_released_while_disk_not_writable(void) {
    return heads_released_under(NOT_WRITABLE);
}

static bool heads_released_while_drive_not_ready(void) {
    return heads_released_under(NOT_READY);
}

static bool one_head_low_and_alternating_at_fastest_data_rate(void) {
    board_t *board = open_board();
    apply_conditions(board, WRITING);
    write_pulse(board, FAST_HALF_BIT_CELL_NS);

    const head_census_t census = census_heads(board, WRITE_EDGE_COUNT, FAST_HALF_BIT_CELL_NS);

    board_close(board);
    CHECK(census.one_head_low == WRITE_EDGE_COUNT);
    CHECK(census.repeats == 0);
    CHECK(census.invalid == 0);
    CHECK(census.driven_high == 0);
    return true;
}

static bool rising_edge_leaves_heads_unchanged(void) {
    board_t *board = open_board();
    apply_conditions(board, WRITING);
    write_pulse(board, HALF_BIT_CELL_NS);
    const board_heads_t before_fall = board_heads(board);
    const uint64_t fall_cycle = board_cycle(board);

    write_pulse(board, HALF_BIT_CELL_NS);

    const board_heads_t after_rise = board_heads(board);
    const uint64_t last_change_after_fall = board_last_head_change_cycle(board) - fall_cycle;
    board_close(board);
    CHECK(one_head_low(before_fall));
    CHECK(one_head_low(after_rise));
    CHECK(after_rise != before_fall);
    CHECK(last_change_after_fall <= EDGE_OUTPUT_LATENCY_CYCLES);
    return true;
}

static uint64_t worst_gate_response_ns;

static void record_gate_response(uint64_t elapsed) {
    if (elapsed > worst_gate_response_ns) {
        worst_gate_response_ns = elapsed;
    }
}

static bool release_at_phase(board_signal_t signal, uint64_t phase) {
    board_t *board = open_board();
    apply_conditions(board, WRITING);
    write_pulse(board, HALF_BIT_CELL_NS);
    board_run_cycles(board, phase);
    const board_heads_t before = board_heads(board);

    const uint64_t elapsed = response_ns(board, signal, LEVEL_HIGH);

    const board_heads_t after = board_heads(board);
    board_close(board);
    record_gate_response(elapsed);
    CHECK(one_head_low(before));
    CHECK(after == HEADS_RELEASED);
    CHECK(elapsed <= GATE_RESPONSE_LIMIT_NS);
    return true;
}

static bool heads_release_when_condition_goes_high(board_signal_t signal) {
    bool passed = true;
    for (uint64_t phase = 0; phase < LOOP_PHASE_SWEEP_CYCLES; phase++) {
        passed = release_at_phase(signal, phase) && passed;
    }
    return passed;
}

static bool heads_release_when_write_gate_closes(void) {
    return heads_release_when_condition_goes_high(SIGNAL_WRITE_GATE);
}

static bool heads_release_when_disk_becomes_not_writable(void) {
    return heads_release_when_condition_goes_high(SIGNAL_WRITABLE_MEDIA);
}

static bool heads_release_when_ready_drops(void) {
    return heads_release_when_condition_goes_high(SIGNAL_READY);
}

static bool engage_at_phase(uint64_t phase) {
    board_t *board = open_board();
    apply_conditions(board, GATE_CLOSED);
    board_run_cycles(board, phase);

    const uint64_t elapsed = response_ns(board, SIGNAL_WRITE_GATE, LEVEL_LOW);

    const board_heads_t heads = board_heads(board);
    board_close(board);
    record_gate_response(elapsed);
    CHECK(one_head_low(heads));
    CHECK(elapsed <= GATE_RESPONSE_LIMIT_NS);
    return true;
}

static bool one_head_engages_when_write_gate_opens(void) {
    bool passed = true;
    for (uint64_t phase = 0; phase < LOOP_PHASE_SWEEP_CYCLES; phase++) {
        passed = engage_at_phase(phase) && passed;
    }
    return passed;
}

static uint64_t worst_loaded_gate_response_ns;

static uint64_t gate_change_during_data(board_t *board, board_level_t gate, bool (*reached)(board_heads_t)) {
    census_heads(board, IDLE_EDGE_COUNT, FAST_HALF_BIT_CELL_NS);
    const uint64_t start = board_cycle(board);
    board_set(board, SIGNAL_WRITE_GATE, gate);
    for (uint32_t edge = 0; edge < LOADED_EDGE_BUDGET && !reached(board_heads(board)); edge++) {
        write_pulse(board, FAST_HALF_BIT_CELL_NS);
    }
    const uint64_t elapsed = cycles_to_ns(board, board_last_head_change_cycle(board) - start);
    if (elapsed > worst_loaded_gate_response_ns) {
        worst_loaded_gate_response_ns = elapsed;
    }
    return elapsed;
}

static bool is_released(board_heads_t heads) {
    return heads == HEADS_RELEASED;
}

static bool heads_release_when_write_gate_closes_during_data(void) {
    board_t *board = open_board();
    apply_conditions(board, WRITING);

    const uint64_t elapsed = gate_change_during_data(board, LEVEL_HIGH, is_released);

    const head_census_t after = census_heads(board, IDLE_EDGE_COUNT, FAST_HALF_BIT_CELL_NS);
    board_close(board);
    CHECK(elapsed <= LOADED_GATE_RESPONSE_LIMIT_NS);
    CHECK(after.released == IDLE_EDGE_COUNT);
    return true;
}

static bool one_head_engages_when_write_gate_opens_during_data(void) {
    board_t *board = open_board();
    apply_conditions(board, GATE_CLOSED);

    const uint64_t elapsed = gate_change_during_data(board, LEVEL_LOW, one_head_low);

    const head_census_t after = census_heads(board, WRITE_EDGE_COUNT, FAST_HALF_BIT_CELL_NS);
    board_close(board);
    CHECK(elapsed <= LOADED_GATE_RESPONSE_LIMIT_NS);
    CHECK(after.one_head_low == WRITE_EDGE_COUNT);
    CHECK(after.repeats == 0);
    CHECK(after.driven_high == 0);
    return true;
}

static bool heads_end_released_after_write_gate_glitch(void) {
    board_t *board = open_board();
    apply_conditions(board, GATE_CLOSED);
    board_set(board, SIGNAL_WRITE_GATE, LEVEL_LOW);
    run_ns(board, GLITCH_NS);

    board_set(board, SIGNAL_WRITE_GATE, LEVEL_HIGH);
    run_ns(board, SETTLE_NS);

    const board_heads_t heads = board_heads(board);
    board_close(board);
    CHECK(heads == HEADS_RELEASED);
    return true;
}

static bool startup_drives_no_pin_and_pulls_up_every_input(void) {
    board_t *board = open_board();

    const uint8_t driven =
        board_port_directions(board, 'A') | board_port_directions(board, 'B') | board_port_directions(board, 'D');

    const uint8_t latch_a = board_port_levels(board, 'A');
    const uint8_t latch_b = board_port_levels(board, 'B');
    const uint8_t latch_d = board_port_levels(board, 'D');
    board_close(board);
    CHECK(driven == 0);
    CHECK((latch_a & CONDITION_PIN_BITS_A) == CONDITION_PIN_BITS_A);
    CHECK((latch_b & READY_PIN_BIT_B) == READY_PIN_BIT_B);
    CHECK((latch_d & WRITE_DATA_PIN_BIT_D) == WRITE_DATA_PIN_BIT_D);
    CHECK((latch_b & HEAD_PIN_BITS_B) == 0);
    return true;
}

static bool heads_release_on_reset_mid_write_and_writing_resumes(void) {
    board_t *board = open_board();
    apply_conditions(board, WRITING);
    census_heads(board, IDLE_EDGE_COUNT, HALF_BIT_CELL_NS);
    const board_heads_t before = board_heads(board);

    board_reset(board);

    const board_heads_t at_reset = board_heads(board);
    board_wait_startup(board);
    const head_census_t after = census_heads(board, IDLE_EDGE_COUNT, HALF_BIT_CELL_NS);
    board_close(board);
    CHECK(one_head_low(before));
    CHECK(at_reset == HEADS_RELEASED);
    CHECK(after.one_head_low == IDLE_EDGE_COUNT);
    CHECK(after.repeats == 0);
    CHECK(after.driven_high == 0);
    return true;
}

static bool clock_is_undivided_and_watchdog_is_armed(void) {
    board_t *board = open_board();

    const uint8_t prescaler = board_clock_prescaler(board);

    const uint8_t watchdog = board_watchdog_control(board);
    board_close(board);
    CHECK(prescaler == 0);
    CHECK(watchdog == WATCHDOG_ARMED_60MS);
    return true;
}

static bool long_low_pulse_changes_the_heads_once(void) {
    board_t *board = open_board();
    apply_conditions(board, WRITING);
    write_pulse(board, HALF_BIT_CELL_NS);
    const board_heads_t before_fall = board_heads(board);
    const uint64_t fall_cycle = board_cycle(board);

    board_set(board, SIGNAL_WRITE_DATA, LEVEL_LOW);
    run_ns(board, SETTLE_NS);

    const board_heads_t while_low = board_heads(board);
    const uint64_t last_change_after_fall = board_last_head_change_cycle(board) - fall_cycle;
    board_close(board);
    CHECK(one_head_low(while_low));
    CHECK(while_low != before_fall);
    CHECK(last_change_after_fall <= EDGE_OUTPUT_LATENCY_CYCLES);
    return true;
}

static board_heads_t heads_after_edge_at_gate_open(uint64_t delay_cycles) {
    board_t *board = open_board();
    apply_conditions(board, GATE_CLOSED);
    census_heads(board, IDLE_EDGE_COUNT, HALF_BIT_CELL_NS);
    board_set(board, SIGNAL_WRITE_GATE, LEVEL_LOW);
    board_run_cycles(board, delay_cycles);
    write_pulse(board, HALF_BIT_CELL_NS);
    run_ns(board, SETTLE_NS);
    const board_heads_t heads = board_heads(board);
    board_close(board);
    return heads;
}

static bool edge_during_gate_opening_keeps_the_head_phase(void) {
    uint32_t wrong_phase = 0;

    for (uint64_t delay = 0; delay < GATE_EDGE_SWEEP_CYCLES; delay++) {
        wrong_phase += heads_after_edge_at_gate_open(delay) != HEADS_HEAD2_LOW;
    }

    CHECK(wrong_phase == 0);
    return true;
}

static board_heads_t heads_at_gate_open_after(uint32_t released_edges) {
    board_t *board = open_board();
    apply_conditions(board, GATE_CLOSED);
    census_heads(board, released_edges, HALF_BIT_CELL_NS);
    apply_conditions(board, WRITING);
    const board_heads_t heads = board_heads(board);
    board_close(board);
    return heads;
}

static bool first_head_follows_the_edge_count_while_released(void) {
    const board_heads_t after_even = heads_at_gate_open_after(IDLE_EDGE_COUNT);

    const board_heads_t after_odd = heads_at_gate_open_after(IDLE_EDGE_COUNT + 1U);

    CHECK(after_even == HEADS_HEAD1_LOW);
    CHECK(after_odd == HEADS_HEAD2_LOW);
    return true;
}

static bool hung_firmware_is_reset_by_the_watchdog_and_writing_resumes(void) {
    board_t *board = open_board();
    apply_conditions(board, WRITING);
    census_heads(board, IDLE_EDGE_COUNT, HALF_BIT_CELL_NS);

    board_inject_hang(board);
    run_ns(board, WATCHDOG_STALL_NS);

    const uint32_t resets = board_reset_count(board);
    const uint8_t reset_flags = board_reset_flags(board);
    const head_census_t after = census_heads(board, IDLE_EDGE_COUNT, HALF_BIT_CELL_NS);
    board_close(board);
    CHECK(resets == 1);
    CHECK((reset_flags & WATCHDOG_RESET_FLAG) != 0);
    CHECK(after.one_head_low == IDLE_EDGE_COUNT);
    CHECK(after.repeats == 0);
    return true;
}

static bool long_write_never_trips_the_watchdog(void) {
    board_t *board = open_board();
    apply_conditions(board, WRITING);

    const head_census_t census = census_heads(board, SOAK_NS / HALF_BIT_CELL_NS, HALF_BIT_CELL_NS);

    const uint32_t resets = board_reset_count(board);
    board_close(board);
    CHECK(resets == 0);
    CHECK(census.one_head_low == SOAK_NS / HALF_BIT_CELL_NS);
    return true;
}

typedef struct {
    const char *name;
    bool (*run)(void);
} test_case_t;

#define TEST_CASE(function) {#function, function}

static const test_case_t TESTS[] = {
    TEST_CASE(heads_released_while_write_gate_inactive),
    TEST_CASE(heads_released_while_disk_not_writable),
    TEST_CASE(heads_released_while_drive_not_ready),
    TEST_CASE(one_head_low_and_alternating_at_fastest_data_rate),
    TEST_CASE(rising_edge_leaves_heads_unchanged),
    TEST_CASE(first_head_follows_the_edge_count_while_released),
    TEST_CASE(long_low_pulse_changes_the_heads_once),
    TEST_CASE(edge_during_gate_opening_keeps_the_head_phase),
    TEST_CASE(heads_release_when_write_gate_closes),
    TEST_CASE(heads_release_when_disk_becomes_not_writable),
    TEST_CASE(heads_release_when_ready_drops),
    TEST_CASE(one_head_engages_when_write_gate_opens),
    TEST_CASE(heads_release_when_write_gate_closes_during_data),
    TEST_CASE(one_head_engages_when_write_gate_opens_during_data),
    TEST_CASE(heads_end_released_after_write_gate_glitch),
    TEST_CASE(startup_drives_no_pin_and_pulls_up_every_input),
    TEST_CASE(clock_is_undivided_and_watchdog_is_armed),
    TEST_CASE(long_write_never_trips_the_watchdog),
    TEST_CASE(hung_firmware_is_reset_by_the_watchdog_and_writing_resumes),
    TEST_CASE(heads_release_on_reset_mid_write_and_writing_resumes),
};

int main(int argc, char **argv) {
    if (argc != 5) {
        fprintf(stderr, "usage: %s <mcu> <firmware.elf> <instructions.insn> <clock-hz>\n", argv[0]);
        return EXIT_FAILURE;
    }
    const uint32_t frequency_hz = (uint32_t)strtoul(argv[4], NULL, 10);
    scenario_configure(argv[1], argv[2], frequency_hz);
    if (!coverage_load(argv[3])) {
        return EXIT_FAILURE;
    }
    const size_t test_count = sizeof TESTS / sizeof TESTS[0];
    int failures = 0;
    for (size_t index = 0; index < test_count; index++) {
        const bool passed = TESTS[index].run();
        printf("%s %s\n", passed ? "PASS" : "FAIL", TESTS[index].name);
        failures += !passed;
    }
    const uint32_t missing = coverage_report_missing();
    printf("worst gate response over %d loop phases: %" PRIu64 " ns of %d ns allowed\n", LOOP_PHASE_SWEEP_CYCLES,
           worst_gate_response_ns, GATE_RESPONSE_LIMIT_NS);
    printf("worst gate response under fastest data: %" PRIu64 " ns of %d ns allowed\n", worst_loaded_gate_response_ns,
           LOADED_GATE_RESPONSE_LIMIT_NS);
    printf("%s at %" PRIu32 " Hz: %zu tests, %d failed, %u firmware instructions never executed\n", argv[1],
           frequency_hz, test_count, failures, missing);
    const uint16_t stack = board_deepest_stack();
    printf("deepest stack: %u bytes of %d allowed\n", (unsigned)stack, STACK_LIMIT_BYTES);
    const bool stack_fits = stack <= STACK_LIMIT_BYTES;
    return failures == 0 && missing == 0 && stack_fits ? EXIT_SUCCESS : EXIT_FAILURE;
}
