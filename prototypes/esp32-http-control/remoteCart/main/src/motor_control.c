#include "motor_control.h"

#include <stdbool.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "sdkconfig.h"

#define LEFT_MOTOR_INPUT_A GPIO_NUM_26
#define LEFT_MOTOR_INPUT_B GPIO_NUM_25
#define RIGHT_MOTOR_INPUT_A GPIO_NUM_33
#define RIGHT_MOTOR_INPUT_B GPIO_NUM_32

#define WATCHDOG_PERIOD_US 50000

static esp_timer_handle_t s_watchdog_timer;
static int64_t s_last_command_time;
static bool s_moving;
static portMUX_TYPE s_motor_mux = portMUX_INITIALIZER_UNLOCKED;

static void set_outputs(
    int left_a,
    int left_b,
    int right_a,
    int right_b
)
{
    gpio_set_level(LEFT_MOTOR_INPUT_A, left_a);
    gpio_set_level(LEFT_MOTOR_INPUT_B, left_b);
    gpio_set_level(RIGHT_MOTOR_INPUT_A, right_a);
    gpio_set_level(RIGHT_MOTOR_INPUT_B, right_b);
}

static void command_watchdog_callback(void *argument)
{
    (void)argument;

    int64_t now = esp_timer_get_time();
    int64_t timeout_us =
        (int64_t)CONFIG_REMOTECART_COMMAND_TIMEOUT_MS * 1000;

    portENTER_CRITICAL(&s_motor_mux);

    if (
        s_moving &&
        now - s_last_command_time >= timeout_us
    )
    {
        set_outputs(0, 0, 0, 0);
        s_moving = false;
    }

    portEXIT_CRITICAL(&s_motor_mux);
}

esp_err_t motor_control_init(void)
{
    gpio_config_t config = {
        .pin_bit_mask =
            (1ULL << LEFT_MOTOR_INPUT_A) |
            (1ULL << LEFT_MOTOR_INPUT_B) |
            (1ULL << RIGHT_MOTOR_INPUT_A) |
            (1ULL << RIGHT_MOTOR_INPUT_B),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    esp_err_t result = gpio_config(&config);

    if (result != ESP_OK)
    {
        return result;
    }

    set_outputs(0, 0, 0, 0);

    esp_timer_create_args_t timer_args = {
        .callback = command_watchdog_callback,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "motor_watchdog",
    };

    result = esp_timer_create(
        &timer_args,
        &s_watchdog_timer
    );

    if (result != ESP_OK)
    {
        return result;
    }

    return esp_timer_start_periodic(
        s_watchdog_timer,
        WATCHDOG_PERIOD_US
    );
}

esp_err_t motor_control_apply(motor_command_t command)
{
    int left_a;
    int left_b;
    int right_a;
    int right_b;

    switch (command)
    {
        case MOTOR_COMMAND_STOP:
            left_a = 0;
            left_b = 0;
            right_a = 0;
            right_b = 0;
            break;

        case MOTOR_COMMAND_FORWARD:
            left_a = 1;
            left_b = 0;
            right_a = 0;
            right_b = 1;
            break;

        case MOTOR_COMMAND_REVERSE:
            left_a = 0;
            left_b = 1;
            right_a = 1;
            right_b = 0;
            break;

        case MOTOR_COMMAND_LEFT:
            left_a = 0;
            left_b = 1;
            right_a = 0;
            right_b = 1;
            break;

        case MOTOR_COMMAND_RIGHT:
            left_a = 1;
            left_b = 0;
            right_a = 1;
            right_b = 0;
            break;

        default:
            return ESP_ERR_INVALID_ARG;
    }

    portENTER_CRITICAL(&s_motor_mux);

    set_outputs(
        left_a,
        left_b,
        right_a,
        right_b
    );

    s_moving = command != MOTOR_COMMAND_STOP;
    s_last_command_time = esp_timer_get_time();

    portEXIT_CRITICAL(&s_motor_mux);

    return ESP_OK;
}

void motor_control_stop(void)
{
    (void)motor_control_apply(MOTOR_COMMAND_STOP);
}
