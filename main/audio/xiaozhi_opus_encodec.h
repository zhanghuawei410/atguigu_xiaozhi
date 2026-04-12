#pragma once

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t xiaozhi_opus_encoder_init(void);

esp_err_t xiaozhi_opus_encoder_get_frame_size(int *pcm_frame_size, int *opus_frame_size);

esp_err_t xiaozhi_opus_encoder_process(uint8_t *pcm_data,
                                       size_t pcm_size,
                                       uint8_t *opus_data,
                                       size_t opus_buf_size,
                                       size_t *encoded_size,
                                       uint64_t *pts);

esp_err_t xiaozhi_opus_encoder_reset(void);

void xiaozhi_opus_encoder_deinit(void);

#ifdef __cplusplus
}
#endif
