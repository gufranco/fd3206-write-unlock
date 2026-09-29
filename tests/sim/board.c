/* SPDX-FileCopyrightText: 2026 Gustavo Franco <gufranco@users.noreply.github.com> */
/* SPDX-License-Identifier: MIT */

#include "board.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "avr_ioport.h"
#include "sim_avr.h"
#include "sim_core.h"
#include "sim_elf.h"

enum {
    MAX_FLASH_BYTES = 4096,
    STARTUP_US = 200,
    HZ_PER_MHZ = 1000000,
    RAM_START = 0x60,
    PORT_COUNT = 4,
    PORT_NAME_BITS = 0x7F,
    CLOCK_PRESCALER_ADDRESS = 0x46,
    CLOCK_PRESCALER_CHANGE_ENABLE = 0x80,
    CLOCK_PRESCALER_BITS = 0x0F,
    CLOCK_PRESCALER_WINDOW_CYCLES = 4,
    UNFUSED_CLOCK_DIVISION = 0x03,
    RESET_FLAGS_ADDRESS = 0x54,
    HANG_OPCODE_LOW = 0xFF,
    HANG_OPCODE_HIGH = 0xCF,
    WATCHDOG_CONTROL_ADDRESS = 0x41,
    LINE1_BIT = 3,
    LINE2_BIT = 2
};

typedef struct {
    char port;
    uint8_t bit;
} pin_t;

static const pin_t SIGNAL_PINS[SIGNAL_COUNT] = {
    [SIGNAL_WRITE_GATE] = {'A', 1},
    [SIGNAL_WRITABLE_MEDIA] = {'A', 0},
    [SIGNAL_READY] = {'B', 1},
    [SIGNAL_WRITE_DATA] = {'D', 2},
};

struct board {
    avr_t *avr;
    avr_irq_t *inputs[SIGNAL_COUNT];
    board_lines_t last_write_lines;
    uint64_t last_line_change_cycle;
    uint32_t resets;
    bool started;
    uint8_t driven_mask[PORT_COUNT];
    uint8_t driven_value[PORT_COUNT];
    uint64_t prescaler_change_until;
    bool prescaler_change_armed;
};

static uint16_t deepest_stack;
static bool covered[MAX_FLASH_BYTES];
static bool expected[MAX_FLASH_BYTES];

static avr_ioport_state_t port_state(const board_t *board, char port) {
    avr_ioport_state_t state = {0};
    avr_ioctl(board->avr, (uint32_t)AVR_IOCTL_IOPORT_GETSTATE(port), &state);
    return state;
}

static bool bit_set(uint8_t value, uint8_t bit) {
    return (value >> bit) & 1u;
}

static board_lines_t classify_write_lines(bool line1_enabled, bool line1_low, bool line2_enabled, bool line2_low) {
    const bool pulled1 = line1_enabled && line1_low;
    const bool pulled2 = line2_enabled && line2_low;
    if ((line1_enabled && !line1_low) || (line2_enabled && !line2_low)) {
        return LINES_DRIVEN_HIGH;
    }
    if (pulled1 && pulled2) {
        return LINES_BOTH_LOW;
    }
    if (pulled1) {
        return LINES_LINE1_LOW;
    }
    return pulled2 ? LINES_LINE2_LOW : LINES_RELEASED;
}

board_lines_t board_lines(const board_t *board) {
    const avr_ioport_state_t port_b = port_state(board, 'B');
    return classify_write_lines(bit_set((uint8_t)port_b.ddr, LINE1_BIT), !bit_set((uint8_t)port_b.port, LINE1_BIT),
                                bit_set((uint8_t)port_b.ddr, LINE2_BIT), !bit_set((uint8_t)port_b.port, LINE2_BIT));
}

static void track_write_lines(board_t *board) {
    const board_lines_t lines = board_lines(board);
    if (lines != board->last_write_lines) {
        board->last_write_lines = lines;
        board->last_line_change_cycle = board->avr->cycle;
    }
}

