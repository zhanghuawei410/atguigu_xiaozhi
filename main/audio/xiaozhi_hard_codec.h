#pragma once
#include <string.h>
#include "driver/i2s_std.h"
#include "driver/i2s_tdm.h"
#include "soc/soc_caps.h"
#include "esp_private/rtc_clk.h"
#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"
#include "unity.h"
#include "driver/i2c_master.h"
#include "esp_log.h"

#define TEST_BOARD_I2S_BCK_PIN (2)
#define TEST_BOARD_I2S_MCK_PIN (3)
#define TEST_BOARD_I2S_DATA_IN_PIN (4)
#define TEST_BOARD_I2S_DATA_OUT_PIN (6)
#define TEST_BOARD_I2S_DATA_WS_PIN (5)
#define CODEC_I2C_SCL_NUM 1
#define CODEC_I2C_SDA_NUM 0
#define ES8311_CODEC_DEFAULT_ADDR 0x30

void xiaozhi_codec_init(void);

void xiaozhi_codec_play(void *data, int len);

void xiaozhi_codec_record(uint8_t *data, size_t size);
