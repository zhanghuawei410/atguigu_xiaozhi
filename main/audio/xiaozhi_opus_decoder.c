#include "xiaozhi_opus_decoder.h"
#include "esp_opus_dec.h"
#include "common/common_types.h"
#include "freertos/FreeRTOS.h"
#include "xiaozhi_hard_codec.h"
#include "esp_log.h"

#define TAG "XIAOZHI_DEC"

esp_audio_dec_handle_t decoder_handle = NULL;

void xiaozhi_opus_decoder_task(void *arg);
void xiaozhi_opus_decoder_init(void)
{
        esp_opus_dec_cfg_t cfg = {                       \
        .sample_rate       = ESP_AUDIO_SAMPLE_RATE_16K,            \
        .channel           = ESP_AUDIO_MONO,                      \
        .frame_duration    = ESP_OPUS_DEC_FRAME_DURATION_60_MS, \
        .self_delimited    = false,                               \
    };

    esp_audio_err_t ret = esp_opus_dec_open(&cfg, sizeof(cfg), &decoder_handle);
    if (ret != ESP_AUDIO_ERR_OK)
    {
        ESP_LOGE(TAG, "esp_opus_dec_open failed, ret=%d", ret);
        decoder_handle = NULL;
        return;
    }
    // 创建解码任务（Opus解码器内部调用栈较深，需要足够的栈空间）
    xTaskCreatePinnedToCoreWithCaps(xiaozhi_opus_decoder_task, "opus_decoder_task", 32 * 1024, NULL, 5, NULL, 0, MALLOC_CAP_SPIRAM);

}

void xiaozhi_opus_decoder_task(void *arg)
{
    esp_audio_dec_out_frame_t output_pcm_frame = {0};
    esp_audio_dec_in_raw_t input_opus_frame = {0};
    output_pcm_frame.buffer = heap_caps_malloc(8 * 1024 , MALLOC_CAP_SPIRAM);
    output_pcm_frame.len = 8 * 1024;
    esp_audio_dec_info_t dec_info = {0};    

   while (1)
   {
        size_t to_decode_data_size = 0;
        // 冲环形缓冲区里拿到需要解码的数据
        void * to_decode_data_pointer = xRingbufferReceive(xiaozhi_handle.decoder_in_ring_h, &to_decode_data_size, portMAX_DELAY);
        input_opus_frame.buffer = to_decode_data_pointer;
        input_opus_frame.len = to_decode_data_size;

        while (input_opus_frame.len > 0)
        {
            esp_opus_dec_decode(decoder_handle, &input_opus_frame, &output_pcm_frame, &dec_info);
            // 播放解码后的数据
            xiaozhi_codec_play(output_pcm_frame.buffer, output_pcm_frame.decoded_size);
            // 更新解码器输入的状态，继续解码剩余的数据
            input_opus_frame.buffer += input_opus_frame.consumed;
            input_opus_frame.len -= input_opus_frame.consumed;
        }
        vRingbufferReturnItem(xiaozhi_handle.decoder_in_ring_h, to_decode_data_pointer);

   }


}