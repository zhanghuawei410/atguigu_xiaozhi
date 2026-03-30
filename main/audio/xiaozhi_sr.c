#include "xiaozhi_sr.h"
#include "xiaozhi_hard_codec.h"
#include "esp_log.h"

#define TAG "XIAOZHI_SR"

static esp_afe_sr_iface_t *afe_handle = NULL;
esp_afe_sr_data_t *afe_data = NULL;
void xiaozhi_sr_init(void)
{
    // inti 8311
    xiaozhi_codec_init();
    /* 初始化模型列表 */
    srmodel_list_t *models = esp_srmodel_init("model");
    /* 初始化AFE配置 */
    afe_config_t *afe_config = afe_config_init("M", models, AFE_TYPE_SR, AFE_MODE_HIGH_PERF);
    afe_config->vad_min_noise_ms = 1000; // The minimum duration of noise or silence in ms.
    afe_config->vad_min_speech_ms = 128; // The minimum duration of speech in ms.
    afe_config->vad_mode = VAD_MODE_1;   // The larger the mode, the higher the speech trigger probability.
    /* 初始化AFE句柄 */
    afe_handle = esp_afe_handle_from_config(afe_config);
    /* 创建AFE数据 */
    afe_data = afe_handle->create_from_config(afe_config);

    /* 创建喂数据任务，绑定到CPU0（音频采集对实时性要求高，独占核心） */
    xTaskCreatePinnedToCoreWithCaps(xiaozhi_sr_feed_tast, "sr_feed", 8 * 1024, NULL, 5, NULL, 0, MALLOC_CAP_SPIRAM);
    /* 创建取数据任务，绑定到CPU1（AFE处理与业务逻辑） */
    xTaskCreatePinnedToCoreWithCaps(xiaozhi_sr_get_tast, "sr_get", 4 * 1024, NULL, 5, NULL, 1, MALLOC_CAP_SPIRAM);
}

/* 喂数据线程 */
void xiaozhi_sr_feed_tast(void *arg)
{
    /* 喂数据 */
    /* 1.获取数据大小 */
    int audio_chunksize = afe_handle->get_feed_chunksize(afe_data);
    /* 2.获取数据通道数 */
    int nch = afe_handle->get_feed_channel_num(afe_data);
    /* 3.从对空间分配一个空间，当作为数据的缓冲区 */
    int16_t *audio_buffer = (int16_t *)malloc(audio_chunksize * nch * sizeof(int16_t));

    while (1)
    {
        /* 4.从麦克风或者I2S接口获取音频数据，填充到audio_buffer中，数据大小为audio_chunksize * nch * sizeof(int16_t) */
        xiaozhi_codec_record((uint8_t *)audio_buffer, audio_chunksize * nch * sizeof(int16_t));
        /* 5.调用AFE的feed函数喂数据 */
        afe_handle->feed(afe_data, audio_buffer);
    }
}
/* 取数据任务：持续从AFE获取处理结果，检测唤醒和语音活动 */
void xiaozhi_sr_get_tast(void *arg)
{
    while (1)
    {
        afe_fetch_result_t *res = afe_handle->fetch(afe_data);
        // 取出来数据
        int16_t *data = res->data;      // 处理后的音频数据
        int data_size = res->data_size; // 处理后的音频数据大小，
        if (res->wakeup_state == WAKENET_DETECTED)
        {
            ESP_LOGI(TAG, "唤醒词已触发");
            /* 唤醒后的处理逻辑，例如启动语音识别 */
        }

        vad_state_t vad_state = res->vad_state;
        if (vad_state == VAD_SILENCE)
        {
            /* 无人声：静音或噪声 */
            ESP_LOGI(TAG, "当前为静音或噪声");
        }
        else if (vad_state == VAD_SPEECH)
        {
            /* 有人声：res->data 中为处理后的音频数据，可送入语音识别 */
            ESP_LOGI(TAG, "检测到语音活动，数据大小：%d bytes", res->data_size);
            /* 处理语音数据，例如送入ASR引擎 */
        }
    }
}