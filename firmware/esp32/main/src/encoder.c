#include "encoder.h"

#include <stdint.h>

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "robot_config.h"

static volatile int32_t s_left_count;
static volatile int32_t s_right_count;
static portMUX_TYPE s_encoder_mux = portMUX_INITIALIZER_UNLOCKED;

static void IRAM_ATTR encoder_isr(void *arg)
{
    gpio_num_t gpio = (gpio_num_t)(uintptr_t)arg;

    portENTER_CRITICAL_ISR(&s_encoder_mux);

    if (gpio == ROBOT_LEFT_ENCODER_GPIO)
    {
        s_left_count++;
    }
    else
    {
        s_right_count++;
    }

    portEXIT_CRITICAL_ISR(&s_encoder_mux);
}

esp_err_t encoder_init(void)
{
    gpio_config_t config = {
        .pin_bit_mask =
            (1ULL << ROBOT_LEFT_ENCODER_GPIO) |
            (1ULL << ROBOT_RIGHT_ENCODER_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_POSEDGE,
    };

    esp_err_t result = gpio_config(&config);

    if (result != ESP_OK)
    {
        return result;
    }

    result = gpio_install_isr_service(0);

    if (result != ESP_OK && result != ESP_ERR_INVALID_STATE)
    {
        return result;
    }

    result = gpio_isr_handler_add(
        ROBOT_LEFT_ENCODER_GPIO,
        encoder_isr,
        (void *)(uintptr_t)ROBOT_LEFT_ENCODER_GPIO
    );

    if (result != ESP_OK)
    {
        return result;
    }

    return gpio_isr_handler_add(
        ROBOT_RIGHT_ENCODER_GPIO,
        encoder_isr,
        (void *)(uintptr_t)ROBOT_RIGHT_ENCODER_GPIO
    );
}

void encoder_get_counts(int32_t *left_count, int32_t *right_count)
{
    if (left_count == NULL || right_count == NULL)
    {
        return;
    }

    portENTER_CRITICAL(&s_encoder_mux);
    *left_count = s_left_count;
    *right_count = s_right_count;
    portEXIT_CRITICAL(&s_encoder_mux);
}
