#include "xiaozhi_hard_codec.h"
#define TAG "CODEC_DEV_UT"

static i2c_master_bus_handle_t i2c_bus_handle;
static i2s_comm_mode_t i2s_in_mode = I2S_COMM_MODE_STD;
static i2s_comm_mode_t i2s_out_mode = I2S_COMM_MODE_STD;
i2s_chan_handle_t tx_handle = NULL;
i2s_chan_handle_t rx_handle = NULL;
const audio_codec_data_if_t *data_if = NULL;
const audio_codec_if_t *out_codec_if = NULL;
esp_codec_dev_handle_t codec_dev;

void xiaozhi_codec_isc_init(void)
{
    ESP_LOGI(TAG, "codec_isc_init");
    i2c_master_bus_config_t i2c_bus_config = {0};
    i2c_bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
    i2c_bus_config.i2c_port = I2C_NUM_0;
    i2c_bus_config.scl_io_num = CODEC_I2C_SCL_NUM;
    i2c_bus_config.sda_io_num = CODEC_I2C_SDA_NUM;
    i2c_bus_config.glitch_ignore_cnt = 7;
    i2c_bus_config.flags.enable_internal_pullup = true;
    if (i2c_new_master_bus(&i2c_bus_config, &i2c_bus_handle) != ESP_OK)
    {
        ESP_LOGI(TAG, "codec_isc_init failed");
    }
    else
    {
        ESP_LOGI(TAG, "codec_isc_init success");
    }
}
void xiaozhi_codec_i2s_init(void)
{
    ESP_LOGI(TAG, "codec_i2s_init");
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.auto_clear_after_cb = true;
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(16000),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(16, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = TEST_BOARD_I2S_MCK_PIN,
            .bclk = TEST_BOARD_I2S_BCK_PIN,
            .ws = TEST_BOARD_I2S_DATA_WS_PIN,
            .dout = TEST_BOARD_I2S_DATA_OUT_PIN,
            .din = TEST_BOARD_I2S_DATA_IN_PIN,
        },
    };
    chan_cfg.auto_clear_after_cb = true;

    // 初始化i2s 通道
    int ret = i2s_new_channel(&chan_cfg, &tx_handle, &rx_handle);
    TEST_ESP_OK(ret);
    // 设置标准通道的配置
    if (i2s_out_mode == I2S_COMM_MODE_STD)
    {
        ret = i2s_channel_init_std_mode(tx_handle, &std_cfg);
    }
    TEST_ESP_OK(ret);
    if (i2s_in_mode == I2S_COMM_MODE_STD)
    {
        ret = i2s_channel_init_std_mode(rx_handle, &std_cfg);
    }
    TEST_ESP_OK(ret);
    // For tx master using duplex mode
    i2s_channel_enable(tx_handle);
    i2s_channel_enable(rx_handle);
}
void xiaozhi_codec_init(void)
{
    // 安装 isc 与 i2c驱动
    xiaozhi_codec_isc_init();
    xiaozhi_codec_i2s_init();
    // 为编解码器设备实现控制接口，数据接口和 GPIO 接口 (使用默认提供的接口实现)
    audio_codec_i2s_cfg_t i2s_cfg = {
        .rx_handle = rx_handle,
        .tx_handle = tx_handle,
    };
    data_if = audio_codec_new_i2s_data(&i2s_cfg);

    audio_codec_i2c_cfg_t i2c_cfg = {
        .addr = ES8311_CODEC_DEFAULT_ADDR,
        .bus_handle = i2c_bus_handle,
        .port = I2S_NUM_0, // 写不写都可以这个，新版本已经不使用这个port赋值
    };
    const audio_codec_ctrl_if_t *out_ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);

    const audio_codec_gpio_if_t *gpio_if = audio_codec_new_gpio();

    // 基于控制接口和 ES8311 特有的配置实现 audio_codec_if_t 接口
    es8311_codec_cfg_t es8311_cfg = {
        .codec_mode = ESP_CODEC_DEV_WORK_MODE_BOTH,
        .ctrl_if = out_ctrl_if,
        .gpio_if = gpio_if,
        .pa_pin = GPIO_NUM_7,
        .use_mclk = true,
    };
    out_codec_if = es8311_codec_new(&es8311_cfg);

    // 通过 API esp_codec_dev_new 获取 esp_codec_dev_handle_t 句柄 参考下面代码用获取到的句柄来进行播放和录制操作:
    esp_codec_dev_cfg_t dev_cfg;
    dev_cfg.codec_if = out_codec_if;              // es8311_codec_new 获取到的接口实现
    dev_cfg.data_if = data_if;                    // audio_codec_new_i2s_data 获取到的数据接口实现
    dev_cfg.dev_type = ESP_CODEC_DEV_TYPE_IN_OUT; // 设备同时支持录制和播放
    codec_dev = esp_codec_dev_new(&dev_cfg);
    // 以下代码展示如何播放音频
    esp_codec_dev_set_out_vol(codec_dev, 50.0);
    esp_codec_dev_sample_info_t fs = {
        .sample_rate = 16000,
        .channel = 1,
        .bits_per_sample = 16,
    };
    esp_codec_dev_open(codec_dev, &fs);

    // 以下代码展示如何录制音频
    esp_codec_dev_set_in_gain(codec_dev, 20);
}
void xiaozhi_codec_play(void *data, int len)
{
    if (esp_codec_dev_write(codec_dev, data, len) != ESP_CODEC_DEV_OK)
    {
        ESP_LOGI(TAG, "esp_codec_dev_write ok");
        ESP_LOGI(TAG, "esp_codec_dev_write failed");
    }
    else
    {
        // ESP_LOGI(TAG, "esp_codec_dev_write success");
    }
}

void xiaozhi_codec_record(uint8_t *data, size_t size)
{
    if (esp_codec_dev_read(codec_dev, data, size) != ESP_CODEC_DEV_OK)
    {
        ESP_LOGI(TAG, "esp_codec_dev_read failed");
    };
}