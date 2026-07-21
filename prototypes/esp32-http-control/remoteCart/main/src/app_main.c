#include "esp_err.h"
#include "motor_control.h"
#include "nvs_flash.h"
#include "web_server.h"
#include "wifi_ap.h"

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
    ESP_ERROR_CHECK(wifi_ap_start());

    esp_err_t result = web_server_start();

    if (result != ESP_OK)
    {
        motor_control_stop();
        ESP_ERROR_CHECK(result);
    }
}