static void step(board_t *board) {
    avr_t *avr = board->avr;
    if (avr->pc < MAX_FLASH_BYTES) {
        covered[avr->pc] = true;
    }
    if (board->started && avr->pc == 0) {
        board->resets++;
    }
    const int state = avr_run(avr);
    if (state == cpu_Done || state == cpu_Crashed) {
        fprintf(stderr, "simulation stopped at pc 0x%04x with state %d\n", avr->pc, state);
        exit(EXIT_FAILURE);
    }
    const uint16_t depth = (uint16_t)(avr->ramend - _avr_sp_get(avr));
    if (board->started && depth > deepest_stack) {
        deepest_stack = depth;
    }
    track_write_lines(board);
}

static avr_t *load_avr(const char *mcu, const char *elf_path, uint32_t frequency_hz) {
    elf_firmware_t firmware;
    memset(&firmware, 0, sizeof firmware);
    if (elf_read_firmware(elf_path, &firmware) != 0) {
        fprintf(stderr, "cannot read firmware %s\n", elf_path);
        return NULL;
    }
    snprintf(firmware.mmcu, sizeof firmware.mmcu, "%s", mcu);
    firmware.frequency = frequency_hz;
    avr_t *avr = avr_make_mcu_by_name(mcu);
    if (avr == NULL) {
        fprintf(stderr, "simavr has no core for %s\n", mcu);
        return NULL;
    }
    avr_init(avr);
    avr->log = LOG_ERROR;
    avr_load_firmware(avr, &firmware);
    return avr;
}

static size_t port_index(char port) {
    return (size_t)(port - 'A') % PORT_COUNT;
}

static void drive_pin(board_t *board, pin_t pin, board_level_t level) {
    const size_t index = port_index(pin.port);
    const uint8_t bit = (uint8_t)(1U << pin.bit);
    board->driven_mask[index] = (uint8_t)(board->driven_mask[index] | bit);
    board->driven_value[index] = level == LEVEL_HIGH ? (uint8_t)(board->driven_value[index] | bit)
                                                     : (uint8_t)(board->driven_value[index] & ~bit);
    avr_ioport_external_t external = {0};
    external.name = (unsigned long)pin.port & PORT_NAME_BITS;
    external.mask = board->driven_mask[index];
    external.value = board->driven_value[index];
    avr_ioctl(board->avr, (uint32_t)AVR_IOCTL_IOPORT_SET_EXTERNAL(pin.port), &external);
}

static void clock_prescaler_write(avr_t *avr, avr_io_addr_t address, uint8_t value, void *param) {
    board_t *board = param;
    const bool change_enable = (value & CLOCK_PRESCALER_CHANGE_ENABLE) != 0U;
    if (change_enable && (value & (uint8_t)~CLOCK_PRESCALER_CHANGE_ENABLE) == 0U) {
        board->prescaler_change_until = avr->cycle + CLOCK_PRESCALER_WINDOW_CYCLES;
        board->prescaler_change_armed = true;
        return;
    }
    if (!change_enable && board->prescaler_change_armed && avr->cycle <= board->prescaler_change_until) {
        avr->data[address] = (uint8_t)(value & CLOCK_PRESCALER_BITS);
    }
    board->prescaler_change_armed = false;
}

static void power_on_unfused(board_t *board) {
    board->avr->data[CLOCK_PRESCALER_ADDRESS] = UNFUSED_CLOCK_DIVISION;
    board->prescaler_change_armed = false;
}

static void wire_inputs(board_t *board) {
    for (int signal = 0; signal < SIGNAL_COUNT; signal++) {
        const pin_t pin = SIGNAL_PINS[signal];
        board->inputs[signal] = avr_io_getirq(board->avr, (uint32_t)AVR_IOCTL_IOPORT_GETIRQ(pin.port), pin.bit);
        drive_pin(board, pin, LEVEL_HIGH);
        avr_raise_irq(board->inputs[signal], LEVEL_HIGH);
    }
}

