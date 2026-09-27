#include "edge_counter.h"

#include "hardware/gpio.h"
#include "hardware/pwm.h"

static uint32_t total_edges;
static uint16_t last_raw;

void edge_counter_start(void) {
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv_mode(&config, PWM_DIV_B_FALLING);
    pwm_config_set_clkdiv_int(&config, 1);
    pwm_init(EDGE_COUNTER_SLICE, &config, true);
    gpio_set_function(PIN_WRITE_DATA, GPIO_FUNC_PWM);
    gpio_disable_pulls(PIN_WRITE_DATA);
    last_raw = edge_counter_raw();
}

void edge_counter_poll(void) {
    const uint16_t raw = edge_counter_raw();
    total_edges += (uint16_t)(raw - last_raw);
    last_raw = raw;
}

uint32_t edge_counter_total(void) {
    return total_edges;
}

uint32_t edge_counter_resolve(uint16_t raw_snapshot) {
    return total_edges - (uint16_t)(last_raw - raw_snapshot);
}
