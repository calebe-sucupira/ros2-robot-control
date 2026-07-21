#include "web_server.h"

#include <stddef.h>

#include "esp_http_server.h"
#include "esp_log.h"
#include "motor_control.h"

typedef struct
{
    motor_command_t command;
    const char *name;
} command_context_t;

static const char *TAG = "WEB_SERVER";
static httpd_handle_t s_server;

extern const unsigned char index_html_start[]
    asm("_binary_index_html_start");

extern const unsigned char index_html_end[]
    asm("_binary_index_html_end");

static const command_context_t COMMAND_FORWARD = {
    .command = MOTOR_COMMAND_FORWARD,
    .name = "forward",
};

static const command_context_t COMMAND_REVERSE = {
    .command = MOTOR_COMMAND_REVERSE,
    .name = "reverse",
};

static const command_context_t COMMAND_LEFT = {
    .command = MOTOR_COMMAND_LEFT,
    .name = "left",
};

static const command_context_t COMMAND_RIGHT = {
    .command = MOTOR_COMMAND_RIGHT,
    .name = "right",
};

static const command_context_t COMMAND_STOP = {
    .command = MOTOR_COMMAND_STOP,
    .name = "stop",
};

static esp_err_t root_handler(httpd_req_t *request)
{
    size_t html_length =
        index_html_end - index_html_start;

    httpd_resp_set_type(
        request,
        "text/html; charset=utf-8"
    );

    httpd_resp_set_hdr(
        request,
        "Cache-Control",
        "no-store"
    );

    return httpd_resp_send(
        request,
        (const char *)index_html_start,
        html_length
    );
}

static esp_err_t command_handler(httpd_req_t *request)
{
    const command_context_t *context =
        request->user_ctx;

    esp_err_t result =
        motor_control_apply(context->command);

    if (result != ESP_OK)
    {
        httpd_resp_send_err(
            request,
            HTTPD_500_INTERNAL_SERVER_ERROR,
            "Falha ao aplicar comando"
        );
        return result;
    }

    ESP_LOGD(TAG, "Comando recebido: %s", context->name);

    httpd_resp_set_type(
        request,
        "text/plain; charset=utf-8"
    );

    return httpd_resp_sendstr(request, "OK");
}

static esp_err_t register_uri(
    httpd_handle_t server,
    const char *path,
    httpd_method_t method,
    esp_err_t (*handler)(httpd_req_t *),
    void *context
)
{
    httpd_uri_t uri = {
        .uri = path,
        .method = method,
        .handler = handler,
        .user_ctx = context,
    };

    return httpd_register_uri_handler(
        server,
        &uri
    );
}

esp_err_t web_server_start(void)
{
    if (s_server != NULL)
    {
        return ESP_ERR_INVALID_STATE;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;

    esp_err_t result = httpd_start(
        &s_server,
        &config
    );

    if (result != ESP_OK)
    {
        return result;
    }

    result = register_uri(
        s_server,
        "/",
        HTTP_GET,
        root_handler,
        NULL
    );

    if (result == ESP_OK)
    {
        result = register_uri(
            s_server,
            "/api/forward",
            HTTP_POST,
            command_handler,
            (void *)&COMMAND_FORWARD
        );
    }

    if (result == ESP_OK)
    {
        result = register_uri(
            s_server,
            "/api/reverse",
            HTTP_POST,
            command_handler,
            (void *)&COMMAND_REVERSE
        );
    }

    if (result == ESP_OK)
    {
        result = register_uri(
            s_server,
            "/api/left",
            HTTP_POST,
            command_handler,
            (void *)&COMMAND_LEFT
        );
    }

    if (result == ESP_OK)
    {
        result = register_uri(
            s_server,
            "/api/right",
            HTTP_POST,
            command_handler,
            (void *)&COMMAND_RIGHT
        );
    }

    if (result == ESP_OK)
    {
        result = register_uri(
            s_server,
            "/api/stop",
            HTTP_POST,
            command_handler,
            (void *)&COMMAND_STOP
        );
    }

    if (result != ESP_OK)
    {
        httpd_stop(s_server);
        s_server = NULL;
        return result;
    }

    ESP_LOGI(
        TAG,
        "Servidor HTTP iniciado na porta %u",
        config.server_port
    );

    return ESP_OK;
}
