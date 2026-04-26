# 尚硅谷AI小智项目 — 调研发现

## 一、WebSocket 实现参考

### 官方示例
- ESP-IDF 提供了 `esp_websocket_client` 示例
- 路径: `$IDF_PATH/examples/protocols/websocket`

### 关键 API
```c
// 初始化
esp_websocket_client_config_t config = {
    .uri = "wss://api.xiaozhi.com/v1/ws/",
    .event_handle = ws_event_handler,
    .transport_type = WEBSOCKET_TRANSPORT_SSL,
};
client = esp_websocket_client_init(&config);

// 连接
esp_websocket_client_start(client);

// 发送数据
esp_websocket_client_send(client, data, len, portMAX_DELAY);

// 接收 (通过事件回调)
```

### 事件类型
- `WEBSOCKET_EVENT_CONNECTED`
- `WEBSOCKET_EVENT_DISCONNECTED`
- `WEBSOCKET_EVENT_DATA` - 收到消息
- `WEBSOCKET_EVENT_ERROR`
- `WEBSOCKET_EVENT_CLOSED`

---

## 二、现有代码分析

### xiaozhi_ws.c 现状
```c
// 已完成: 基础框架
void xiaozhi_ws_init(void)
{
    esp_websocket_client_config_t websocket_cfg = {};
    websocket_cfg.url = "wss://api.xiaozhi.com/v1/ws/";
    websocket_cfg.event_handle = xiaozhi_ws_event_handler;
    // ... 其他配置
    esp_websocket_client_handle_t client = esp_websocket_client_init(&websocket_cfg);
    // TODO: 需要完成事件处理、连接启动、发送/接收逻辑
}
```

### 缺失部分
1. `xiaozhi_ws_event_handler` 函数实现
2. `esp_websocket_client_start()` 调用
3. 音频数据发送循环
4. 接收数据处理逻辑
5. 与 RingBuffer 集成

---

## 三、集成方案

### 发送路径
```
sr_get (CPU1) → sr2encoder_ringbuffer → opus_encoder → encoder2ws_ringbuffer
                                                                        ↓
                                                             WebSocket 发送任务
                                                                        ↓
                                                               网络 (AI服务器)
```

### 接收路径
```
网络 (AI服务器)
    ↓
WebSocket 接收任务
    ↓
decoder_in_ringbuffer → opus_decoder → 播放
```

### 消息格式 (推测)
```json
// 发送
{
  "type": "audio",
  "data": "<base64编码的Opus数据>"
}

// 接收
{
  "type": "response",
  "audio": "<Opus数据>",
  "text": "AI回复文字"
}
```

---

## 四、实现步骤详解

### Step 1: 完善事件处理
```c
static void ws_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;
    
    switch(event_id) {
        case WEBSOCKET_EVENT_CONNECTED:
            ESP_LOGI(TAG, "WebSocket connected");
            break;
        case WEBSOCKET_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "WebSocket disconnected");
            break;
        case WEBSOCKET_EVENT_DATA:
            // 处理接收到的数据
            break;
    }
}
```

### Step 2: 音频发送任务
```c
void ws_send_task(void *arg)
{
    while (1) {
        // 从 encoder2ws_ringbuffer 读取 Opus 数据
        size_t len;
        uint8_t *data = xRingbufferReceive(encoder2ws_ringbuffer, &len, portMAX_DELAY);
        
        if (data && ws_client) {
            // 发送到服务器
            esp_websocket_client_send_bin(ws_client, (char*)data, len, portMAX_DELAY);
            vRingbufferReturnItem(encoder2ws_ringbuffer, data);
        }
    }
}
```

---

## 五、参考资源

1. [ESP-IDF WebSocket 文档](https://docs.espressif.com/projects/esp-idf/en/latest/api-reference/protocols/esp_websocket_client.html)
2. [小智官方文档](https://github.com/FengQuanLi/WifiBot/blob/main/README.md)
3. 现有组件: `managed_components/espressif__esp_websocket_client/`
