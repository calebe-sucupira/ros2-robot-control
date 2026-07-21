#pragma once

#include "esp_err.h"

esp_err_t motor_control_init(void);
void motor_control_set(double left_percent, double right_percent);
void motor_control_stop(void);
