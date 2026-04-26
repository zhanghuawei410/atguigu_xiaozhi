#include "xiaozhi_sr.h"
#include "xiaozhi_hard_codec.h"
#include "esp_log.h"
#include "common_types.h"
#include "freertos/ringbuf.h"

extern xiaozhi_handle_t xiaozhi_handle;

#define TAG "XIAOZHI_SR"

static esp_afe_sr_iface_t *afe_handle = NULL;
esp_afe_sr_data_t *afe_data = NULL;

void xiaozhi_vad_cb_test(void)
{
    ESP_LOGI(TAG, "VAD状态改变回调函数被调用,当前状态 %s \r\n", xiaozhi_handle.current_vad_state == VAD_SPEECH ? "语音" : "静音");
}

void xiaozhi_sr_init(void)
{

    xiaozhi_handle.vad_change_callback = xiaozhi_vad_cb_test;
    // inti 8311
    xiaozhi_codec_init();
    /* 初始化模型列表 */
    srmodel_list_t *models = esp_srmodel_init("model");
    /* 初始化AFE配置 */
    afe_config_t *afe_config = afe_config_init("M", models, AFE_TYPE_SR, AFE_MODE_HIGH_PERF);

    afe_config->se_init = false;
    afe_config->aec_init = false;
    afe_config->vad_mode = false;

    afe_config->vad_min_noise_ms = 1500; // 静音持续1500ms才判定语音结束，允许说话中的自然停顿
    afe_config->vad_min_speech_ms = 300; // 最短语音300ms，避免短促噪声误触
    afe_config->vad_mode = VAD_MODE_1;   // The larger the mode, the higher the speech trigger probability.
    /* 初始化AFE句柄 */
    afe_handle = esp_afe_handle_from_config(afe_config);
    /* 创建AFE数据 */
    afe_data = afe_handle->create_from_config(afe_config);

    /* 创建喂数据任务，绑定到CPU0（音频采集对实时性要求高，独占核心） */
    xTaskCreatePinnedToCoreWithCaps(xiaozhi_sr_feed_tast, "sr_feed", 32 * 1024, NULL, 5, NULL, 0, MALLOC_CAP_SPIRAM);
    /* 创建取数据任务，绑定到CPU1（AFE处理与业务逻辑，优先级高于feed避免ringbuffer溢出） */
    xTaskCreatePinnedToCoreWithCaps(xiaozhi_sr_get_tast, "sr_get", 32 * 1024, NULL, 7, NULL, 1, MALLOC_CAP_SPIRAM);
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
            // 为了安全 上次 状态为静默状态
            xiaozhi_handle.last_vas_state = VAD_SILENCE;
            xiaozhi_handle.current_vad_state = VAD_SILENCE;
            xiaozhi_handle.wake_flag = true; // 设置唤醒标志
            /* 唤醒后的处理逻辑，例如启动语音识别 */
            /* 执行回调函数 */
            if (xiaozhi_handle.wake_callback)
            {
                xiaozhi_handle.wake_callback();
            }
        }
        /* 我们关注的是关键词唤醒后 人声状态的改变 */
        if (xiaozhi_handle.wake_flag)
        {
            // 更新当前的状态 而且这个地方 是 唤醒后 才会更新
            xiaozhi_handle.current_vad_state = res->vad_state;
            if (xiaozhi_handle.current_vad_state != xiaozhi_handle.last_vas_state)
            {
                if (xiaozhi_handle.vad_change_callback)
                {
                    xiaozhi_handle.vad_change_callback();
                }

                // 语音结束（SPEECH → SILENCE），本轮交互结束，清除唤醒标志
                if (xiaozhi_handle.last_vas_state == VAD_SPEECH && xiaozhi_handle.current_vad_state == VAD_SILENCE)
                {
                    ESP_LOGI(TAG, "语音结束，清除唤醒标志");
                    xiaozhi_handle.wake_flag = false;
                }

                // 更新 上次 状态 将当前状态 存 为 上次状态
                xiaozhi_handle.last_vas_state = xiaozhi_handle.current_vad_state;
            }
        }

        // 既要唤醒过，也要有人声，满足这两个条件的音频才会被送到后续的语音识别模块
        if (xiaozhi_handle.wake_flag && xiaozhi_handle.current_vad_state == VAD_SPEECH)
        {
            //
            if (res->vad_cache_size > 0 && res->vad_cache != NULL)
            {
                // 如果vad_cache_size大于0，说明有被截断的音频数据需要补齐
                // 这里我们简单地将vad_cache的数据追加到data前面，形成完整的音频数据
                int16_t *full_data = (int16_t *)malloc(res->vad_cache_size + res->data_size);
                memcpy(full_data, res->vad_cache, res->vad_cache_size);
                memcpy(full_data + res->vad_cache_size / sizeof(int16_t), res->data, res->data_size);
                data = full_data;
                data_size += res->vad_cache_size;
            }
            // 将处理后的音频数据送入语音识别模块
            xRingbufferSend(xiaozhi_handle.sr2encoder_ringbuffer_handle, data, data_size, portMAX_DELAY);
            // TODO: 实现语音识别处理逻辑
        }
    }
}