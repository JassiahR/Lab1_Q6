#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_sleep.h"
#include "driver/gpio.h"
#include "driver/hw_timer.h"
#include "driver/uart.h"

#define BUF_SIZE       1024
#define TEST_RELOAD      true
#define FOREVER 1
#define USER_CHAR "o"
#define ON_STATE 0
#define OFF_STATE 1

struct time_store {
    int min;
    int second;
    int millisec;
};

struct time_store current_time;
int count;
unsigned int events, state, state_events;
struct time_store cooktime, targettick;
uint32_t tps;
uint8_t *key;
int interval;

void config_input() {
    gpio_config_t config_input;

    config_input.mode = GPIO_MODE_DEF_INPUT;
    config_input.pull_up_en = GPIO_PULLUP_DISABLE;
    config_input.pull_down_en = GPIO_PULLDOWN_DISABLE;
    config_input.intr_type = GPIO_INTR_DISABLE;
    config_input.pin_bit_mask = (1ULL << GPIO_NUM_0);
    gpio_config(&config_input);
}

void init_time_store(struct time_store *time) {
    time->min = 0;
    time->second = 0;
    time->millisec = 0;
}

void hw_timer_callback1(void *arg) {
    count++;
    if (count == 10) {
        current_time.millisec++;
        if (current_time.millisec == 501) {
            current_time.millisec = 0;
        }
        count = 0;
    }
}

int time(struct time_store times) {
    return times.millisec;
}

bool clock_tick() {
    int temp = time(current_time);
    interval++;
    if (interval > 5) {
        interval = 0;
    }
    return (temp % 100 == 0);
}

void hw_timer_config() {
    hw_timer_init(hw_timer_callback1, NULL);
    hw_timer_alarm_us(100, TEST_RELOAD);
    hw_timer_get_intr_type(1);
    hw_timer_set_reload(1);
    hw_timer_set_load_data(100);
}

void config_uart() {
    uart_config_t config_uart;
    config_uart.baud_rate = 74880;
    config_uart.data_bits = UART_DATA_8_BITS;
    config_uart.parity = UART_PARITY_DISABLE;
    config_uart.stop_bits = UART_STOP_BITS_1;
    config_uart.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    uart_param_config(UART_NUM_0, &config_uart);
    uart_driver_install(UART_NUM_0, BUF_SIZE * 2, 0, 0, NULL, 0);
}

uint8_t *serial_receive() {
    uint8_t *data = (uint8_t *)malloc(BUF_SIZE);
    int len = uart_read_bytes(UART_NUM_0, data, BUF_SIZE, 100 / portTICK_RATE_MS);
    return data;
}

void On_State() {
    const char *message = "ON_STATE \n";
    uart_write_bytes(UART_NUM_0, message, strlen(message));
    gpio_set_level(GPIO_NUM_2, 1);
}

void Off_state() {
    const char *message = "OFF_STATE \n";
    uart_write_bytes(UART_NUM_0, message, strlen(message));
    gpio_set_level(GPIO_NUM_2, 0);
}

// Define function pointer type for state transitions
typedef void (*state_fn)();

// Define a structure for the state table
typedef struct {
    state_fn action;
    uint8_t next_state;
} state_transition;

// Define transition actions for each state
void handle_on_state() {
    key = serial_receive();
    if (key == (uint8_t *)USER_CHAR && interval >= 5 && clock_tick()) {
        state = OFF_STATE;
    } else if (key != (uint8_t *)USER_CHAR && clock_tick()) {
        state = OFF_STATE;
    } else {
        On_State();
    }
}

void handle_off_state() {
    key = serial_receive();
    if (key == (uint8_t *)USER_CHAR && interval == 5 && clock_tick()) {
        state = ON_STATE;
    } else if (key != (uint8_t *)USER_CHAR && clock_tick()) {
        state = ON_STATE;
    } else {
        Off_state();
    }
}

// Define state transition table
state_transition state_table[] = {
    {handle_on_state, OFF_STATE},
    {handle_off_state, ON_STATE}
};

void app_main() {
    uint8_t data2 = (uint8_t)USER_CHAR;
    init_time_store(&current_time);
    count = 0;
    state = ON_STATE;

    config_input();
    config_uart();
    hw_timer_config();

    while (FOREVER) {
        key = serial_receive();
        printf("StateA = %2d \r", state);
        state_table[state].action();  // Call the action for the current state
    }
}
