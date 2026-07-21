#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "wifi_ap.h"
#include "esp_http_server.h"
#include "driver/gpio.h"

static const char *TAG = "MAIN";

#define MOTOR_A_IN1 26
#define MOTOR_A_IN2 25
#define MOTOR_B_IN3 33
#define MOTOR_B_IN4 32

void setup_motores() {
    gpio_reset_pin(MOTOR_A_IN1);
    gpio_set_direction(MOTOR_A_IN1, GPIO_MODE_OUTPUT);
    gpio_reset_pin(MOTOR_A_IN2);
    gpio_set_direction(MOTOR_A_IN2, GPIO_MODE_OUTPUT);
    
    gpio_reset_pin(MOTOR_B_IN3);
    gpio_set_direction(MOTOR_B_IN3, GPIO_MODE_OUTPUT);
    gpio_reset_pin(MOTOR_B_IN4);
    gpio_set_direction(MOTOR_B_IN4, GPIO_MODE_OUTPUT);
}

void motor_parar() {
    gpio_set_level(MOTOR_A_IN1, 0);
    gpio_set_level(MOTOR_A_IN2, 0);
    gpio_set_level(MOTOR_B_IN3, 0);
    gpio_set_level(MOTOR_B_IN4, 0);
}

void motor_direita() {
    gpio_set_level(MOTOR_A_IN1, 1);
    gpio_set_level(MOTOR_A_IN2, 0);
    gpio_set_level(MOTOR_B_IN3, 1);
    gpio_set_level(MOTOR_B_IN4, 0);
    
}

void motor_esquerda() {
    gpio_set_level(MOTOR_A_IN1, 0);
    gpio_set_level(MOTOR_A_IN2, 1);
    gpio_set_level(MOTOR_B_IN3, 0);
    gpio_set_level(MOTOR_B_IN4, 1);
}

void motor_re() { 
    gpio_set_level(MOTOR_A_IN1, 0);
    gpio_set_level(MOTOR_A_IN2, 1);
    gpio_set_level(MOTOR_B_IN3, 1);
    gpio_set_level(MOTOR_B_IN4, 0);
}

void motor_frente() { 
    gpio_set_level(MOTOR_A_IN1, 1);
    gpio_set_level(MOTOR_A_IN2, 0);
    gpio_set_level(MOTOR_B_IN3, 0);
    gpio_set_level(MOTOR_B_IN4, 1);
}

static esp_err_t frente_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Comando: Mover para FRENTE");
    motor_frente();
    httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t parar_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Comando: PARAR");
    motor_parar();
    httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t re_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Comando: RE");
    motor_re();
    httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t esquerda_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Comando: ESQUERDA");
    motor_esquerda();
    httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t direita_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Comando: DIREITA");
    motor_direita();
    httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// No seu código C, adicione este handler
static esp_err_t root_handler(httpd_req_t *req)
{

  const char* html_resp = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<title>Controle Carrinho ESP32</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
    body { font-family: Arial, sans-serif; text-align: center; }
    .btn-grid { display: grid; grid-template-columns: 1fr 1fr 1fr; gap: 10px; max-width: 300px; margin: 20px auto; }
    button { padding: 20px; font-size: 1.2em; border: 1px solid #ccc; border-radius: 8px; }
    #frente { grid-column: 2; }
    #parar { grid-column: 2; background-color: #f44336; color: white;}
</style>
</head>
<body>
    <h1>Controle do Carrinho</h1>
    <div class="btn-grid">
        <div></div>
        <button id="frente" onclick="sendCommand('/frente')">Frente</button>
        <div></div>
        <button id="esquerda" onclick="sendCommand('/esquerda')">Esquerda</button>
        <button id="parar" onclick="sendCommand('/parar')">PARAR</button>
        <button id="direita" onclick="sendCommand('/direita')">Direita</button>
        <div></div>
        <button id="re" onclick="sendCommand('/re')">R&eacute;</button>
        <div></div>
    </div>

<script>
    // O script continua o mesmo
    function sendCommand(command) {
        fetch(command)
            .then(response => {
                if (!response.ok) {
                    console.error('Erro ao enviar comando!');
                } else {
                    console.log('Comando ' + command + ' enviado com sucesso.');
                }
            })
            .catch(error => console.error('Erro de conexao:', error));
    }
</script>
</body>
</html>
)rawliteral";

    httpd_resp_send(req, html_resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// E registre a nova URI no servidor:
static const httpd_uri_t uri_root = {
    .uri      = "/",
    .method   = HTTP_GET,
    .handler  = root_handler,
    .user_ctx = NULL
};

static const httpd_uri_t uri_frente = {
    .uri      = "/frente",
    .method   = HTTP_GET,
    .handler  = frente_handler,
    .user_ctx = NULL
};

static const httpd_uri_t uri_parar = {
    .uri      = "/parar",
    .method   = HTTP_GET,
    .handler  = parar_handler,
    .user_ctx = NULL
};

static const httpd_uri_t uri_re = {
    .uri      = "/re",
    .method   = HTTP_GET,
    .handler  = re_handler,
    .user_ctx = NULL
};

static const httpd_uri_t uri_esquerda = {
    .uri      = "/esquerda",
    .method   = HTTP_GET,
    .handler  = esquerda_handler,
    .user_ctx = NULL
};


static const httpd_uri_t uri_direita = {
    .uri      = "/direita",
    .method   = HTTP_GET,
    .handler  = direita_handler,
    .user_ctx = NULL
};

static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;

    ESP_LOGI(TAG, "Iniciando o servidor na porta: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        
        ESP_LOGI(TAG, "Registrando URIs");
        httpd_register_uri_handler(server, &uri_root);
        httpd_register_uri_handler(server, &uri_frente);
        httpd_register_uri_handler(server, &uri_parar);
        httpd_register_uri_handler(server, &uri_re);
        httpd_register_uri_handler(server, &uri_esquerda);
        httpd_register_uri_handler(server, &uri_direita);
        
        return server;
    }

    ESP_LOGI(TAG, "Erro ao iniciar o servidor!");
    return NULL;
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    setup_motores();

    ESP_LOGI(TAG, "Inicializando Ponto de Acesso (AP)...");
    
    wifi_init_softap();
    start_webserver();
    ESP_LOGI(TAG, "Servidor web iniciado.");
}