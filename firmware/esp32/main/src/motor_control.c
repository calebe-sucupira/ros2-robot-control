#include "motor_control.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "robot_config.h"

static double clamp(double value, double minimum, double maximum)
{
    return fmax(minimum, fmin(value, maximum));
}

static void apply_motor(
    double power,
    gpio_num_t input_a,
    gpio_num_t input_b,
    ledc_channel_t channel,
    bool forward_a,
    bool forward_b
)
{
    if (fabs(power) < 0.01)
    {
        gpio_set_level(input_a, 0);
        gpio_set_level(input_b, 0);
        ledc_set_duty(ROBOT_PWM_MODE, channel, ROBOT_PWM_MAX_DUTY);
        ledc_update_duty(ROBOT_PWM_MODE, channel);
        return;
    }

    bool forward = power > 0.0;

    gpio_set_level(input_a, forward ? forward_a : !forward_a);
    gpio_set_level(input_b, forward ? forward_b : !forward_b);

    double magnitude = clamp(
        fabs(power) + ROBOT_MOTOR_DEADZONE_PERCENT,
        0.0,
        ROBOT_MOTOR_MAX_PERCENT
    );

    uint32_t duty = ROBOT_PWM_MAX_DUTY -
        (uint32_t)(ROBOT_PWM_MAX_DUTY * magnitude / 100.0);

    ledc_set_duty(ROBOT_PWM_MODE, channel, duty);
    ledc_update_duty(ROBOT_PWM_MODE, channel);
}

esp_err_t motor_control_init(void)
{
    gpio_config_t direction_config = {
        .pin_bit_mask =
            (1ULL << ROBOT_LEFT_IN_A_GPIO) |
            (1ULL << ROBOT_LEFT_IN_B_GPIO) |
            (1ULL << ROBOT_RIGHT_IN_A_GPIO) |
            (1ULL << ROBOT_RIGHT_IN_B_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    esp_err_t result = gpio_config(&direction_config);

    if (result != ESP_OK)
    {
        return result;
    }

    ledc_timer_config_t timer_config = {
        .speed_mode = ROBOT_PWM_MODE,
        .duty_resolution = ROBOT_PWM_RESOLUTION,
        .timer_num = ROBOT_PWM_TIMER,
        .freq_hz = ROBOT_PWM_FREQUENCY_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };

    result = ledc_timer_config(&timer_config);

    if (result != ESP_OK)
    {
        return result;
    }

    ledc_channel_config_t right_channel = {
        .gpio_num = ROBOT_RIGHT_PWM_GPIO,
        .speed_mode = ROBOT_PWM_MODE,
        .channel = ROBOT_RIGHT_PWM_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = ROBOT_PWM_TIMER,
        .duty = ROBOT_PWM_MAX_DUTY,
        .hpoint = 0,
    };

    result = ledc_channel_config(&right_channel);

    if (result != ESP_OK)
    {
        return result;
    }

    ledc_channel_config_t left_channel = {
        .gpio_num = ROBOT_LEFT_PWM_GPIO,
        .speed_mode = ROBOT_PWM_MODE,
        .channel = ROBOT_LEFT_PWM_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = ROBOT_PWM_TIMER,
        .duty = ROBOT_PWM_MAX_DUTY,
        .hpoint = 0,
    };

    result = ledc_channel_config(&left_channel);

    if (result != ESP_OK)
    {
        return result;
    }

    motor_control_stop();
    return ESP_OK;
}

void motor_control_set(double left_percent, double right_percent)
{
    apply_motor(
        left_percent,
        ROBOT_LEFT_IN_A_GPIO,
        ROBOT_LEFT_IN_B_GPIO,
        ROBOT_LEFT_PWM_CHANNEL,
        false,
        true
    );

    apply_motor(
        right_percent,
        ROBOT_RIGHT_IN_A_GPIO,
        ROBOT_RIGHT_IN_B_GPIO,
        ROBOT_RIGHT_PWM_CHANNEL,
        true,
        false
    );
}

void motor_control_stop(void)
{
    motor_control_set(0.0, 0.0);
}
