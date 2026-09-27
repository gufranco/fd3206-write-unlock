#include "board.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "avr_ioport.h"
#include "sim_avr.h"
#include "sim_elf.h"

enum {
    MAX_FLASH_BYTES = 32768,
    STARTUP_US = 200,
    CYCLES_PER_MHZ = 1000000,
    PORT_COUNT = 3
};

typedef struct {
    char port;
    uint8_t bit;
} pin_t;

typedef struct {
    const char *mcu;
    uint32_t frequency;
    pin_t pins[SIGNAL_COUNT];
    uint16_t clock_prescaler_address;
    uint16_t watchdog_address;
    char ports[PORT_COUNT + 1];
} target_t;

static const target_t TARGETS[] = {
    {"attiny2313a", 8000000, {{'A', 1}, {'A', 0}, {'B', 1}, {'D', 2}}, 0x46, 0x41, "ABD"},
    {"atmega328p", 16000000, {{'B', 4}, {'B', 0}, {'B', 1}, {'D', 2}}, 0x61, 0x60, "BCD"},
};

struct board {
    avr_t *avr;
    const target_t *target;
    avr_irq_t *inputs[SIGNAL_COUNT];
    uint8_t last_heads;
    uint64_t last_head_change_cycle;
    uint32_t resets;
    bool started;
};

static bool covered[MAX_FLASH_BYTES];
static bool expected[MAX_FLASH_BYTES];

static const target_t *find_target(const char *mcu) {
    for (size_t index = 0; index < sizeof TARGETS / sizeof TARGETS[0]; index++) {
        if (strcmp(TARGETS[index].mcu, mcu) == 0) {
            return &TARGETS[index];
        }
    }
    return NULL;
}

static avr_ioport_state_t port_state(const board_t *board, char port) {
    avr_ioport_state_t state = {0};
    avr_ioctl(board->avr, AVR_IOCTL_IOPORT_GETSTATE(port), &state);
    return state;
}

static void on_port_b_direction(avr_irq_t *irq, uint32_t value, void *param) {
    (void)irq;
    board_t *board = param;
    const uint8_t heads = (uint8_t)(value & HEADS_MASK);
    if (heads == board->last_heads) {
        return;
    }
    board->last_heads = heads;
    board->last_head_change_cycle = board->avr->cycle;
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
}

static avr_t *load_avr(const target_t *target, const char *elf_path) {
    elf_firmware_t firmware;
    memset(&firmware, 0, sizeof firmware);
    if (elf_read_firmware(elf_path, &firmware) != 0) {
        fprintf(stderr, "cannot read firmware %s\n", elf_path);
        return NULL;
    }
    snprintf(firmware.mmcu, sizeof firmware.mmcu, "%s", target->mcu);
    firmware.frequency = target->frequency;
    avr_t *avr = avr_make_mcu_by_name(target->mcu);
    if (avr == NULL) {
        fprintf(stderr, "simavr has no core for %s\n", target->mcu);
        return NULL;
    }
    avr_init(avr);
    avr->log = LOG_ERROR;
    avr_load_firmware(avr, &firmware);
    return avr;
}

static void wire_board(board_t *board) {
    for (int signal = 0; signal < SIGNAL_COUNT; signal++) {
        const pin_t pin = board->target->pins[signal];
        board->inputs[signal] = avr_io_getirq(board->avr, AVR_IOCTL_IOPORT_GETIRQ(pin.port), pin.bit);
        avr_raise_irq(board->inputs[signal], LEVEL_HIGH);
    }
    avr_irq_t *direction = avr_io_getirq(board->avr, AVR_IOCTL_IOPORT_GETIRQ('B'), IOPORT_IRQ_DIRECTION_ALL);
    avr_irq_register_notify(direction, on_port_b_direction, board);
}

board_t *board_open(const board_config_t *config) {
    const target_t *target = find_target(config->mcu);
    if (target == NULL) {
        fprintf(stderr, "unknown target %s\n", config->mcu);
        return NULL;
    }
    avr_t *avr = load_avr(target, config->elf_path);
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
    board->target = target;
    wire_board(board);
    board_run_cycles(board, STARTUP_US * board_cycles_per_us(board));
    board->started = true;
    return board;
}

void board_close(board_t *board) {
    avr_terminate(board->avr);
    free(board);
}

void board_set(board_t *board, board_signal_t signal, board_level_t level) {
    avr_raise_irq(board->inputs[signal], level);
}

void board_run_cycles(board_t *board, uint64_t cycles) {
    const uint64_t target_cycle = board->avr->cycle + cycles;
    while (board->avr->cycle < target_cycle) {
        step(board);
    }
}

uint64_t board_cycles_per_us(const board_t *board) {
    return board->target->frequency / CYCLES_PER_MHZ;
}

uint64_t board_cycle(const board_t *board) {
    return board->avr->cycle;
}

uint64_t board_last_head_change_cycle(const board_t *board) {
    return board->last_head_change_cycle;
}

uint8_t board_head_directions(const board_t *board) {
    return (uint8_t)(port_state(board, 'B').ddr & HEADS_MASK);
}

uint8_t board_head_levels(const board_t *board) {
    return (uint8_t)(port_state(board, 'B').port & HEADS_MASK);
}

uint8_t board_port_directions(const board_t *board, char port) {
    return (uint8_t)port_state(board, port).ddr;
}

bool board_has_port(const board_t *board, char port) {
    return strchr(board->target->ports, port) != NULL;
}

uint8_t board_clock_prescaler(const board_t *board) {
    return board->avr->data[board->target->clock_prescaler_address];
}

uint8_t board_watchdog_control(const board_t *board) {
    return board->avr->data[board->target->watchdog_address];
}

uint32_t board_reset_count(const board_t *board) {
    return board->resets;
}

bool coverage_load(const char *instruction_list_path) {
    FILE *file = fopen(instruction_list_path, "r");
    if (file == NULL) {
        fprintf(stderr, "cannot open %s\n", instruction_list_path);
        return false;
    }
    unsigned address = 0;
    while (fscanf(file, "%u", &address) == 1) {
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
