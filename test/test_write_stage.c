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
    SETTLE_NS = 10000,
    LATENCY_LIMIT_NS = 1500,
    JITTER_LIMIT_NS = 375,
    RELEASE_LIMIT_NS = 5000,
    ENGAGE_LIMIT_NS = 5000,
    WRITE_EDGE_COUNT = 1000,
    IDLE_EDGE_COUNT = 100,
    PHASE_SPREAD_CYCLES = 32,
    WATCHDOG_SOAK_EDGES = 20000,
    RANDOM_SEED = 0x3206u,
    EVEN_IDLE_EDGES = 4,
    ODD_IDLE_EDGES = 5,
    WATCHDOG_ENABLE_BIT = 0x08
};

typedef struct {
    board_level_t write_gate;
    board_level_t write_protect;
    board_level_t ready;
} write_conditions_t;

typedef struct {
    uint32_t edges;
    uint32_t edges_with_one_head;
    uint32_t edges_with_no_head;
    uint32_t edges_repeating_head;
    uint32_t edges_driving_high;
} head_census_t;

typedef struct {
    uint64_t min_ns;
    uint64_t max_ns;
} latency_t;

static const write_conditions_t WRITING = {LEVEL_LOW, LEVEL_LOW, LEVEL_LOW};
static const write_conditions_t GATE_CLOSED = {LEVEL_HIGH, LEVEL_LOW, LEVEL_LOW};
static const write_conditions_t PROTECTED = {LEVEL_LOW, LEVEL_HIGH, LEVEL_LOW};
static const write_conditions_t NOT_READY = {LEVEL_LOW, LEVEL_LOW, LEVEL_HIGH};

static const char *target_mcu;
static const char *target_elf;
static uint32_t random_state = RANDOM_SEED;
static int failures;

