#include "encoder.h"
#include "esp_err.h"
#include "microros_node.h"
#include "motor_control.h"
#include "nvs_flash.h"
#include "odometry.h"
#include "robot_control.h"
#include "wifi_manager.h"

static void initialize_nvs(void)
{
    esp_err_t result = nvs_flash_init();

    if (
        result == ESP_ERR_NVS_NO_FREE_PAGES ||
        result == ESP_ERR_NVS_NEW_VERSION_FOUND
    )
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        result = nvs_flash_init();
    }

    ESP_ERROR_CHECK(result);
}

void app_main(void)
{
    initialize_nvs();

    ESP_ERROR_CHECK(motor_control_init());
    ESP_ERROR_CHECK(encoder_init());

    odometry_reset();

    ESP_ERROR_CHECK(wifi_manager_init());
    ESP_ERROR_CHECK(robot_control_start());
    ESP_ERROR_CHECK(microros_node_start());
}
