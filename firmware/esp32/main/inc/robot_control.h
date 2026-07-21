#pragma once

#include "esp_err.h"

esp_err_t robot_control_start(void);
esp_err_t robot_control_set_command(
    double linear_velocity,
    double angular_velocity
);
