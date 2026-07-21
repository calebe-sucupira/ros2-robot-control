#pragma once

#include "esp_err.h"

typedef enum
{
    MOTOR_COMMAND_STOP,
    MOTOR_COMMAND_FORWARD,
    MOTOR_COMMAND_REVERSE,
    MOTOR_COMMAND_LEFT,
    MOTOR_COMMAND_RIGHT,
} motor_command_t;

esp_err_t motor_control_init(void);
esp_err_t motor_control_apply(motor_command_t command);
void motor_control_stop(void);
