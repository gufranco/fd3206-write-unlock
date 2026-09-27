#ifndef FDSWRITEUNLOCK_TEST_BOARD_H
#define FDSWRITEUNLOCK_TEST_BOARD_H

#include <stdbool.h>
#include <stdint.h>

enum {
    HEAD1_MASK = 0x08,
    HEAD2_MASK = 0x04,
    HEADS_MASK = HEAD1_MASK | HEAD2_MASK
};

typedef enum {
    SIGNAL_WRITE_GATE,
    SIGNAL_WRITE_PROTECT,
    SIGNAL_READY,
    SIGNAL_WRITE_DATA,
    SIGNAL_COUNT
} board_signal_t;

typedef enum {
    LEVEL_LOW = 0,
    LEVEL_HIGH = 1
} board_level_t;

typedef struct {
    const char *mcu;
    const char *elf_path;
} board_config_t;

typedef struct board board_t;

board_t *board_open(const board_config_t *config);
void board_close(board_t *board);
void board_set(board_t *board, board_signal_t signal, board_level_t level);
void board_run_cycles(board_t *board, uint64_t cycles);
uint64_t board_cycles_per_us(const board_t *board);
uint64_t board_cycle(const board_t *board);
uint64_t board_last_head_change_cycle(const board_t *board);
uint8_t board_head_directions(const board_t *board);
uint8_t board_head_levels(const board_t *board);
uint8_t board_port_directions(const board_t *board, char port);
bool board_has_port(const board_t *board, char port);
uint8_t board_clock_prescaler(const board_t *board);
uint8_t board_watchdog_control(const board_t *board);
uint32_t board_reset_count(const board_t *board);

bool coverage_load(const char *instruction_list_path);
uint32_t coverage_report_missing(void);

#endif
