#include <stdio.h>
#include "internet/xiaozhi_wifi.h"
#include "esp_log.h"
#include "button/xiaozhi_button.h"
#include "display/xiaozhi_display.h"
#include "internet/xiaozhi_active.h"
#include "common/common_types.h"
#include "esp_system.h"
#include "audio/xiaozhi_sr.h"
#include "audio/xiaozhi_opus_encodec.h"
#include "audio/xiaozhi_opus_decoder.h"
#include "freertos/ringbuf.h"

#define RING_FIFO_SIZE (8 * 1024)
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
void xiaozhi_create_all_ringbuffers(void)
{
    xiaozhi_handle.sr2encoder_ringbuffer_handle = xRingbufferCreateWithCaps(RING_FIFO_SIZE, RINGBUF_TYPE_BYTEBUF, MALLOC_CAP_SPIRAM);
    xiaozhi_handle.encoder2ws_ringbuffer_handle = xRingbufferCreateWithCaps(RING_FIFO_SIZE * 2, RINGBUF_TYPE_NOSPLIT, MALLOC_CAP_SPIRAM);
    xiaozhi_handle.decoder_in_ring_h = xRingbufferCreateWithCaps(RING_FIFO_SIZE * 2, RINGBUF_TYPE_NOSPLIT, MALLOC_CAP_SPIRAM);
}

void encoder_decoder_test_task(void *ar)
{
    // 从编码器缓冲区读取数据，发送到 解码缓冲区
    while (1)
    {
        size_t item_size = 0;
        uint8_t *item = (uint8_t *)xRingbufferReceive(xiaozhi_handle.encoder2ws_ringbuffer_handle, &item_size, portMAX_DELAY);
        if (item != NULL)
        {
            // 将数据发送到解码器的环形缓冲区
            xRingbufferSend(xiaozhi_handle.decoder_in_ring_h, item, item_size, portMAX_DELAY);
            // 释放编码器环形缓冲区的数据
            vRingbufferReturnItem(xiaozhi_handle.encoder2ws_ringbuffer_handle, item);
        }
    }
}

void app_main(void)
{
    // 1. 创建所有环形缓冲区
    xiaozhi_create_all_ringbuffers();

    // 2. 测试语音识别
    xiaozhi_sr_init();

    // 3. 初始化opus编码器
    esp_err_t ret = xiaozhi_opus_encoder_init();
    if (ret != ESP_OK)
    {
        ESP_LOGE("main", "opus encoder init failed: %s", esp_err_to_name(ret));
        return;
    }
    // 4.初始化opus解码器
    xiaozhi_opus_decoder_init();
    // 5. 创建一个测试用的编码器-解码器数据搬运任务，正式流程中这个功能会被websocket相关的任务替代
    xTaskCreate(encoder_decoder_test_task, "encoder_decoder_test_task", 32 * 1024, NULL, 5, NULL);

    // TODO: 恢复完整初始化流程后删除以下注释
    // xiaozhi_display_init();
    // xiaozhi_wifi_init();
    // xiaozhi_button_init();
    // xiaozhi_button_front_register_cb(BUTTON_SINGLE_CLICK, button_cb, (void *)1);
    // xiaozhi_button_back_register_cb(BUTTON_SINGLE_CLICK, button_cb, (void *)2);
    // xiaozhi_button_back_register_cb(BUTTON_LONG_PRESS_START, button_cb, (void *)3);
    // xiaozhi_display_delete_qrcode();
    // xiaozhi_active_request();
    // if (xiaozhi_handle.active_flag == false) {
    //     char *temp_buffer = (char *)calloc(60, 1);
    //     snprintf(temp_buffer, 60, "请到官网控制台激活设备\n激活码: %s", xiaozhi_handle.active_code);
    //     xiaozhi_display_text(temp_buffer);
    //     free(temp_buffer);
    //     xiaozhi_display_tip("激活后单击sw2按键");
    // }
}