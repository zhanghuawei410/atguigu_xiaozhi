#include "xiaozhi_opus_encodec.h"

#include <string.h>
#include "esp_log.h"
#include "esp_audio_enc.h"
#include "esp_audio_types.h"
#include "esp_opus_enc.h"
#include "freertos/ringbuf.h"
#include "./common/common_types.h"
#include "freertos/FreeRTOS.h"


#define TAG "XIAOZHI_OPUS"

static esp_audio_enc_handle_t s_encoder = NULL;
static int s_pcm_frame_size = 0;
static int s_opus_frame_size = 0;

uint8_t *pcm_data_buffer;
uint8_t *opus_data_buffer;

void xiaozhi_opus_encoder_task(void *arg);

static esp_err_t xiaozhi_audio_ret_to_esp_err(esp_audio_err_t ret)
{
    switch (ret)
    {
    case ESP_AUDIO_ERR_OK:
        return ESP_OK;
    case ESP_AUDIO_ERR_MEM_LACK:
        return ESP_ERR_NO_MEM;
    case ESP_AUDIO_ERR_INVALID_PARAMETER:
        return ESP_ERR_INVALID_ARG;
    case ESP_AUDIO_ERR_BUFF_NOT_ENOUGH:
        return ESP_ERR_INVALID_SIZE;
    default:
        return ESP_FAIL;
    }
}

esp_err_t xiaozhi_opus_encoder_init(void)
{
    if (s_encoder != NULL)
    {
        return ESP_OK;
    }

    esp_opus_enc_config_t opus_cfg = ESP_OPUS_ENC_CONFIG_DEFAULT();
    opus_cfg.sample_rate = ESP_AUDIO_SAMPLE_RATE_16K;
    opus_cfg.channel = ESP_AUDIO_MONO;
    opus_cfg.bits_per_sample = ESP_AUDIO_BIT16;
    opus_cfg.bitrate = 20000;
    opus_cfg.frame_duration = ESP_OPUS_ENC_FRAME_DURATION_60_MS;
    opus_cfg.application_mode = ESP_OPUS_ENC_APPLICATION_VOIP;

    esp_audio_err_t ret = esp_opus_enc_open(&opus_cfg, sizeof(opus_cfg), &s_encoder);
    if (ret != ESP_AUDIO_ERR_OK)
    {
        ESP_LOGE(TAG, "esp_opus_enc_open failed, ret=%d", ret);
        s_encoder = NULL;
        return xiaozhi_audio_ret_to_esp_err(ret);
    }

    ret = esp_opus_enc_get_frame_size(s_encoder, &s_pcm_frame_size, &s_opus_frame_size);
    if (ret != ESP_AUDIO_ERR_OK)
    {
        ESP_LOGE(TAG, "esp_opus_enc_get_frame_size failed, ret=%d", ret);
        xiaozhi_opus_encoder_deinit();
        return xiaozhi_audio_ret_to_esp_err(ret);
    }

    ESP_LOGI(TAG, "opus encoder init success, pcm_frame_size=%d opus_frame_size=%d",
             s_pcm_frame_size, s_opus_frame_size);

    // 创建encoder的两个小缓冲区
    pcm_data_buffer = heap_caps_malloc(s_pcm_frame_size, MALLOC_CAP_SPIRAM);
    opus_data_buffer = heap_caps_malloc(s_opus_frame_size, MALLOC_CAP_SPIRAM);
    if (pcm_data_buffer == NULL || opus_data_buffer == NULL)
    {
        ESP_LOGE(TAG, "failed to allocate encoder buffers");
        xiaozhi_opus_encoder_deinit();
        return ESP_ERR_NO_MEM;
    }

    // 创建task（Opus编码器内部调用栈较深，需要足够的栈空间）
    xTaskCreatePinnedToCoreWithCaps(xiaozhi_opus_encoder_task, "opus_encoder_task", 32 * 1024, NULL, 5, NULL, 1, MALLOC_CAP_SPIRAM);

    return ESP_OK;
}

