#pragma once

#include <stdint.h>

#include "esp_err.h"

esp_err_t encoder_init(void);
void encoder_get_counts(int32_t *left_count, int32_t *right_count);
