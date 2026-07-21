#pragma once

typedef struct
{
    double x;
    double y;
    double theta;
    double linear_velocity;
    double angular_velocity;
} odometry_state_t;

void odometry_reset(void);
void odometry_update(
    double left_distance,
    double right_distance,
    double delta_time
);
void odometry_get_state(odometry_state_t *state);
