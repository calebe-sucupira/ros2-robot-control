#include <stdio.h>

#include "microros_app.h"
#include "motor_control.h"
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "picow_udp_transport.h"
#include "remote_cart_config.h"
#include "rmw_microros/rmw_microros.h"

static bool connect_wifi(void)
{
    printf(
        "Conectando à rede %s\n",
        REMOTE_CART_WIFI_SSID
    );

    int result = cyw43_arch_wifi_connect_timeout_ms(
        REMOTE_CART_WIFI_SSID,
        REMOTE_CART_WIFI_PASSWORD,
        CYW43_AUTH_WPA2_AES_PSK,
        30000
    );

    if (result != 0)
    {
        printf("Falha ao conectar ao Wi-Fi: %d\n", result);
        return false;
    }

    printf("Wi-Fi conectado\n");
    return true;
}

static void wait_for_agent(void)
{
    printf(
        "Procurando micro-ROS Agent em %s:%d\n",
        REMOTE_CART_AGENT_IP,
        REMOTE_CART_AGENT_PORT
    );

    bool led_on = false;

    while (
        rmw_uros_ping_agent(1000, 1) != RMW_RET_OK
    )
    {
        led_on = !led_on;
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led_on);
        sleep_ms(1000);
    }

    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, true);
    printf("micro-ROS Agent conectado\n");
}

int main(void)
{
    stdio_init_all();
    motor_control_init();

    if (cyw43_arch_init() != 0)
    {
        printf("Falha ao inicializar o Wi-Fi\n");
        return 1;
    }

    cyw43_arch_enable_sta_mode();

    if (!connect_wifi())
    {
        cyw43_arch_deinit();
        return 1;
    }

    static picow_udp_transport_t transport;

    if (
        !picow_udp_transport_init(
            &transport,
            REMOTE_CART_AGENT_IP,
            REMOTE_CART_AGENT_PORT
        )
    )
    {
        printf("Configuração inválida do transporte UDP\n");
        cyw43_arch_deinit();
        return 1;
    }

    rmw_ret_t transport_result =
        rmw_uros_set_custom_transport(
            false,
            &transport,
            picow_udp_transport_open,
            picow_udp_transport_close,
            picow_udp_transport_write,
            picow_udp_transport_read
        );

    if (transport_result != RMW_RET_OK)
    {
        printf("Falha ao configurar transporte micro-ROS\n");
        cyw43_arch_deinit();
        return 1;
    }

    wait_for_agent();

    int result = microros_app_run(&transport);

    motor_control_stop();
    cyw43_arch_deinit();

    return result;
}
