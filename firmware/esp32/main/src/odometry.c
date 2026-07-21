#include "odometry.h"

#include <math.h>
#include <stddef.h>

#include "freertos/FreeRTOS.h"
#include "robot_config.h"

static odometry_state_t s_state;
static portMUX_TYPE s_odometry_mux = portMUX_INITIALIZER_UNLOCKED;

static double normalize_angle(double angle)
{
    return atan2(sin(angle), cos(angle));
}

void odometry_reset(void)
{
    portENTER_CRITICAL(&s_odometry_mux);
    s_state = (odometry_state_t){0};
    portEXIT_CRITICAL(&s_odometry_mux);
}

void odometry_update(
    double left_distance,
    double right_distance,
    double delta_time
)
{
    odometry_state_t current;

    odometry_get_state(&current);

    double center_distance = (left_distance + right_distance) / 2.0;
    double delta_theta =
        (right_distance - left_distance) / ROBOT_TRACK_WIDTH_M;
    double middle_theta = current.theta + delta_theta / 2.0;

    odometry_state_t next = {
        .x = current.x + center_distance * cos(middle_theta),
        .y = current.y + center_distance * sin(middle_theta),
        .theta = normalize_angle(current.theta + delta_theta),
        .linear_velocity =
            delta_time > 0.0 ? center_distance / delta_time : 0.0,
        .angular_velocity =
            delta_time > 0.0 ? delta_theta / delta_time : 0.0,
    };

    portENTER_CRITICAL(&s_odometry_mux);
    s_state = next;
    portEXIT_CRITICAL(&s_odometry_mux);
}

void odometry_get_state(odometry_state_t *state)
{
    if (state == NULL)
    {
        return;
    }

    portENTER_CRITICAL(&s_odometry_mux);
    *state = s_state;
    portEXIT_CRITICAL(&s_odometry_mux);
}
