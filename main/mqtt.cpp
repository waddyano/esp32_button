#include "esp_log.h"
#include "mqtt.h"
#include "mqtt_client.h"
#include "cJSON.h"
#include <functional>
//#include "driver/gpio.h"

static const char *TAG = "MQTT";

static char device_name[32];

static std::function<void()> on_connect;
static esp_mqtt_client_handle_t mqtt_client = nullptr;

std::function<void(bool)> mqtt_on_state_published;

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

static void parse_json(int length, const char *data)
{
    const char *end;
    cJSON *root = cJSON_ParseWithLengthOpts(data, length, &end, 0);
    ESP_LOGI(TAG, "Parsed %p %.*s - end %p after %p", root, (int)(data - end), data, end, data);
    if (end < data + length)
    {
        ESP_LOGI(TAG, "Failed at %c", *end);
    }
    auto *item = cJSON_GetObjectItem(root, "POWER");
    if (!cJSON_IsString(item))
    {
        ESP_LOGI(TAG, "No Power! %p", item);
    }
    else
    {
        ESP_LOGI(TAG, "Power: %s", item->valuestring);
        if (mqtt_on_state_published)
        {
            mqtt_on_state_published(strcmp(item->valuestring, "ON") == 0);
        }
        //gpio_set_level(GPIO_NUM_2, (int)strcmp(item->valuestring, "ON") == 0);
    }
    cJSON_Delete(root);
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    esp_mqtt_event_handle_t event = static_cast<esp_mqtt_event_handle_t>(event_data);
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        if (on_connect)
        {
            on_connect();
        }
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;
    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DELETED:
        ESP_LOGI(TAG, "MQTT_EVENT_DELETED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        parse_json(event->data_len, event->data);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) 
        {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));

        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

void mqtt_set_device_name(const char *name)
{
    strlcpy(device_name, name, sizeof(device_name));
}

void mqtt_publish()
{
    char message[64];
    snprintf(message, sizeof(message), "cmnd/%s/Power", device_name);
    int msg_id = esp_mqtt_client_publish(mqtt_client, message, "toggle", 0, 1, 0);
    ESP_LOGI(TAG, "sent power publish successful, msg_id=%d", msg_id);
}

void mqtt_publish_state()
{
    char message[64];
    snprintf(message, sizeof(message), "cmnd/%s/state", device_name);
    int msg_id = esp_mqtt_client_publish(mqtt_client, message, nullptr, 0, 1, 0);
    ESP_LOGI(TAG, "sent state publish successful, msg_id=%d", msg_id);
}

void mqtt_subscribe()
{
    on_connect = []()
    {
        char message[64];
        snprintf(message, sizeof(message), "stat/%s/RESULT", device_name);
        int msg_id = esp_mqtt_client_subscribe(mqtt_client, message, 0);
        ESP_LOGI(TAG, "subscribe successful, msg_id=%d", msg_id);
        mqtt_publish_state();
    };
}


void mqtt_start()
{
    esp_mqtt_client_config_t mqtt_cfg{};
    mqtt_cfg.uri = "mqtt://raspberrypi.home";

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(mqtt_client, MQTT_EVENT_ANY, mqtt_event_handler, NULL);
    esp_mqtt_client_start(mqtt_client);
}