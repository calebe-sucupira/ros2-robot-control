#include "wifi_ap.h"

#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "sdkconfig.h"

static const char *TAG = "WIFI_AP";

esp_err_t wifi_ap_start(void)
{
    size_t password_length =
        strlen(CONFIG_REMOTECART_WIFI_PASSWORD);

    if (
        password_length > 0 &&
        password_length < 8
    )
    {
        ESP_LOGE(
            TAG,
            "A senha do ponto de acesso deve ter ao menos 8 caracteres"
        );
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t result = esp_netif_init();

    if (result != ESP_OK)
    {
        return result;
    }

    result = esp_event_loop_create_default();

    if (result != ESP_OK)
    {
        return result;
    }

    esp_netif_t *network_interface =
        esp_netif_create_default_wifi_ap();

    if (network_interface == NULL)
    {
        return ESP_FAIL;
    }

    wifi_init_config_t init_config =
        WIFI_INIT_CONFIG_DEFAULT();

    result = esp_wifi_init(&init_config);

    if (result != ESP_OK)
    {
        return result;
    }

    wifi_config_t wifi_config = {0};

    strlcpy(
        (char *)wifi_config.ap.ssid,
        CONFIG_REMOTECART_WIFI_SSID,
        sizeof(wifi_config.ap.ssid)
    );

    strlcpy(
        (char *)wifi_config.ap.password,
        CONFIG_REMOTECART_WIFI_PASSWORD,
        sizeof(wifi_config.ap.password)
    );

    wifi_config.ap.ssid_len =
        strlen(CONFIG_REMOTECART_WIFI_SSID);
    wifi_config.ap.channel =
        CONFIG_REMOTECART_WIFI_CHANNEL;
    wifi_config.ap.max_connection =
        CONFIG_REMOTECART_MAX_CONNECTIONS;
    wifi_config.ap.authmode =
        password_length == 0
            ? WIFI_AUTH_OPEN
            : WIFI_AUTH_WPA2_PSK;

    result = esp_wifi_set_mode(WIFI_MODE_AP);

    if (result != ESP_OK)
    {
        return result;
    }

    result = esp_wifi_set_config(
        WIFI_IF_AP,
        &wifi_config
    );

    if (result != ESP_OK)
    {
        return result;
    }

    result = esp_wifi_start();

    if (result != ESP_OK)
    {
        return result;
    }

    esp_netif_ip_info_t ip_info;
    result = esp_netif_get_ip_info(
        network_interface,
        &ip_info
    );

    if (result != ESP_OK)
    {
        return result;
    }

    ESP_LOGI(
        TAG,
        "Ponto de acesso iniciado: %s",
        CONFIG_REMOTECART_WIFI_SSID
    );

    ESP_LOGI(
        TAG,
        "Interface disponível em " IPSTR,
        IP2STR(&ip_info.ip)
    );

    return ESP_OK;
}