void xiaozhi_opus_encoder_task(void *arg)
{
    // 不停的从环形缓冲区 拉数据 来编码
    esp_audio_enc_in_frame_t in_frame = {
        .buffer = pcm_data_buffer,
        .len = s_pcm_frame_size,
    };
    esp_audio_enc_out_frame_t out_frame = {
        .buffer = opus_data_buffer,
        .len = s_opus_frame_size,
    };
    size_t this_time_read_size = 0;
    while (1)
    {

        size_t remain_frame_size = s_pcm_frame_size;
        uint8_t *current_buffer_start_position = in_frame.buffer;
        while (remain_frame_size > 0)
        {
            // 从环形缓冲区 当中 读取数据
            void *data_pointer = xRingbufferReceiveUpTo(xiaozhi_handle.sr2encoder_ringbuffer_handle, &this_time_read_size, portMAX_DELAY, remain_frame_size);
            if (data_pointer == NULL)
            {
                ESP_LOGE(TAG, "failed to receive from ringbuffer");
                continue;
            }
            // 内存拷贝，将环形缓冲区的数据拷贝到编码器的输入缓冲区
            memcpy(current_buffer_start_position, data_pointer, this_time_read_size);
            current_buffer_start_position += this_time_read_size;
            remain_frame_size -= this_time_read_size;
            // 释放环形缓冲区的数据
            vRingbufferReturnItem(xiaozhi_handle.sr2encoder_ringbuffer_handle, data_pointer);
        }
        // 压缩后的数据 就在 out_frame.buffer 里， 大小是 out_frame.encoded_bytes
        esp_opus_enc_process(s_encoder, &in_frame, &out_frame);
        // 发送给下一个环形缓冲区
        xRingbufferSend(xiaozhi_handle.encoder2ws_ringbuffer_handle, out_frame.buffer, out_frame.encoded_bytes, portMAX_DELAY);
    }
}

esp_err_t xiaozhi_opus_encoder_get_frame_size(int *pcm_frame_size, int *opus_frame_size)
{
    if (s_encoder == NULL)
    {
        return ESP_ERR_INVALID_STATE;
    }
    if (pcm_frame_size == NULL || opus_frame_size == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }
    *pcm_frame_size = s_pcm_frame_size;
    *opus_frame_size = s_opus_frame_size;
    return ESP_OK;
}

esp_err_t xiaozhi_opus_encoder_process(uint8_t *pcm_data,
                                       size_t pcm_size,
                                       uint8_t *opus_data,
                                       size_t opus_buf_size,
                                       size_t *encoded_size,
                                       uint64_t *pts)
{
    if (s_encoder == NULL)
    {
        return ESP_ERR_INVALID_STATE;
    }
    if (pcm_data == NULL || opus_data == NULL || encoded_size == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }
    if (pcm_size != (size_t)s_pcm_frame_size)
    {
        ESP_LOGE(TAG, "pcm size invalid, need=%d actual=%u", s_pcm_frame_size, (unsigned int)pcm_size);
        return ESP_ERR_INVALID_SIZE;
    }
    if (opus_buf_size < (size_t)s_opus_frame_size || opus_buf_size > UINT32_MAX)
    {
        ESP_LOGE(TAG, "opus buffer too small, need>=%d actual=%u", s_opus_frame_size, (unsigned int)opus_buf_size);
        return ESP_ERR_INVALID_SIZE;
    }

    esp_audio_enc_in_frame_t in_frame = {
        .buffer = pcm_data,
        .len = (uint32_t)pcm_size,
    };
    esp_audio_enc_out_frame_t out_frame = {
        .buffer = opus_data,
        .len = (uint32_t)opus_buf_size,
    };

    esp_audio_err_t ret = esp_opus_enc_process(s_encoder, &in_frame, &out_frame);
    if (ret != ESP_AUDIO_ERR_OK)
    {
        ESP_LOGE(TAG, "esp_opus_enc_process failed, ret=%d", ret);
        *encoded_size = 0;
        if (pts)
        {
            *pts = 0;
        }
        return xiaozhi_audio_ret_to_esp_err(ret);
    }

    *encoded_size = out_frame.encoded_bytes;
    if (pts)
    {
        *pts = out_frame.pts;
    }
    return ESP_OK;
}

esp_err_t xiaozhi_opus_encoder_reset(void)
{
    if (s_encoder == NULL)
    {
        return ESP_ERR_INVALID_STATE;
    }
    return xiaozhi_audio_ret_to_esp_err(esp_opus_enc_reset(s_encoder));
}

void xiaozhi_opus_encoder_deinit(void)
{
    if (s_encoder != NULL)
    {
        esp_opus_enc_close(s_encoder);
        s_encoder = NULL;
    }
    s_pcm_frame_size = 0;
    s_opus_frame_size = 0;
}
