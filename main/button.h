
#pragma once

#include <string.h>

#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "driver/gpio.h"

#include "esp_log.h"
#include "esp_timer.h"

#include "logging.h"

#define BUTTON_PRESSED_HISTORY (0x003F)
#define BUTTON_RELEASED_HISTORY (0xF000)
#define BUTTON_ACTIVITY_MASK (BUTTON_PRESSED_HISTORY | BUTTON_RELEASED_HISTORY)
#define BUTTON_LONG_PRESS_DURATION_US (1000 * 1000)
#define BUTTON_QUEUE_DEPTH (16)
#define BUTTON_QUEUE_MAX_WAIT_TIME (1000)

typedef enum
{
        BUTTON_DOWN,
        BUTTON_UP,
        BUTTON_LONG,
        BUTTON_ERROR
} button_state_t;

static const char __attribute__((unused)) * BUTTON_STATE_STRING[] = {
    "BUTTON_DOWN",
    "BUTTON_UP",
    "BUTTON_LONG",
    "BUTTON_ERROR"};

typedef struct
{
        gpio_num_t pin : 8;
        button_state_t prev_state : 8;
        button_state_t new_state : 8;
} __packed button_event_t;

QueueHandle_t button_init(unsigned long long pin_select);
void button_deinit(void);
