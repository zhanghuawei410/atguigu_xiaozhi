#pragma once
#include <stdbool.h>

typedef struct
{
    char *websocket_url;
    char *websocket_token;
    bool active_flag;  // 激活标志
    char *active_code; // 用来保存激活码
} xiaozhi_handle_t;

typedef struct
{
    char *name; // 显示的文本
    char *emoji;
} xiaozhi_emoji_t;

extern xiaozhi_handle_t xiaozhi_handle;