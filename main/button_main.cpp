/* This example code is in the Public Domain (or CC0 licensed, at your option.)
   Unless required by applicable law or agreed to in writing, this software is
   distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_bt.h"
#include "esp_bt_defs.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_gatt_defs.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"

#include "esp_hidh.h"
#include "esp_hid_gap.h"

#include "driver/gpio.h"

#include "index.h"
#include "mqtt.h"
#include "ota.h"
#include "wifi.h"

static const char *TAG = "ESP_HIDH_DEMO";

static esp_hidh_dev_t *dev = nullptr;

const gpio_num_t LED_PIN = GPIO_NUM_2;
static bool current_state;

static esp_err_t index_handler(httpd_req_t *req)
{
    esp_err_t res = httpd_resp_set_type(req, "text/html");
    if(res != ESP_OK){
        return res;
    }
    res = httpd_resp_set_hdr(req, "Connection", "close");
	return httpd_resp_send(req, index_page, strlen(index_page));
}

static esp_err_t toggle_handler(httpd_req_t *req)
{
    esp_err_t res = httpd_resp_set_type(req, "text/plain");
    if(res != ESP_OK){
        return res;
    }
    res = httpd_resp_set_hdr(req, "Connection", "close");
    mqtt_publish();
	return httpd_resp_send(req, "OK", 2);
}

static esp_err_t state_handler(httpd_req_t *req)
{
    esp_err_t res = httpd_resp_set_type(req, "text/plain");
    if(res != ESP_OK){
        return res;
    }
    res = httpd_resp_set_hdr(req, "Connection", "close");
    if (current_state)
    {
	    return httpd_resp_send(req, "ON", 2);
    }
    else
    {
	    return httpd_resp_send(req, "OFF", 3);
    }
}

static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;
    config.max_uri_handlers = 32;

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) 
    {
        ota_add_endpoints(server);
        httpd_uri_t handler{};
        handler.uri       = "/";
        handler.method    = HTTP_GET;
        handler.handler   = index_handler;
        httpd_register_uri_handler(server, &handler);
        handler.uri       = "/toggle";
        handler.method    = HTTP_GET;
        handler.handler   = toggle_handler;
        httpd_register_uri_handler(server, &handler);
        handler.uri       = "/state";
        handler.method    = HTTP_GET;
        handler.handler   = state_handler;
        httpd_register_uri_handler(server, &handler);
    }

    return server;
}

static void hidh_callback(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    esp_hidh_event_t event = (esp_hidh_event_t)id;
    esp_hidh_event_data_t *param = (esp_hidh_event_data_t *)event_data;

    switch (event) {
    case ESP_HIDH_OPEN_EVENT: {
        if (param->open.status == ESP_OK) {
            const uint8_t *bda = esp_hidh_dev_bda_get(param->open.dev);
            ESP_LOGI(TAG, ESP_BD_ADDR_STR " OPEN: %s", ESP_BD_ADDR_HEX(bda), esp_hidh_dev_name_get(param->open.dev));
            esp_hidh_dev_dump(param->open.dev, stdout);
        } else {
            ESP_LOGE(TAG, " OPEN failed!");
        }
        break;
    }
    case ESP_HIDH_BATTERY_EVENT: {
        const uint8_t *bda = esp_hidh_dev_bda_get(param->battery.dev);
        ESP_LOGI(TAG, ESP_BD_ADDR_STR " BATTERY: %d%%", ESP_BD_ADDR_HEX(bda), param->battery.level);
        break;
    }
    case ESP_HIDH_INPUT_EVENT: {
        const uint8_t *bda = esp_hidh_dev_bda_get(param->input.dev);
        ESP_LOGI(TAG, ESP_BD_ADDR_STR " INPUT: %8s, MAP: %2u, ID: %3u, Len: %d, Data:", ESP_BD_ADDR_HEX(bda), esp_hid_usage_str(param->input.usage), param->input.map_index, param->input.report_id, param->input.length);
        if (param->input.length > 0)
        {
            if (param->input.data[0] != 0)
            {
                mqtt_publish();
            }
        }
        ESP_LOG_BUFFER_HEX(TAG, param->input.data, param->input.length);
        break;
    }
    case ESP_HIDH_FEATURE_EVENT: {
        const uint8_t *bda = esp_hidh_dev_bda_get(param->feature.dev);
        ESP_LOGI(TAG, ESP_BD_ADDR_STR " FEATURE: %8s, MAP: %2u, ID: %3u, Len: %d", ESP_BD_ADDR_HEX(bda),
                 esp_hid_usage_str(param->feature.usage), param->feature.map_index, param->feature.report_id,
                 param->feature.length);
        ESP_LOG_BUFFER_HEX(TAG, param->feature.data, param->feature.length);
        break;
    }
    case ESP_HIDH_CLOSE_EVENT: {
        const uint8_t *bda = esp_hidh_dev_bda_get(param->close.dev);
        ESP_LOGI(TAG, ESP_BD_ADDR_STR " CLOSE: %s", ESP_BD_ADDR_HEX(bda), esp_hidh_dev_name_get(param->close.dev));
        if (param->close.dev == dev)
        {
            ESP_LOGI(TAG, "closed dev");
            dev = nullptr;
        }
        break;
    }
    default:
        ESP_LOGI(TAG, "EVENT: %d", event);
        break;
    }
}

#define SCAN_DURATION_SECONDS 5

void hid_demo_task(void *pvParameters)
{
    for (;;)
    {
        for (;;)
        {
            size_t results_len = 0;
            esp_hid_scan_result_t *results = NULL;
            ESP_LOGI(TAG, "SCAN...");
            //start scan for HID devices
            esp_hid_scan(SCAN_DURATION_SECONDS, &results_len, &results);
            ESP_LOGI(TAG, "SCAN: %u results", results_len);
            if (results_len) {
                esp_hid_scan_result_t *r = results;
                esp_hid_scan_result_t *cr = NULL;
                while (r) {
                    printf("  %s: " ESP_BD_ADDR_STR ", ", (r->transport == ESP_HID_TRANSPORT_BLE) ? "BLE" : "BT ", ESP_BD_ADDR_HEX(r->bda));
                    printf("RSSI: %d, ", r->rssi);
                    printf("USAGE: %s, ", esp_hid_usage_str(r->usage));
                    if (r->transport == ESP_HID_TRANSPORT_BLE) {
                        cr = r;
                        printf("APPEARANCE: 0x%04x, ", r->ble.appearance);
                        printf("ADDR_TYPE: '%s', ", ble_addr_type_str(r->ble.addr_type));
                    }
#if CONFIG_BT_HID_HOST_ENABLED
                    if (r->transport == ESP_HID_TRANSPORT_BT) {
                        cr = r;
                        printf("COD: %s[", esp_hid_cod_major_str(r->bt.cod.major));
                        esp_hid_cod_minor_print(r->bt.cod.minor, stdout);
                        printf("] srv 0x%03x, ", r->bt.cod.service);
                        print_uuid(&r->bt.uuid);
                        printf(", ");
                    }
#endif /* CONFIG_BT_HID_HOST_ENABLED */
                    printf("NAME: %s ", r->name ? r->name : "");
                    printf("\n");
                    r = r->next;
                }
                if (cr) {
                    //open the last result
                    dev = esp_hidh_dev_open(cr->bda, cr->transport, cr->ble.addr_type);
                }
                //free the results
                esp_hid_scan_results_free(results);

                if (dev != nullptr)
                {
                    break;
                }
            }
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
        printf("dev is %p\n", dev);
        while (dev != nullptr)
        {
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
    }
}

