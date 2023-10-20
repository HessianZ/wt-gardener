/* ESPNOW Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

/*
   This example shows how to use ESPNOW.
   Prepare two device, one for sending ESPNOW data and another for receiving
   ESPNOW data.
*/
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <assert.h>
#include <esp_sleep.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "nvs_flash.h"
#include "esp_event_loop.h"
#include "tcpip_adapter.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_system.h"
#include "rom/ets_sys.h"
#include "rom/crc.h"
#include "espnow.h"

static const char *TAG = "sys_espnow";

static xQueueHandle example_espnow_queue;

static uint8_t example_broadcast_mac[ESP_NOW_ETH_ALEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

/* WiFi should start before using ESPNOW */
void espnow_wifi_init(void) {
    tcpip_adapter_init();

    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));
    ESP_ERROR_CHECK(esp_wifi_set_mode(ESPNOW_WIFI_MODE));
    ESP_ERROR_CHECK(esp_wifi_start());

    /* In order to simplify example, channel is set after WiFi started.
     * This is not necessary in real application if the two devices have
     * been already on the same channel.
     */
    ESP_ERROR_CHECK(esp_wifi_set_channel(CONFIG_ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE));
}

/* ESPNOW sending or receiving callback function is called in WiFi task.
 * Users should not do lengthy operations from this task. Instead, post
 * necessary data to a queue and handle it from a lower priority task. */
static void espnow_send_cb(const uint8_t *mac_addr, esp_now_send_status_t status) {
    example_espnow_event_t evt;
    example_espnow_event_send_cb_t *send_cb = &evt.info.send_cb;

    if (mac_addr == NULL) {
        ESP_LOGE(TAG, "Send cb arg error");
        return;
    }

    evt.id = EXAMPLE_ESPNOW_SEND_CB;
    memcpy(send_cb->mac_addr, mac_addr, ESP_NOW_ETH_ALEN);
    send_cb->status = status;
    if (xQueueSend(example_espnow_queue, &evt, portMAX_DELAY) != pdTRUE) {
        ESP_LOGW(TAG, "Send send queue fail");
    }
}

/* Prepare ESPNOW data to be sent. */
static void espnow_sign_userdata(espnow_send_param_t *send_param) {
    espnow_userdata_t *buf = (espnow_userdata_t *) send_param->buffer;

    assert(send_param->len >= sizeof(espnow_userdata_t));

    buf->type = IS_BROADCAST_ADDR(send_param->dest_mac) ? EXAMPLE_ESPNOW_DATA_BROADCAST : EXAMPLE_ESPNOW_DATA_UNICAST;
    buf->crc = 0;
    buf->magic = esp_random();
    buf->crc = crc16_le(UINT16_MAX, (uint8_t const *) buf, send_param->len);
}

static void espnow_task(void *pvParameter) {
    ESP_LOGD(TAG, "espnow_task start");

    example_espnow_event_t evt;

    while (xQueueReceive(example_espnow_queue, &evt, portMAX_DELAY) == pdTRUE) {
        switch (evt.id) {
            case EXAMPLE_ESPNOW_SEND_CB: {
                example_espnow_event_send_cb_t *send_cb = &evt.info.send_cb;

                ESP_LOGI(TAG, "Send data to "MACSTR", status1: %d", MAC2STR(send_cb->mac_addr), send_cb->status);

                // go deep sleep
                uint32_t sleep_time_sec = CONFIG_GARDENER_SLEEP_DURATION;
                ESP_LOGI(TAG, "esp_deep_sleep %ds", sleep_time_sec);
                esp_deep_sleep(sleep_time_sec * 1000000UL);

                vTaskDelete(NULL);

                break;
            }
            default:
                ESP_LOGE(TAG, "Callback type error: %d", evt.id);
                break;
        }
    }
}

esp_err_t espnow_task_init(void) {
    example_espnow_queue = xQueueCreate(ESPNOW_QUEUE_SIZE, sizeof(example_espnow_event_t));
    if (example_espnow_queue == NULL) {
        ESP_LOGE(TAG, "Create mutex fail");
        return ESP_FAIL;
    }

    /* Initialize ESPNOW and register sending and receiving callback function. */
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_send_cb(espnow_send_cb));

    /* Set primary master key. */
    ESP_ERROR_CHECK(esp_now_set_pmk((uint8_t *) CONFIG_ESPNOW_PMK));

    /* Add broadcast peer information to peer list. */
    esp_now_peer_info_t *peer = malloc(sizeof(esp_now_peer_info_t));
    if (peer == NULL) {
        ESP_LOGE(TAG, "Malloc peer information fail");
        vSemaphoreDelete(example_espnow_queue);
        esp_now_deinit();
        return ESP_FAIL;
    }
    memset(peer, 0, sizeof(esp_now_peer_info_t));
    peer->channel = CONFIG_ESPNOW_CHANNEL;
    peer->ifidx = ESPNOW_WIFI_IF;
    peer->encrypt = false;
    memcpy(peer->peer_addr, example_broadcast_mac, ESP_NOW_ETH_ALEN);
    ESP_ERROR_CHECK(esp_now_add_peer(peer));
    free(peer);

    xTaskCreate(espnow_task, "espnow_task", 2048, NULL, 4, NULL);

    return ESP_OK;
}

esp_err_t espnow_send_data(void *data, size_t data_len) {
    /* Initialize sending parameters. */
    espnow_send_param_t *send_param;
    send_param = malloc(sizeof(espnow_send_param_t));
    memset(send_param, 0, sizeof(espnow_send_param_t));
    if (send_param == NULL) {
        ESP_LOGE(TAG, "Malloc send parameter fail");
        vSemaphoreDelete(example_espnow_queue);
        esp_now_deinit();
        return ESP_FAIL;
    }
    send_param->unicast = false;
    send_param->broadcast = true;

    memcpy(send_param->dest_mac, example_broadcast_mac, ESP_NOW_ETH_ALEN);

    send_param->len = sizeof(espnow_userdata_t) + data_len;
    send_param->buffer = malloc(send_param->len);
    if (send_param->buffer == NULL) {
        ESP_LOGE(TAG, "Malloc send buffer fail");
        free(send_param);
        vSemaphoreDelete(example_espnow_queue);
        esp_now_deinit();
        return ESP_FAIL;
    }

    espnow_userdata_t *buf = (espnow_userdata_t *) send_param->buffer;

    memcpy(buf->payload, data, data_len);

    espnow_sign_userdata(send_param);

    esp_err_t ret = esp_now_send(send_param->dest_mac, send_param->buffer, send_param->len);

    free(send_param->buffer);

    return ret;
}

static void espnow_deinit(espnow_send_param_t *send_param) {
    free(send_param->buffer);
    free(send_param);
    vSemaphoreDelete(example_espnow_queue);
    esp_now_deinit();
}
