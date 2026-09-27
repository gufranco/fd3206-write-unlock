#include "write_gate.h"

#include "board.h"
#include "edge_counter.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/structs/iobank0.h"
#include "hardware/timer.h"
#include "pico/platform.h"
#include "write_log.h"

enum {
    EDGE_EVENTS = GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL,
    EVENT_BITS_PER_PIN = 4,
    PINS_PER_EVENT_REGISTER = 8,
    RECONCILE_INTERVAL_US = 1000
};

static const uint CONDITION_PINS[] = {PIN_WRITE_GATE_N, PIN_WRITE_PROTECT_N, PIN_READY_N};

_Static_assert(PIN_WRITE_GATE_N / PINS_PER_EVENT_REGISTER == PIN_WRITE_PROTECT_N / PINS_PER_EVENT_REGISTER &&
                   PIN_WRITE_GATE_N / PINS_PER_EVENT_REGISTER == PIN_READY_N / PINS_PER_EVENT_REGISTER,
               "write condition pins must share one interrupt status register");

static void __time_critical_func(set_head_output_override)(uint pin, uint override) {
    hw_write_masked(&io_bank0_hw->io[pin].ctrl, override << IO_BANK0_GPIO0_CTRL_OEOVER_LSB,
                    IO_BANK0_GPIO0_CTRL_OEOVER_BITS);
}

static void __time_critical_func(apply_write_conditions)(void) {
    const bool writing = (sio_hw->gpio_in & WRITE_CONDITION_PINS) == 0;
    const uint override = writing ? GPIO_OVERRIDE_NORMAL : GPIO_OVERRIDE_LOW;
    set_head_output_override(PIN_HEAD1, override);
    set_head_output_override(PIN_HEAD2, override);
    gpio_put(PIN_ACTIVITY_LED, writing);
    write_log_mark(writing, edge_counter_raw(), time_us_32());
}

static uint32_t condition_event_mask(void) {
    uint32_t mask = 0;
    for (uint index = 0; index < count_of(CONDITION_PINS); index++) {
        mask |= (uint32_t)EDGE_EVENTS << (EVENT_BITS_PER_PIN * (CONDITION_PINS[index] % PINS_PER_EVENT_REGISTER));
    }
    return mask;
}

static uint32_t event_mask;

enum { CONDITION_EVENT_REGISTER = PIN_WRITE_GATE_N / PINS_PER_EVENT_REGISTER };
static uint32_t last_reconcile_us;

static void __isr __time_critical_func(on_write_condition_change)(void) {
    io_bank0_hw->proc0_irq_ctrl.intf[CONDITION_EVENT_REGISTER] = 0;
    io_bank0_hw->intr[CONDITION_EVENT_REGISTER] = event_mask;
    apply_write_conditions();
}

static void request_write_condition_check(void) {
    io_bank0_hw->proc0_irq_ctrl.intf[CONDITION_EVENT_REGISTER] = event_mask;
}

static void start_condition_input(uint pin) {
    gpio_init(pin);
    gpio_disable_pulls(pin);
    gpio_set_irq_enabled(pin, EDGE_EVENTS, true);
}

static void start_head_output(uint pin) {
    gpio_set_function(pin, WRITE_STAGE_GPIO_FUNCTION);
    set_head_output_override(pin, GPIO_OVERRIDE_LOW);
    gpio_pull_down(pin);
}

void write_gate_start(void) {
    start_head_output(PIN_HEAD1);
    start_head_output(PIN_HEAD2);
    gpio_init(PIN_ACTIVITY_LED);
    gpio_set_dir(PIN_ACTIVITY_LED, GPIO_OUT);
    event_mask = condition_event_mask();
    for (uint index = 0; index < count_of(CONDITION_PINS); index++) {
        start_condition_input(CONDITION_PINS[index]);
    }
    irq_set_exclusive_handler(IO_IRQ_BANK0, on_write_condition_change);
    irq_set_priority(IO_IRQ_BANK0, PICO_HIGHEST_IRQ_PRIORITY);
    irq_set_enabled(IO_IRQ_BANK0, true);
    request_write_condition_check();
}

void write_gate_reconcile(void) {
    const uint32_t now_us = time_us_32();
    if (now_us - last_reconcile_us < RECONCILE_INTERVAL_US) {
        return;
    }
    last_reconcile_us = now_us;
    request_write_condition_check();
}