#define CHECK(condition)                                                            \
    do {                                                                            \
        if (!(condition)) {                                                         \
            fprintf(stderr, "%s:%d: %s: check failed: %s\n", __FILE__, __LINE__,    \
                    target_mcu, #condition);                                        \
            return false;                                                           \
        }                                                                           \
    } while (0)

static uint32_t next_random(void) {
    random_state ^= random_state << 13;
    random_state ^= random_state >> 17;
    random_state ^= random_state << 5;
    return random_state;
}

static uint64_t ns_to_cycles(const board_t *board, uint64_t ns) {
    return ns * board_cycles_per_us(board) / NS_PER_US;
}

static uint64_t cycles_to_ns(const board_t *board, uint64_t cycles) {
    return cycles * NS_PER_US / board_cycles_per_us(board);
}

static board_t *open_board(void) {
    const board_config_t config = {.mcu = target_mcu, .elf_path = target_elf};
    board_t *board = board_open(&config);
    if (board == NULL) {
        exit(EXIT_FAILURE);
    }
    return board;
}

static void run_ns(board_t *board, uint64_t ns) {
    board_run_cycles(board, ns_to_cycles(board, ns));
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

static head_census_t census_heads(board_t *board, uint32_t edges, uint64_t period_ns) {
    head_census_t census = {0};
    uint8_t previous = board_head_directions(board);
    for (uint32_t edge = 0; edge < edges; edge++) {
        write_pulse(board, period_ns);
        const uint8_t heads = board_head_directions(board);
        census.edges++;
        census.edges_with_one_head += heads == HEAD1_MASK || heads == HEAD2_MASK;
        census.edges_with_no_head += heads == 0;
        census.edges_repeating_head += heads != 0 && heads == previous;
        census.edges_driving_high += board_head_levels(board) != 0;
        previous = heads;
    }
    return census;
}

static uint64_t head_change_ns_since(const board_t *board, uint64_t start_cycle) {
    return cycles_to_ns(board, board_last_head_change_cycle(board) - start_cycle);
}

static latency_t measure_edge_latency(board_t *board, uint32_t edges) {
    latency_t latency = {UINT64_MAX, 0};
    for (uint32_t edge = 0; edge < edges; edge++) {
        board_run_cycles(board, next_random() % PHASE_SPREAD_CYCLES);
        const uint64_t edge_cycle = board_cycle(board);
        write_pulse(board, HALF_BIT_CELL_NS);
        const uint64_t delay_ns = head_change_ns_since(board, edge_cycle);
        latency.min_ns = delay_ns < latency.min_ns ? delay_ns : latency.min_ns;
        latency.max_ns = delay_ns > latency.max_ns ? delay_ns : latency.max_ns;
    }
    return latency;
}

static uint64_t measure_release_ns(board_t *board, board_signal_t signal) {
    const uint64_t start = board_cycle(board);
    board_set(board, signal, LEVEL_HIGH);
    run_ns(board, RELEASE_LIMIT_NS);
    return board_head_directions(board) == 0 ? head_change_ns_since(board, start) : UINT64_MAX;
}

static uint64_t measure_engage_ns(board_t *board) {
    const uint64_t start = board_cycle(board);
    board_set(board, SIGNAL_WRITE_GATE, LEVEL_LOW);
    run_ns(board, ENGAGE_LIMIT_NS);
    return board_head_directions(board) != 0 ? head_change_ns_since(board, start) : UINT64_MAX;
}

static bool heads_float_under(write_conditions_t conditions) {
    board_t *board = open_board();
    apply_conditions(board, conditions);

    const head_census_t census = census_heads(board, IDLE_EDGE_COUNT, HALF_BIT_CELL_NS);

    board_close(board);
    CHECK(census.edges_with_no_head == IDLE_EDGE_COUNT);
    CHECK(census.edges_driving_high == 0);
    return true;
}

static bool heads_float_when_write_gate_is_inactive(void) {
    return heads_float_under(GATE_CLOSED);
}

static bool heads_float_when_disk_is_write_protected(void) {
    return heads_float_under(PROTECTED);
}

static bool heads_float_when_drive_is_not_ready(void) {
    return heads_float_under(NOT_READY);
}

static bool one_head_alternates_on_every_falling_edge_while_writing(void) {
    board_t *board = open_board();
    apply_conditions(board, WRITING);

    const head_census_t census = census_heads(board, WRITE_EDGE_COUNT, FAST_HALF_BIT_CELL_NS);

    board_close(board);
    CHECK(census.edges_with_one_head == WRITE_EDGE_COUNT);
    CHECK(census.edges_repeating_head == 0);
    CHECK(census.edges_driving_high == 0);
    return true;
}

static bool rising_edge_leaves_heads_unchanged(void) {
    board_t *board = open_board();
    apply_conditions(board, WRITING);
    board_set(board, SIGNAL_WRITE_DATA, LEVEL_LOW);
    run_ns(board, WRITE_PULSE_LOW_NS);
    const uint8_t heads_after_fall = board_head_directions(board);

    board_set(board, SIGNAL_WRITE_DATA, LEVEL_HIGH);
    run_ns(board, HALF_BIT_CELL_NS);

    const uint8_t heads_after_rise = board_head_directions(board);
    board_close(board);
    CHECK(heads_after_fall != 0);
    CHECK(heads_after_rise == heads_after_fall);
    return true;
}

static bool head_switches_within_latency_and_jitter_limits(void) {
    board_t *board = open_board();
    apply_conditions(board, WRITING);

    const latency_t latency = measure_edge_latency(board, WRITE_EDGE_COUNT);

    board_close(board);
    printf("  %s edge latency %llu to %llu ns\n", target_mcu, (unsigned long long)latency.min_ns,
           (unsigned long long)latency.max_ns);
    CHECK(latency.max_ns <= LATENCY_LIMIT_NS);
    CHECK(latency.max_ns - latency.min_ns <= JITTER_LIMIT_NS);
    return true;
}

static bool heads_release_when_signal_goes_inactive(board_signal_t signal) {
    board_t *board = open_board();
    apply_conditions(board, WRITING);
    write_pulse(board, HALF_BIT_CELL_NS);
    const uint8_t heads_while_writing = board_head_directions(board);

    const uint64_t release_ns = measure_release_ns(board, signal);

    board_close(board);
    CHECK(heads_while_writing != 0);
    CHECK(release_ns <= RELEASE_LIMIT_NS);
    return true;
}

static bool heads_release_when_write_gate_closes_without_data_edges(void) {
    return heads_release_when_signal_goes_inactive(SIGNAL_WRITE_GATE);
}

static bool heads_release_when_write_protect_asserts_without_data_edges(void) {
    return heads_release_when_signal_goes_inactive(SIGNAL_WRITE_PROTECT);
}

static bool heads_release_when_ready_drops_without_data_edges(void) {
    return heads_release_when_signal_goes_inactive(SIGNAL_READY);
}

static bool head_engages_after_idle_edges(uint32_t idle_edges, uint8_t expected_head) {
    board_t *board = open_board();
    apply_conditions(board, GATE_CLOSED);
    census_heads(board, idle_edges, HALF_BIT_CELL_NS);

    const uint64_t engage_ns = measure_engage_ns(board);

    const uint8_t heads = board_head_directions(board);
    const uint8_t levels = board_head_levels(board);
    board_close(board);
    CHECK(engage_ns <= ENGAGE_LIMIT_NS);
    CHECK(heads == expected_head);
    CHECK(levels == 0);
    return true;
}

static bool head_one_engages_when_gate_opens_after_even_edge_count(void) {
    return head_engages_after_idle_edges(EVEN_IDLE_EDGES, HEAD1_MASK);
}

static bool head_two_engages_when_gate_opens_after_odd_edge_count(void) {
    return head_engages_after_idle_edges(ODD_IDLE_EDGES, HEAD2_MASK);
}

static uint32_t count_ports_with_outputs(const board_t *board) {
    static const char PORTS[] = {'A', 'B', 'C', 'D'};
    uint32_t driven = 0;
    for (size_t index = 0; index < sizeof PORTS; index++) {
        driven += board_has_port(board, PORTS[index]) && board_port_directions(board, PORTS[index]) != 0;
    }
    return driven;
}

static bool startup_leaves_every_pin_undriven(void) {
    board_t *board = open_board();

    const uint32_t driven_ports = count_ports_with_outputs(board);

    board_close(board);
    CHECK(driven_ports == 0);
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

    const head_census_t census = census_heads(board, WATCHDOG_SOAK_EDGES, HALF_BIT_CELL_NS);

    const uint32_t resets = board_reset_count(board);
    board_close(board);
    CHECK(resets == 0);
    CHECK(census.edges_with_one_head == WATCHDOG_SOAK_EDGES);
    return true;
}

typedef struct {
    const char *name;
    bool (*run)(void);
} test_case_t;

#define TEST_CASE(function) {#function, function}

static const test_case_t TESTS[] = {
    TEST_CASE(heads_float_when_write_gate_is_inactive),
    TEST_CASE(heads_float_when_disk_is_write_protected),
    TEST_CASE(heads_float_when_drive_is_not_ready),
    TEST_CASE(one_head_alternates_on_every_falling_edge_while_writing),
    TEST_CASE(rising_edge_leaves_heads_unchanged),
    TEST_CASE(head_switches_within_latency_and_jitter_limits),
    TEST_CASE(heads_release_when_write_gate_closes_without_data_edges),
    TEST_CASE(heads_release_when_write_protect_asserts_without_data_edges),
    TEST_CASE(heads_release_when_ready_drops_without_data_edges),
    TEST_CASE(head_one_engages_when_gate_opens_after_even_edge_count),
    TEST_CASE(head_two_engages_when_gate_opens_after_odd_edge_count),
    TEST_CASE(startup_leaves_every_pin_undriven),
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
    for (size_t index = 0; index < test_count; index++) {
        const bool passed = TESTS[index].run();
        printf("%s %s: %s\n", passed ? "PASS" : "FAIL", target_mcu, TESTS[index].name);
        failures += !passed;
    }
    const uint32_t missing = coverage_report_missing();
    printf("%s: %zu tests, %d failed, %u firmware instructions never executed\n", target_mcu, test_count,
           failures, missing);
    return failures == 0 && missing == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
