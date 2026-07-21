#include "microros_node.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "geometry_msgs/msg/twist.h"
#include "nav_msgs/msg/odometry.h"
#include "odometry.h"
#include "rcl/rcl.h"
#include "rclc/executor.h"
#include "rclc/rclc.h"
#include "rmw_microros/rmw_microros.h"
#include "robot_config.h"
#include "robot_control.h"
#include "rosidl_runtime_c/string_functions.h"
#include "udp_transport.h"
#include "wifi_manager.h"

#if !defined(RMW_UXRCE_TRANSPORT_CUSTOM)
#error "RMW_UXRCE_TRANSPORT must be configured as custom"
#endif

#define RCL_CHECK(call)                                      \
    do                                                       \
    {                                                        \
        rcl_ret_t result = (call);                           \
        if (result != RCL_RET_OK)                            \
        {                                                    \
            ESP_LOGE(                                       \
                TAG,                                        \
                "Falha micro-ROS na linha %d: %d",          \
                __LINE__,                                   \
                (int)result                                 \
            );                                              \
            vTaskDelete(NULL);                              \
            return;                                         \
        }                                                    \
    } while (0)

static const char *TAG = "MICROROS";
static rcl_publisher_t s_odometry_publisher;

static udp_transport_context_t s_transport_context = {
    .socket_fd = -1,
    .agent_ip = ROBOT_AGENT_IP,
    .agent_port = ROBOT_AGENT_PORT,
};

static void command_callback(const void *message_input)
{
    const geometry_msgs__msg__Twist *message = message_input;

    esp_err_t result = robot_control_set_command(
        message->linear.x,
        message->angular.z
    );

    if (result != ESP_OK)
    {
        ESP_LOGW(TAG, "Comando de velocidade descartado");
    }
}

static void fill_odometry_message(
    nav_msgs__msg__Odometry *message
)
{
    odometry_state_t state;
    odometry_get_state(&state);

    int64_t timestamp = esp_timer_get_time();

    message->header.stamp.sec = timestamp / 1000000;
    message->header.stamp.nanosec =
        (timestamp % 1000000) * 1000;

    message->pose.pose.position.x = state.x;
    message->pose.pose.position.y = state.y;
    message->pose.pose.position.z = 0.0;

    message->pose.pose.orientation.x = 0.0;
    message->pose.pose.orientation.y = 0.0;
    message->pose.pose.orientation.z =
        sin(state.theta / 2.0);
    message->pose.pose.orientation.w =
        cos(state.theta / 2.0);

    message->twist.twist.linear.x =
        state.linear_velocity;
    message->twist.twist.angular.z =
        state.angular_velocity;
}

static void microros_task(void *argument)
{
    (void)argument;

    wifi_manager_wait_connected();

    rmw_ret_t transport_result =
        rmw_uros_set_custom_transport(
            false,
            &s_transport_context,
            udp_transport_open,
            udp_transport_close,
            udp_transport_write,
            udp_transport_read
        );

    if (transport_result != RMW_RET_OK)
    {
        ESP_LOGE(TAG, "Falha ao configurar transporte");
        vTaskDelete(NULL);
        return;
    }

    while (
        rmw_uros_ping_agent(100, 1) != RMW_RET_OK
    )
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    rcl_allocator_t allocator =
        rcl_get_default_allocator();

    rclc_support_t support;
    rcl_node_t node;
    rcl_subscription_t command_subscription;
    rclc_executor_t executor;

    RCL_CHECK(
        rclc_support_init(
            &support,
            0,
            NULL,
            &allocator
        )
    );

    RCL_CHECK(
        rclc_node_init_default(
            &node,
            "esp32_robot",
            "",
            &support
        )
    );

    nav_msgs__msg__Odometry odometry_message;

    if (
        !nav_msgs__msg__Odometry__init(
            &odometry_message
        )
    )
    {
        ESP_LOGE(TAG, "Falha ao inicializar odometria");
        vTaskDelete(NULL);
        return;
    }

    if (
        !rosidl_runtime_c__String__assign(
            &odometry_message.header.frame_id,
            "odom"
        ) ||
        !rosidl_runtime_c__String__assign(
            &odometry_message.child_frame_id,
            "base_link"
        )
    )
    {
        ESP_LOGE(TAG, "Falha ao configurar frames");
        vTaskDelete(NULL);
        return;
    }

    RCL_CHECK(
        rclc_publisher_init_default(
            &s_odometry_publisher,
            &node,
            ROSIDL_GET_MSG_TYPE_SUPPORT(
                nav_msgs,
                msg,
                Odometry
            ),
            "odom"
        )
    );

    geometry_msgs__msg__Twist command_message;

    if (
        !geometry_msgs__msg__Twist__init(
            &command_message
        )
    )
    {
        ESP_LOGE(TAG, "Falha ao inicializar cmd_vel");
        vTaskDelete(NULL);
        return;
    }

    RCL_CHECK(
        rclc_subscription_init_default(
            &command_subscription,
            &node,
            ROSIDL_GET_MSG_TYPE_SUPPORT(
                geometry_msgs,
                msg,
                Twist
            ),
            "cmd_vel"
        )
    );

    RCL_CHECK(
        rclc_executor_init(
            &executor,
            &support.context,
            1,
            &allocator
        )
    );

    RCL_CHECK(
        rclc_executor_add_subscription(
            &executor,
            &command_subscription,
            &command_message,
            command_callback,
            ON_NEW_DATA
        )
    );

    while (true)
    {
        fill_odometry_message(&odometry_message);

        rcl_ret_t publish_result = rcl_publish(
            &s_odometry_publisher,
            &odometry_message,
            NULL
        );

        if (publish_result != RCL_RET_OK)
        {
            ESP_LOGW(
                TAG,
                "Falha ao publicar odometria: %d",
                (int)publish_result
            );
        }

        rcl_ret_t spin_result =
            rclc_executor_spin_some(
                &executor,
                RCL_MS_TO_NS(10)
            );

        if (
            spin_result != RCL_RET_OK &&
            spin_result != RCL_RET_TIMEOUT
        )
        {
            ESP_LOGW(
                TAG,
                "Falha no executor: %d",
                (int)spin_result
            );
        }

        vTaskDelay(
            pdMS_TO_TICKS(
                ROBOT_CONTROL_PERIOD_MS
            )
        );
    }
}

esp_err_t microros_node_start(void)
{
    BaseType_t result = xTaskCreatePinnedToCore(
        microros_task,
        "microros",
        ROBOT_MICROROS_TASK_STACK,
        NULL,
        ROBOT_MICROROS_TASK_PRIORITY,
        NULL,
        ROBOT_MICROROS_TASK_CORE
    );

    return result == pdPASS
        ? ESP_OK
        : ESP_ERR_NO_MEM;
}
