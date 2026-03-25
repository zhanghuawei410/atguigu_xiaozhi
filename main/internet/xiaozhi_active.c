#include "internet/xiaozhi_active.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_random.h"
#include "esp_log.h"
#include "cJSON.h"
#include "esp_app_desc.h"
#include <stdlib.h>
#include "esp_tls.h"
#include "common/common_types.h"

esp_http_client_handle_t client;

#define TAG "xiaozhi_active"

static char *output_buffer = NULL; // Buffer to store response of http request from event handler
static int output_len = 0;         // Stores number of bytes read
static uint32_t currnet_total_len = 0;

esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{

    switch (evt->event_id)
    {
    case HTTP_EVENT_ERROR:
        ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
        break;
    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
        break;
    case HTTP_EVENT_ON_HEADER:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
        if (strcmp(evt->header_key, "Content-Length") == 0)
        {
            // 获取数据的长度
            output_len = atoi(evt->header_value);
            output_buffer = malloc(output_len + 1);
            output_buffer[output_len] = '\0';
        }
        break;
    case HTTP_EVENT_ON_DATA:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
        memcpy(output_buffer + currnet_total_len, evt->data, evt->data_len);
        currnet_total_len += evt->data_len;

        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
        ESP_LOGI(TAG, "res: %s", output_buffer);
        xiaozhi_active_parse_response(output_buffer, output_len);
        if (output_buffer != NULL)
        {

            free(output_buffer);
            output_buffer = NULL;
        }
        output_len = 0;
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
        int mbedtls_err = 0;
        esp_err_t err = esp_tls_get_and_clear_last_error((esp_tls_error_handle_t)evt->data, &mbedtls_err, NULL);
        if (err != 0)
        {
            ESP_LOGI(TAG, "Last esp error code: 0x%x", err);
            ESP_LOGI(TAG, "Last mbedtls failure: 0x%x", mbedtls_err);
        }
        if (output_buffer != NULL)
        {
            free(output_buffer);
            output_buffer = NULL;
        }
        output_len = 0;
        break;
    case HTTP_EVENT_REDIRECT:
        ESP_LOGD(TAG, "HTTP_EVENT_REDIRECT");
        esp_http_client_set_header(evt->client, "From", "user@example.com");
        esp_http_client_set_header(evt->client, "Accept", "text/html");
        esp_http_client_set_redirection(evt->client);
        break;
    }
    return ESP_OK;
}

// 不写初始化了 因为http客户端是一次性的
void xiaozhi_active_request(void)
{
    // 0. 初始化
    esp_http_client_config_t config = {
        .host = "api.tenclass.net",
        .path = "/xiaozhi/ota/",
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .event_handler = _http_event_handler,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .method = HTTP_METHOD_POST,
    };
    client = esp_http_client_init(&config);

    // 设置请求头
    xiaozhi_active_set_request_header();
    // 设置请求体
    xiaozhi_active_set_request_body();
    // 发送请求
    esp_err_t err = esp_http_client_perform(client);
}

uint8_t mac[6];

char mac_str[18];
char uuid_str[37];
void xiaozhi_active_set_request_header(void)
{
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "Host", "api.tenclass.net");
    esp_http_client_set_header(client, "User-Agent", "bread-compact-wifi-128x64/1.0.1");

    esp_wifi_get_mac(WIFI_IF_STA, mac);
    sprintf(mac_str, "%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    ESP_LOGW("xiaozhi_active", "Device-Id: %s", mac_str);
    esp_http_client_set_header(client, "Device-Id", mac_str);
    xiaozhi_active_uuid_v4_gen(uuid_str);
    esp_http_client_set_header(client, "Client-Id", uuid_str);
}
// 7b94d69a - 9808 - 4c59 - 9c9b - 704333b38aff
void xiaozhi_active_uuid_v4_gen(char *uuid_str)
{
    // 0. 开启nvs的一个命名空间
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("xiaozhi_data", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(err));
        return;
    }
    // 1. 先去nvs中 看一下有没有之前就创建好的uuid
    size_t required_size;
    err = nvs_get_str(nvs_handle, "uuid", NULL, &required_size); // 试探性读 看看到底有没有一个叫uuid的数据

    if (err == ESP_OK)
    {
        // 2. 如果有 就从nvs中读取上一次生成的结果 继续使用
        ESP_LOGI(TAG, "当前用的是上一次的uuid");
        nvs_get_str(nvs_handle, "uuid", uuid_str, &required_size);
    }
    else
    {
        // 3. 如果没有 就生成一个新的 uuid
        ESP_LOGI(TAG, "当前用的是新生成的uuid");
        // 生成uuid v4 16个字节
        uint32_t rand1 = esp_random(); // 生成一个32bit的随机数(4个字节)
        uint32_t rand2 = esp_random();
        uint32_t rand3 = esp_random();
        uint32_t rand4 = esp_random();

        sprintf(uuid_str,
                "%08x-%04x-%04x-%04x-%04x%08x",
                (unsigned int)(rand1),
                (unsigned int)(rand2 >> 16),
                (unsigned int)(((rand2 >> 12) & 0x0fff) | 0x4000), // 设置版本号4
                (unsigned int)(((rand3 >> 16) & 0x3fff) | 0x8000), // 设置变体8/9/a/b
                (unsigned int)(rand3 & 0xffff),
                (unsigned int)(rand4));
        // 把这个uuid存起来
        nvs_set_str(nvs_handle, "uuid", uuid_str);
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
    }
}

