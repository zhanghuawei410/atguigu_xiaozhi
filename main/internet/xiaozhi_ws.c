#include "xiaozhi_ws.h"
#include "esp_log.h"
#include "esp_websocket_client.h"
#include "esp_event.h"
#include <cJSON.h>
#include "common/common_types.h"
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#define TAG "xiaozhi_ws"

char *hello_json_str =
#include "pre_data/xiaozhi_hello.txt"
    ;
esp_websocket_client_handle_t ws_client;

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0)
    {
        ESP_LOGE(TAG, "%s: 0x%x", message, error_code);
    }
}

static void websocket_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;
    switch (event_id)
    {
    case WEBSOCKET_EVENT_BEGIN:
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_BEGIN");
        break;
#if WS_TRANSPORT_HEADER_CALLBACK_SUPPORT
    case WEBSOCKET_EVENT_HEADER_RECEIVED:
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_HEADER_RECEIVED: %.*s", data->data_len, data->data_ptr);
        break;
#endif
    case WEBSOCKET_EVENT_CONNECTED:
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_CONNECTED");
        break;
    case WEBSOCKET_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_DISCONNECTED");
        log_error_if_nonzero("HTTP status code", data->error_handle.esp_ws_handshake_status_code);
        if (data->error_handle.error_type == WEBSOCKET_ERROR_TYPE_TCP_TRANSPORT)
        {
            log_error_if_nonzero("reported from esp-tls", data->error_handle.esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", data->error_handle.esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno", data->error_handle.esp_transport_sock_errno);
        }
        break;
    case WEBSOCKET_EVENT_DATA:
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_DATA");
        ESP_LOGI(TAG, "Received opcode=%d", data->op_code);
        // 文本帧 服务器返回的 字幕表情等信息
        if (data->op_code == 0x1)
        {
            ESP_LOGI(TAG, "Received text message: %.*s", data->data_len, data->data_ptr);
        }
        // 二进制帧 音频数据
        else if (data->op_code == 0x2)
        {
            ESP_LOGI(TAG, "Received binary message of length: %d", data->data_len);
        }

        break;
    case WEBSOCKET_EVENT_ERROR:
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_ERROR");
        log_error_if_nonzero("HTTP status code", data->error_handle.esp_ws_handshake_status_code);
        if (data->error_handle.error_type == WEBSOCKET_ERROR_TYPE_TCP_TRANSPORT)
        {
            log_error_if_nonzero("reported from esp-tls", data->error_handle.esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", data->error_handle.esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno", data->error_handle.esp_transport_sock_errno);
        }
        break;
    case WEBSOCKET_EVENT_FINISH:
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_FINISH");
        break;
    }
}
void xiaozhi_ws_init(void)
{
    esp_websocket_client_config_t websocket_cfg = {};
    websocket_cfg.uri = xiaozhi_handle.websocket_url;

    // 和wss相关的
    websocket_cfg.transport = WEBSOCKET_TRANSPORT_OVER_SSL;
    websocket_cfg.crt_bundle_attach = esp_crt_bundle_attach; // 使用 esp_crt_bundle_attach 来验证服务器证书
    websocket_cfg.reconnect_timeout_ms = 5000;               // 断线重连间隔时间 5s
    websocket_cfg.network_timeout_ms = 5000;                 // 网络操作超时时间 5s

    // 初始化 WebSocket 客户端 并获取客户端句柄
    ws_client = esp_websocket_client_init(&websocket_cfg);

    // 根据websocket_token的实际长度分配内存，并构造Authorization头的内容
    uint8_t auth_header_str_len = strlen("Bearer ") + strlen(xiaozhi_handle.websocket_token);
    char *auth_header_str = heap_caps_malloc(auth_header_str_len + 1, MALLOC_CAP_SPIRAM);
    auth_header_str[0] = '\0'; // 确保字符串以 null 结尾
    sprintf(auth_header_str, "Bearer %s", xiaozhi_handle.websocket_token);

    esp_websocket_client_append_header(ws_client, "Authorization", auth_header_str);
    esp_websocket_client_append_header(ws_client, "Protocol-Version", "1");
    esp_websocket_client_append_header(ws_client, "Device-Id", xiaozhi_handle.mac_str);
    esp_websocket_client_append_header(ws_client, "Client-Id", xiaozhi_handle.uuid);

    // 注册 WebSocket 事件处理函数
    esp_websocket_register_events(ws_client, WEBSOCKET_EVENT_ANY, websocket_event_handler, (void *)ws_client);

    // start
    esp_websocket_client_start(ws_client);
    // 测试start 5s后，发送hello
    vTaskDelay(5000 / portTICK_PERIOD_MS);
    xiaozhi_ws_send_hello();
}
void xiaozhi_ws_send_text(char *data)
{
    esp_websocket_client_send_text(ws_client, data, strlen(data), 2000);  
}

void xiaozhi_ws_send_hello(void)
{

    xiaozhi_ws_send_text(hello_json_str);
}