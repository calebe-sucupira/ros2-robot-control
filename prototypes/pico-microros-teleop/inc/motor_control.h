#pragma once

void motor_control_init(void);
void motor_control_set_command(float linear, float angular);
void motor_control_poll(void);
void motor_control_stop(void);