void xiaozhi_active_set_request_body(void)
{
    // 最外层的json
    cJSON *root = cJSON_CreateObject();
    cJSON *application = cJSON_CreateObject();
    cJSON *board = cJSON_CreateObject();

    cJSON_AddItemToObject(root, "application", application);
    cJSON_AddItemToObject(root, "board", board);

    cJSON_AddStringToObject(application, "version", "1.0.1");
    cJSON_AddStringToObject(application, "elf_sha256", esp_app_get_elf_sha256_str());

    cJSON_AddStringToObject(board, "type", "bread-compact-wifi");
    cJSON_AddStringToObject(board, "name", "bread-compact-wifi-128x64");
    cJSON_AddStringToObject(board, "ssid", "万能wifi钥匙");
    cJSON_AddNumberToObject(board, "rssi", -55);
    cJSON_AddNumberToObject(board, "channel", 1);
    cJSON_AddStringToObject(board, "ip", "192.168.1.11");
    cJSON_AddStringToObject(board, "mac", mac_str);

    char *res = cJSON_PrintUnformatted(root);

    esp_http_client_set_post_field(client, res, strlen(res));

    cJSON_Delete(root);

    ESP_LOGI(TAG, "body :%s", res);
}

void xiaozhi_active_parse_response(char *res, int len)
{
    cJSON *root = cJSON_ParseWithLength(res, len);
    if (root == NULL)
    {
        ESP_LOGE(TAG, "woc 不是json");
        return;
    }

    cJSON *websocket = cJSON_GetObjectItem(root, "websocket");
    if (websocket == NULL)
    {
        ESP_LOGE(TAG, "websocket is null");
        return;
    }
    cJSON *ws_url = cJSON_GetObjectItem(websocket, "url");
    if (ws_url == NULL)
    {
        ESP_LOGE(TAG, "ws_url is null");
        return;
    }
    // ws_url_str这个东西是从cjson节点里直接返回的
    // 如果json将来被删除 会被跟着释放
    // 所以为了内存安全 必须拷贝。
    char *ws_url_str = cJSON_GetStringValue(ws_url);
    xiaozhi_handle.websocket_url = malloc(strlen(ws_url_str) + 1);
    strcpy(xiaozhi_handle.websocket_url, ws_url_str);

    cJSON *token = cJSON_GetObjectItem(websocket, "token");
    if (token == NULL)
    {
        ESP_LOGE(TAG, "token is null");
        return;
    }
    char *token_str = cJSON_GetStringValue(token);
    xiaozhi_handle.websocket_token = malloc(strlen(token_str) + 1);
    strcpy(xiaozhi_handle.websocket_token, token_str);

    // 解析是否激活

    cJSON *activation = cJSON_GetObjectItem(root, "activation");
    if (activation == NULL)
    {
        // 如果当前这个json已经又了websocket信息 但是没有激活信息
        //  说明已经激活了
        xiaozhi_handle.active_flag = true;
    }
    else
    {
        // 有ws信息，且有激活信息
        // 说明没有激活
        xiaozhi_handle.active_flag = false;
        // 解析出激活码
        xiaozhi_handle.active_code = malloc(7);
        xiaozhi_handle.active_code[6] = '\0';
        cJSON *active_code = cJSON_GetObjectItem(activation, "code");
        strcpy(xiaozhi_handle.active_code, active_code->valuestring);
        ESP_LOGI(TAG, "xiaozhi_handle.ws_url = %s\nxiaozhi_handle.ws_token = %s\nxiaozhi_handle.active_flag = %d\nxiaozhi_handle.active_code = %s\n",
                 xiaozhi_handle.websocket_url,
                 xiaozhi_handle.websocket_token,
                 xiaozhi_handle.active_flag,
                 xiaozhi_handle.active_code);
    }
}