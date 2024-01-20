
#pragma once

#include <string.h>

#include "driver/gpio.h"
#include "driver/mcpwm.h"

typedef struct
{
    gpio_num_t ch_A_pin;
    gpio_num_t ch_B_pin;
    
    mcpwm_unit_t mcpwm_unit;
    mcpwm_timer_t mcpwm_timer;
    mcpwm_io_signals_t mcpwm_ch_A_signal;
    mcpwm_io_signals_t mcpwm_ch_B_signal;

    mcpwm_config_t mcpwm_config;
} motor_handle_t;

motor_handle_t *brushed_motor_default_config(motor_handle_t *handle);
void brushed_motor_init(motor_handle_t *handle);

void brushed_motor_set(motor_handle_t *handle, float duty_cycle);
void brushed_motor_forward(motor_handle_t *handle, float duty_cycle);
void brushed_motor_backward(motor_handle_t *handle, float duty_cycle);
void brushed_motor_stop(motor_handle_t *handle);


