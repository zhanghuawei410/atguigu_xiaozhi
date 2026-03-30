#include <stdio.h>
#include "internet/xiaozhi_wifi.h"
#include "esp_log.h"
#include "button/xiaozhi_button.h"
#include "display/xiaozhi_display.h"
#include "internet/xiaozhi_active.h"
#include "common/common_types.h"
#include "esp_system.h"
#include "audio/xiaozhi_sr.h"

xiaozhi_handle_t xiaozhi_handle;

void button_cb(void *button_handle, void *user_data)
{
    uint8_t event = (uint8_t)user_data;
    if (event == 1)
    {
        // 前面的按键被单击了
        ESP_LOGI("button", "front button single click");
        if (xiaozhi_handle.active_flag == false)
        {
            // 在没被激活的状态下 这个按键的功能是重启整个设备
            esp_restart();
        }
    }

    if (event == 2)
    {
        // 后面的按键被单击了
        ESP_LOGI("button", "back button single click");
    }

    if (event == 3)
    {
        // 后面的按键被长按了
        ESP_LOGI("button", "back button long press start");
        xiaozhi_wifi_reset();
    }
}

void app_main(void)
{
    // 1. 各种初始化
    // 1.1 先初始化lvgl
    // xiaozhi_display_init();
    // // 1.2 再初始化wifi
    // xiaozhi_wifi_init();
    // // 1.3 初始化按键
    // xiaozhi_button_init();
    // xiaozhi_button_front_register_cb(BUTTON_SINGLE_CLICK, button_cb, (void *)1);
    // xiaozhi_button_back_register_cb(BUTTON_SINGLE_CLICK, button_cb, (void *)2);
    // xiaozhi_button_back_register_cb(BUTTON_LONG_PRESS_START, button_cb, (void *)3);
    // // 1.4 配网成功就删除二维码
    // xiaozhi_display_delete_qrcode();

    // // xiaozhi_display_test();
    // // 2. 去拿了激活信息
    // xiaozhi_active_request();
    // // 2.1 xiaozhi的句柄里现在有激活信息
    // if (xiaozhi_handle.active_flag == false)
    // {
    //     char *temp_buffer = (char *)calloc(60, 1);
    //     sprintf(temp_buffer, "请到官网控制台激活设备\n 激活码: %s", xiaozhi_handle.active_code);
    //     xiaozhi_display_text(temp_buffer);
    //     free(temp_buffer);
    //     xiaozhi_display_tip("激活后单击sw2按键");
    // }
    // 测试语音识别
    xiaozhi_sr_init();
}