board_t *board_open(const char *mcu, const char *elf_path, uint32_t frequency_hz) {
    avr_t *avr = load_avr(mcu, elf_path, frequency_hz);
    if (avr == NULL) {
        return NULL;
    }
    board_t *board = calloc(1, sizeof *board);
    if (board == NULL) {
        fprintf(stderr, "out of memory opening board\n");
        avr_terminate(avr);
        return NULL;
    }
    board->avr = avr;
    board->last_write_lines = LINES_RELEASED;
    wire_inputs(board);
    avr_register_io_write(avr, CLOCK_PRESCALER_ADDRESS, clock_prescaler_write, board);
    power_on_unfused(board);
    board_wait_startup(board);
    board->started = true;
    return board;
}

void board_wait_startup(board_t *board) {
    board_run_cycles(board, (uint64_t)STARTUP_US * board_frequency_hz(board) / HZ_PER_MHZ);
}

void board_close(board_t *board) {
    avr_terminate(board->avr);
    free(board);
}

void board_set(board_t *board, board_signal_t signal, board_level_t level) {
    drive_pin(board, SIGNAL_PINS[signal], level);
    avr_raise_irq(board->inputs[signal], level);
    track_write_lines(board);
}

void board_run_cycles(board_t *board, uint64_t cycles) {
    const uint64_t target_cycle = board->avr->cycle + cycles;
    while (board->avr->cycle < target_cycle) {
        step(board);
    }
}

uint64_t board_frequency_hz(const board_t *board) {
    return board->avr->frequency;
}

uint64_t board_cycle(const board_t *board) {
    return board->avr->cycle;
}

uint64_t board_last_line_change_cycle(const board_t *board) {
    return board->last_line_change_cycle;
}

uint8_t board_port_directions(const board_t *board, char port) {
    return (uint8_t)port_state(board, port).ddr;
}

uint8_t board_port_levels(const board_t *board, char port) {
    return (uint8_t)port_state(board, port).port;
}

uint8_t board_clock_prescaler(const board_t *board) {
    return board->avr->data[CLOCK_PRESCALER_ADDRESS];
}

uint8_t board_watchdog_control(const board_t *board) {
    return board->avr->data[WATCHDOG_CONTROL_ADDRESS];
}

uint32_t board_reset_count(const board_t *board) {
    return board->resets;
}

uint8_t board_reset_flags(const board_t *board) {
    return board->avr->data[RESET_FLAGS_ADDRESS];
}

void board_inject_hang(board_t *board) {
    avr_t *avr = board->avr;
    const uint32_t address = avr->flashend - 1U;
    avr->flash[address] = HANG_OPCODE_LOW;
    avr->flash[address + 1U] = HANG_OPCODE_HIGH;
    avr->pc = address;
}

void board_reset(board_t *board) {
    avr_reset(board->avr);
    power_on_unfused(board);
    for (int signal = 0; signal < SIGNAL_COUNT; signal++) {
        const pin_t pin = SIGNAL_PINS[signal];
        const size_t index = port_index(pin.port);
        const board_level_t level = (board->driven_value[index] & (1U << pin.bit)) != 0U ? LEVEL_HIGH : LEVEL_LOW;
        drive_pin(board, pin, level);
        avr_raise_irq(board->inputs[signal], level);
    }
    track_write_lines(board);
}

uint16_t board_deepest_stack(void) {
    return deepest_stack;
}

uint16_t board_ram_bytes(const board_t *board) {
    return (uint16_t)(board->avr->ramend + 1U - RAM_START);
}

bool coverage_load(const char *instruction_list_path) {
    FILE *file = fopen(instruction_list_path, "r");
    if (file == NULL) {
        fprintf(stderr, "cannot open %s\n", instruction_list_path);
        return false;
    }
    unsigned address = 0;
    while (fscanf(file, "%u %*s", &address) == 1) {
        if (address < MAX_FLASH_BYTES) {
            expected[address] = true;
        }
    }
    fclose(file);
    return true;
}

uint32_t coverage_report_missing(void) {
    uint32_t missing = 0;
    for (unsigned address = 0; address < MAX_FLASH_BYTES; address++) {
        if (expected[address] && !covered[address]) {
            fprintf(stderr, "instruction at 0x%04x never executed\n", address);
            missing++;
        }
    }
    return missing;
}
