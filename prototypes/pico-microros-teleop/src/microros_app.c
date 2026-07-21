#include "microros_app.h"

#include <stdio.h>

#include "geometry_msgs/msg/twist.h"
#include "motor_control.h"
#include "pico/stdlib.h"
#include "rcl/rcl.h"
#include "rclc/executor.h"
#include "rclc/rclc.h"

static void command_callback(const void *message_input)
{
    const geometry_msgs__msg__Twist *message =
        message_input;

    motor_control_set_command(
        (float)message->linear.x,
        (float)message->angular.z
    );
}

static bool check_result(
    rcl_ret_t result,
    const char *operation
)
{
    if (result == RCL_RET_OK)
    {
        return true;
    }

    printf(
        "Falha micro-ROS em %s: %d\n",
        operation,
        (int)result
    );

    return false;
}

int microros_app_run(
    picow_udp_transport_t *transport
)
{
    (void)transport;

    rcl_allocator_t allocator =
        rcl_get_default_allocator();

    rclc_support_t support;
    rcl_node_t node = rcl_get_zero_initialized_node();
    rcl_subscription_t subscription =
        rcl_get_zero_initialized_subscription();
    rclc_executor_t executor =
        rclc_executor_get_zero_initialized_executor();

    geometry_msgs__msg__Twist message;
    bool support_initialized = false;
    bool node_initialized = false;
    bool subscription_initialized = false;
    bool executor_initialized = false;
    bool message_initialized = false;
    int status = 1;

    rcl_ret_t result = rclc_support_init(
        &support,
        0,
        NULL,
        &allocator
    );

    if (!check_result(result, "support_init"))
    {
        goto cleanup;
    }

    support_initialized = true;

    result = rclc_node_init_default(
        &node,
        "pico_car_teleop",
        "",
        &support
    );

    if (!check_result(result, "node_init"))
    {
        goto cleanup;
    }

    node_initialized = true;

    if (!geometry_msgs__msg__Twist__init(&message))
    {
        printf("Falha ao inicializar mensagem Twist\n");
        goto cleanup;
    }

    message_initialized = true;

    result = rclc_subscription_init_default(
        &subscription,
        &node,
        ROSIDL_GET_MSG_TYPE_SUPPORT(
            geometry_msgs,
            msg,
            Twist
        ),
        "cmd_vel"
    );

    if (!check_result(result, "subscription_init"))
    {
        goto cleanup;
    }

    subscription_initialized = true;

    result = rclc_executor_init(
        &executor,
        &support.context,
        1,
        &allocator
    );

    if (!check_result(result, "executor_init"))
    {
        goto cleanup;
    }

    executor_initialized = true;

    result = rclc_executor_add_subscription(
        &executor,
        &subscription,
        &message,
        command_callback,
        ON_NEW_DATA
    );

    if (!check_result(result, "add_subscription"))
    {
        goto cleanup;
    }

    printf("Aguardando comandos em /cmd_vel\n");
    status = 0;

    while (true)
    {
        result = rclc_executor_spin_some(
            &executor,
            RCL_MS_TO_NS(20)
        );

        motor_control_poll();

        if (
            result != RCL_RET_OK &&
            result != RCL_RET_TIMEOUT
        )
        {
            check_result(result, "executor_spin");
            status = 1;
            break;
        }

        sleep_ms(5);
    }

cleanup:
    motor_control_stop();

    if (executor_initialized)
    {
        rclc_executor_fini(&executor);
    }

    if (subscription_initialized)
    {
        rcl_ret_t cleanup_result = rcl_subscription_fini(
            &subscription,
            &node
        );

        if (cleanup_result != RCL_RET_OK)
        {
            printf(
                "Falha ao finalizar subscription: %d\n",
                (int)cleanup_result
            );
        }
    }

    if (message_initialized)
    {
        geometry_msgs__msg__Twist__fini(&message);
    }

    if (node_initialized)
    {
        rcl_ret_t cleanup_result = rcl_node_fini(&node);

        if (cleanup_result != RCL_RET_OK)
        {
            printf(
                "Falha ao finalizar node: %d\n",
                (int)cleanup_result
            );
        }
    }

    if (support_initialized)
    {
        rclc_support_fini(&support);
    }

    return status;
}
