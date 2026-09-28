#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "board.h"

enum {
    NS_PER_US = 1000,
    HALF_BIT_CELL_NS = 5187,
    FAST_HALF_BIT_CELL_NS = 4715,
    WRITE_PULSE_LOW_NS = 1000,
    SETTLE_NS = 20000,
    GATE_RESPONSE_LIMIT_NS = 10000,
    IDLE_EDGE_COUNT = 100,
    WRITE_EDGE_COUNT = 1000,
    SOAK_NS = 600000000,
    GLITCH_NS = 250,
    WATCHDOG_ENABLE_BIT = 0x08,
    SIGNAL_PIN_BITS_B = 0x0E,
    SIGNAL_PIN_BITS_A = 0x03,
    WRITE_DATA_PIN_BIT_D = 0x04
};

typedef struct {
    board_level_t write_gate;
    board_level_t write_protect;
    board_level_t ready;
} write_conditions_t;

static const write_conditions_t WRITING = {LEVEL_LOW, LEVEL_LOW, LEVEL_LOW};
static const write_conditions_t GATE_CLOSED = {LEVEL_HIGH, LEVEL_LOW, LEVEL_LOW};
static const write_conditions_t PROTECTED = {LEVEL_LOW, LEVEL_HIGH, LEVEL_LOW};
static const write_conditions_t NOT_READY = {LEVEL_LOW, LEVEL_LOW, LEVEL_HIGH};

static const char *target_mcu;
static const char *target_elf;

#define CHECK(condition)                                                                   \
    do {                                                                                   \
        if (!(condition)) {                                                                \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition); \
            return false;                                                                  \
        }                                                                                  \
    } while (0)

static board_t *open_board(void) {
    board_t *board = board_open(target_mcu, target_elf);
    if (board == NULL) {
        exit(EXIT_FAILURE);
    }
    return board;
}

static void run_ns(board_t *board, uint64_t ns) {
    board_run_cycles(board, ns * board_cycles_per_us(board) / NS_PER_US);
}

static uint64_t cycles_to_ns(const board_t *board, uint64_t cycles) {
    return cycles * NS_PER_US / board_cycles_per_us(board);
}

static void apply_conditions(board_t *board, write_conditions_t conditions) {
    board_set(board, SIGNAL_WRITE_GATE, conditions.write_gate);
    board_set(board, SIGNAL_WRITE_PROTECT, conditions.write_protect);
    board_set(board, SIGNAL_READY, conditions.ready);
    run_ns(board, SETTLE_NS);
}

static void write_pulse(board_t *board, uint64_t period_ns) {
    board_set(board, SIGNAL_WRITE_DATA, LEVEL_LOW);
    run_ns(board, WRITE_PULSE_LOW_NS);
    board_set(board, SIGNAL_WRITE_DATA, LEVEL_HIGH);
    run_ns(board, period_ns - WRITE_PULSE_LOW_NS);
}

typedef struct {
    uint32_t released;
    uint32_t one_head_low;
    uint32_t repeats;
    uint32_t invalid;
    uint32_t driven_high;
} head_census_t;

static bool one_head_low(board_heads_t heads) {
    return heads == HEADS_HEAD1_LOW || heads == HEADS_HEAD2_LOW;
}

static head_census_t census_heads(board_t *board, uint32_t edges, uint64_t period_ns) {
    head_census_t census = {0};
    board_heads_t previous = board_heads(board);
    for (uint32_t edge = 0; edge < edges; edge++) {
        write_pulse(board, period_ns);
        const board_heads_t heads = board_heads(board);
        census.released += heads == HEADS_RELEASED;
        census.one_head_low += one_head_low(heads);
        census.repeats += one_head_low(heads) && heads == previous;
        census.invalid += heads != HEADS_RELEASED && !one_head_low(heads);
        census.driven_high += (board_port_levels(board, 'B') & SIGNAL_PIN_BITS_B) != 0;
        previous = heads;
    }
    return census;
}

static uint64_t response_ns(board_t *board, board_signal_t signal, board_level_t level) {
    const uint64_t start = board_cycle(board);
    board_set(board, signal, level);
    run_ns(board, SETTLE_NS);
    return cycles_to_ns(board, board_last_head_change_cycle(board) - start);
}

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

