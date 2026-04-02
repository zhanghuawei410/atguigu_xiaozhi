#pragma once
#include <stdbool.h>
#include "esp_afe_sr_models.h"
#include "esp_afe_sr_iface.h"
#include <stdbool.h>
typedef struct
{
    char *websocket_url;
    char *websocket_token;
    bool active_flag;  // 激活标志
    char *active_code; // 用来保存激活码

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