extern "C" void app_main(void)
{
    esp_err_t ret;
#if HID_HOST_MODE == HIDH_IDLE_MODE
    ESP_LOGE(TAG, "Please turn on BT HID host or BLE!");
    return;
#endif
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    gpio_pad_select_gpio(LED_PIN); 
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT); 

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_ERROR_CHECK( ret );
    ESP_LOGI(TAG, "setting hid gap, mode:%d", HID_HOST_MODE);
    ESP_ERROR_CHECK( esp_hid_gap_init(HID_HOST_MODE) );
    ESP_ERROR_CHECK( esp_ble_gattc_register_callback(esp_hidh_gattc_event_handler) );
    esp_hidh_config_t config = {
        .callback = hidh_callback,
        .event_stack_size = 4096,
        .callback_arg = NULL,
    };
    ESP_ERROR_CHECK( esp_hidh_init(&config) );

    xTaskCreate(&hid_demo_task, "hid_task", 6 * 1024, NULL, 2, NULL);

    ESP_LOGI(TAG, "start up wifi");
    mqtt_set_device_name("sonoff-s31-3");
    wifi_init_sta("esp_button", true);

    mqtt_on_state_published = [](bool on)
    {
        gpio_set_level(LED_PIN, on);
        current_state = on;
    };
    mqtt_start();
    start_webserver();
    mqtt_subscribe();
}
