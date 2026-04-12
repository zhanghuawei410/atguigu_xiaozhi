#pragma once
#include <stdbool.h>
#include "esp_afe_sr_models.h"
#include "esp_afe_sr_iface.h"
#include "freertos/ringbuf.h"
typedef struct
{
    char *websocket_url;
    char *websocket_token;
    bool active_flag;  // 激活标志
    char *active_code; // 用来保存激活码

    // 整个业务中所有的环形缓冲区句柄
    RingbufHandle_t sr2encoder_ringbuffer_handle;
    RingbufHandle_t encoder2ws_ringbuffer_handle;
    RingbufHandle_t decoder_in_ring_h; //解码器 输入的环形缓冲区

    void (*wake_callback)(void); // 唤醒回调函数
    vad_state_t last_vas_state;
    vad_state_t current_vad_state;
    bool wake_flag; // 唤醒标志
    void (*vad_change_callback)(void);

} xiaozhi_handle_t;

typedef struct
{
    char *name; // 显示的文本
    char *emoji;
} xiaozhi_emoji_t;

extern xiaozhi_handle_t xiaozhi_handle;