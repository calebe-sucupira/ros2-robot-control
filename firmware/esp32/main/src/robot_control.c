#include "robot_control.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>

#include "encoder.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "motor_control.h"
#include "odometry.h"
#include "robot_config.h"

typedef struct
{
    double setpoint;
    double filtered_velocity;
    double integral;
} wheel_controller_t;

typedef struct
{
    double left_setpoint;
    double right_setpoint;
    int64_t received_at;
} motion_command_t;

static const char *TAG = "ROBOT_CONTROL";
static QueueHandle_t s_command_queue;

static double clamp(double value, double minimum, double maximum)
{
    return fmax(minimum, fmin(value, maximum));
}

static int direction(double value)
{
    if (value > 0.0)
    {
        return 1;
    }

    if (value < 0.0)
    {
        return -1;
    }

    return 0;
}

static double controller_step(
    wheel_controller_t *controller,
    double setpoint,
    double feedforward,
    double delta_time
)
{
    controller->setpoint = setpoint;

    if (fabs(setpoint) < 0.01)
    {
        controller->integral = 0.0;
        return 0.0;
    }

    double error = setpoint - controller->filtered_velocity;

    controller->integral = clamp(
        controller->integral + error * delta_time,
        -ROBOT_INTEGRAL_LIMIT,
        ROBOT_INTEGRAL_LIMIT
    );

    return setpoint * feedforward +
        ROBOT_KP * error +
        ROBOT_KI * controller->integral;
}

static bool should_synchronize(double left_setpoint, double right_setpoint)
{
    if (left_setpoint * right_setpoint <= 0.0)
    {
        return false;
    }

    double reference = fmax(fabs(left_setpoint), fabs(right_setpoint));

    if (reference < 0.01)
    {
        return false;
    }

    return fabs(left_setpoint - right_setpoint) <= reference * 0.1;
}

static void control_task(void *argument)
{
    (void)argument;

    wheel_controller_t left_controller = {0};
    wheel_controller_t right_controller = {0};
    motion_command_t command = {0};

    int32_t previous_left_count = 0;
    int32_t previous_right_count = 0;
    int previous_left_direction = 0;
    int previous_right_direction = 0;

    int64_t previous_cycle_time = esp_timer_get_time();
    TickType_t last_wake_time = xTaskGetTickCount();

    encoder_get_counts(
        &previous_left_count,
        &previous_right_count
    );

    while (true)
    {
        motion_command_t received_command;

        if (
            xQueueReceive(
                s_command_queue,
                &received_command,
                0
            ) == pdTRUE
        )
        {
            command = received_command;
        }

        int64_t now = esp_timer_get_time();
        double delta_time =
            (double)(now - previous_cycle_time) / 1000000.0;
        previous_cycle_time = now;

        bool command_active =
            command.received_at > 0 &&
            now - command.received_at <= ROBOT_COMMAND_TIMEOUT_US;

        double left_setpoint =
            command_active ? command.left_setpoint : 0.0;
        double right_setpoint =
            command_active ? command.right_setpoint : 0.0;

        int32_t left_count;
        int32_t right_count;

        encoder_get_counts(&left_count, &right_count);

        int32_t left_delta = left_count - previous_left_count;
        int32_t right_delta = right_count - previous_right_count;

        previous_left_count = left_count;
        previous_right_count = right_count;

        // Encoders de canal único não fornecem o sentido de rotação.
        int32_t signed_left_delta =
            left_delta * previous_left_direction;
        int32_t signed_right_delta =
            right_delta * previous_right_direction;

        double left_distance =
            signed_left_delta * ROBOT_METERS_PER_TICK;
        double right_distance =
            signed_right_delta * ROBOT_METERS_PER_TICK;

        odometry_update(
            left_distance,
            right_distance,
            delta_time
        );

        double left_raw_velocity =
            delta_time > 0.0
                ? signed_left_delta / delta_time
                : 0.0;

        double right_raw_velocity =
            delta_time > 0.0
                ? signed_right_delta / delta_time
                : 0.0;

        left_controller.filtered_velocity =
            left_controller.filtered_velocity * ROBOT_FILTER_ALPHA +
            left_raw_velocity * (1.0 - ROBOT_FILTER_ALPHA);

        right_controller.filtered_velocity =
            right_controller.filtered_velocity * ROBOT_FILTER_ALPHA +
            right_raw_velocity * (1.0 - ROBOT_FILTER_ALPHA);

        double left_output = controller_step(
            &left_controller,
            left_setpoint,
            ROBOT_LEFT_FEEDFORWARD,
            delta_time
        );

        double right_output = controller_step(
            &right_controller,
            right_setpoint,
            ROBOT_RIGHT_FEEDFORWARD,
            delta_time
        );

        if (should_synchronize(left_setpoint, right_setpoint))
        {
            double synchronization_error =
                left_controller.filtered_velocity -
                right_controller.filtered_velocity;

            double correction = clamp(
                synchronization_error * ROBOT_SYNC_GAIN,
                -4.0,
                4.0
            );

            left_output -= correction;
            right_output += correction;
        }

        motor_control_set(left_output, right_output);

        previous_left_direction = direction(left_setpoint);
        previous_right_direction = direction(right_setpoint);

        vTaskDelayUntil(
            &last_wake_time,
            pdMS_TO_TICKS(ROBOT_CONTROL_PERIOD_MS)
        );
    }
}

esp_err_t robot_control_start(void)
{
    s_command_queue = xQueueCreate(
        1,
        sizeof(motion_command_t)
    );

    if (s_command_queue == NULL)
    {
        return ESP_ERR_NO_MEM;
    }

    BaseType_t result = xTaskCreatePinnedToCore(
        control_task,
        "robot_control",
        ROBOT_CONTROL_TASK_STACK,
        NULL,
        ROBOT_CONTROL_TASK_PRIORITY,
        NULL,
        ROBOT_CONTROL_TASK_CORE
    );

    if (result != pdPASS)
    {
        vQueueDelete(s_command_queue);
        s_command_queue = NULL;
        ESP_LOGE(TAG, "Falha ao criar task de controle");
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

esp_err_t robot_control_set_command(
    double linear_velocity,
    double angular_velocity
)
{
    if (s_command_queue == NULL)
    {
        return ESP_ERR_INVALID_STATE;
    }

    motion_command_t command = {
        .left_setpoint =
            (
                linear_velocity -
                angular_velocity * ROBOT_TRACK_WIDTH_M / 2.0
            ) * ROBOT_TICKS_PER_METER,
        .right_setpoint =
            (
                linear_velocity +
                angular_velocity * ROBOT_TRACK_WIDTH_M / 2.0
            ) * ROBOT_TICKS_PER_METER,
        .received_at = esp_timer_get_time(),
    };

    return xQueueOverwrite(
        s_command_queue,
        &command
    ) == pdPASS
        ? ESP_OK
        : ESP_FAIL;
}