static bool heads_released_while_disk_write_protected(void) {
    return heads_released_under(PROTECTED);
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
    board_set(board, SIGNAL_WRITE_DATA, LEVEL_LOW);
    run_ns(board, WRITE_PULSE_LOW_NS);
    const board_heads_t after_fall = board_heads(board);

    board_set(board, SIGNAL_WRITE_DATA, LEVEL_HIGH);
    run_ns(board, HALF_BIT_CELL_NS);

    const board_heads_t after_rise = board_heads(board);
    board_close(board);
    CHECK(one_head_low(after_fall));
    CHECK(after_rise == after_fall);
    return true;
}

static bool heads_release_when_condition_goes_high(board_signal_t signal) {
    board_t *board = open_board();
    apply_conditions(board, WRITING);
    write_pulse(board, HALF_BIT_CELL_NS);
    const board_heads_t before = board_heads(board);

    const uint64_t elapsed = response_ns(board, signal, LEVEL_HIGH);

    const board_heads_t after = board_heads(board);
    board_close(board);
    CHECK(one_head_low(before));
    CHECK(after == HEADS_RELEASED);
    CHECK(elapsed <= GATE_RESPONSE_LIMIT_NS);
    return true;
}

static bool heads_release_when_write_gate_closes(void) {
    return heads_release_when_condition_goes_high(SIGNAL_WRITE_GATE);
}

static bool heads_release_when_write_protect_asserts(void) {
    return heads_release_when_condition_goes_high(SIGNAL_WRITE_PROTECT);
}

static bool heads_release_when_ready_drops(void) {
    return heads_release_when_condition_goes_high(SIGNAL_READY);
}

static bool one_head_engages_when_write_gate_opens(void) {
    board_t *board = open_board();
    apply_conditions(board, GATE_CLOSED);

    const uint64_t elapsed = response_ns(board, SIGNAL_WRITE_GATE, LEVEL_LOW);

    const board_heads_t heads = board_heads(board);
    board_close(board);
    CHECK(one_head_low(heads));
    CHECK(elapsed <= GATE_RESPONSE_LIMIT_NS);
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

static bool startup_drives_no_pin_and_pulls_no_signal_line(void) {
    board_t *board = open_board();

    const uint8_t driven = board_port_directions(board, 'A') | board_port_directions(board, 'B') |
                           board_port_directions(board, 'D');

    const uint8_t pulled = (board_port_levels(board, 'A') & SIGNAL_PIN_BITS_A) |
                           (board_port_levels(board, 'B') & SIGNAL_PIN_BITS_B) |
                           (board_port_levels(board, 'D') & WRITE_DATA_PIN_BIT_D);
    board_close(board);
    CHECK(driven == 0);
    CHECK(pulled == 0);
    return true;
}

static bool clock_is_undivided_and_watchdog_is_armed(void) {
    board_t *board = open_board();

    const uint8_t prescaler = board_clock_prescaler(board);

    const uint8_t watchdog = board_watchdog_control(board);
    board_close(board);
    CHECK(prescaler == 0);
    CHECK((watchdog & WATCHDOG_ENABLE_BIT) != 0);
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
    TEST_CASE(heads_released_while_disk_write_protected),
    TEST_CASE(heads_released_while_drive_not_ready),
    TEST_CASE(one_head_low_and_alternating_at_fastest_data_rate),
    TEST_CASE(rising_edge_leaves_heads_unchanged),
    TEST_CASE(heads_release_when_write_gate_closes),
    TEST_CASE(heads_release_when_write_protect_asserts),
    TEST_CASE(heads_release_when_ready_drops),
    TEST_CASE(one_head_engages_when_write_gate_opens),
    TEST_CASE(heads_end_released_after_write_gate_glitch),
    TEST_CASE(startup_drives_no_pin_and_pulls_no_signal_line),
    TEST_CASE(clock_is_undivided_and_watchdog_is_armed),
    TEST_CASE(long_write_never_trips_the_watchdog),
};

int main(int argc, char **argv) {
    if (argc != 4) {
        fprintf(stderr, "usage: %s <mcu> <firmware.elf> <instructions.insn>\n", argv[0]);
        return EXIT_FAILURE;
    }
    target_mcu = argv[1];
    target_elf = argv[2];
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
    printf("%s: %zu tests, %d failed, %u firmware instructions never executed\n", target_mcu, test_count, failures,
           missing);
    return failures == 0 && missing == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
