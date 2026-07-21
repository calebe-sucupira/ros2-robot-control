#include "motor_control.h"

#include <math.h>
#include <stdbool.h>

#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "pico/stdlib.h"
#include "remote_cart_config.h"

#define LEFT_INPUT_A 18
#define LEFT_INPUT_B 19
#define RIGHT_INPUT_A 20
#define RIGHT_INPUT_B 16

#define LEFT_PWM_GPIO 9
#define RIGHT_PWM_GPIO 8

#define PWM_FREQUENCY_HZ 5000
#define PWM_WRAP 999
#define INPUT_DEADZONE 0.05f
#define OUTPUT_DEADZONE 0.01f

typedef struct
{
    uint slice;
    uint channel;
    uint input_a;
    uint input_b;
    bool inverted;
} motor_t;

static motor_t s_left_motor;
static motor_t s_right_motor;
static absolute_time_t s_last_command_time;
static bool s_command_active;

static float clamp(float value, float minimum, float maximum)
{
    return fmaxf(minimum, fminf(value, maximum));
}

static void configure_direction_pin(uint gpio)
{
    gpio_init(gpio);
    gpio_set_dir(gpio, GPIO_OUT);
    gpio_put(gpio, 0);
}

static void apply_motor(
    const motor_t *motor,
    float speed
)
{
    speed = clamp(speed, -1.0f, 1.0f);

    if (motor->inverted)
    {
        speed = -speed;
    }

    uint16_t level =
        (uint16_t)(fabsf(speed) * PWM_WRAP);

    if (speed > OUTPUT_DEADZONE)
    {
        gpio_put(motor->input_a, 1);
        gpio_put(motor->input_b, 0);
    }
    else if (speed < -OUTPUT_DEADZONE)
    {
        gpio_put(motor->input_a, 0);
        gpio_put(motor->input_b, 1);
    }
    else
    {
        gpio_put(motor->input_a, 0);
        gpio_put(motor->input_b, 0);
        level = 0;
    }

    pwm_set_chan_level(
        motor->slice,
        motor->channel,
        level
    );
}

void motor_control_init(void)
{
    configure_direction_pin(LEFT_INPUT_A);
    configure_direction_pin(LEFT_INPUT_B);
    configure_direction_pin(RIGHT_INPUT_A);
    configure_direction_pin(RIGHT_INPUT_B);

    gpio_set_function(LEFT_PWM_GPIO, GPIO_FUNC_PWM);
    gpio_set_function(RIGHT_PWM_GPIO, GPIO_FUNC_PWM);

    s_left_motor = (motor_t){
        .slice = pwm_gpio_to_slice_num(LEFT_PWM_GPIO),
        .channel = pwm_gpio_to_channel(LEFT_PWM_GPIO),
        .input_a = LEFT_INPUT_A,
        .input_b = LEFT_INPUT_B,
        .inverted = false,
    };

    s_right_motor = (motor_t){
        .slice = pwm_gpio_to_slice_num(RIGHT_PWM_GPIO),
        .channel = pwm_gpio_to_channel(RIGHT_PWM_GPIO),
        .input_a = RIGHT_INPUT_A,
        .input_b = RIGHT_INPUT_B,
        .inverted = true,
    };

    float divider =
        (float)clock_get_hz(clk_sys) /
        (PWM_FREQUENCY_HZ * (PWM_WRAP + 1));

    pwm_set_clkdiv(s_left_motor.slice, divider);
    pwm_set_wrap(s_left_motor.slice, PWM_WRAP);

    if (s_right_motor.slice != s_left_motor.slice)
    {
        pwm_set_clkdiv(s_right_motor.slice, divider);
        pwm_set_wrap(s_right_motor.slice, PWM_WRAP);
    }

    pwm_set_enabled(s_left_motor.slice, true);

    if (s_right_motor.slice != s_left_motor.slice)
    {
        pwm_set_enabled(s_right_motor.slice, true);
    }

    motor_control_stop();
}

void motor_control_set_command(float linear, float angular)
{
    if (fabsf(linear) < INPUT_DEADZONE)
    {
        linear = 0.0f;
    }

    if (fabsf(angular) < INPUT_DEADZONE)
    {
        angular = 0.0f;
    }

    float left = linear - angular;
    float right = linear + angular;

    float scale = fmaxf(
        1.0f,
        fmaxf(fabsf(left), fabsf(right))
    );

    apply_motor(&s_left_motor, left / scale);
    apply_motor(&s_right_motor, right / scale);

    s_command_active =
        fabsf(left) > OUTPUT_DEADZONE ||
        fabsf(right) > OUTPUT_DEADZONE;

    s_last_command_time = get_absolute_time();
}

void motor_control_poll(void)
{
    if (!s_command_active)
    {
        return;
    }

    int64_t elapsed_us = absolute_time_diff_us(
        s_last_command_time,
        get_absolute_time()
    );

    if (
        elapsed_us >=
        REMOTE_CART_COMMAND_TIMEOUT_MS * 1000
    )
    {
        motor_control_stop();
    }
}

void motor_control_stop(void)
{
    apply_motor(&s_left_motor, 0.0f);
    apply_motor(&s_right_motor, 0.0f);
    s_command_active = false;
}
