//
// Created by Hessian on 2023/9/26.
//

#include <sys/cdefs.h>
#include <gpio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "espnow.h"
#include "app_gardener.h"

static const char *TAG = "APP_MAIN";

typedef enum AppState {
    APP_WAITING_WIFI,
    APP_WAITING_IP,
    APP_RUNNING,
} AppState;

static AppState g_app_state = APP_WAITING_WIFI;

static void led_blink(int times, int interval_ms)
{
    for (int i = 0; i < times; i++) {
        gpio_set_level(GPIO_NUM_2, 0);
        vTaskDelay(interval_ms / portTICK_PERIOD_MS);
        gpio_set_level(GPIO_NUM_2, 1);
        vTaskDelay(interval_ms / portTICK_PERIOD_MS);
    }
}

_Noreturn static void led_task(void *param)
{
    gpio_config_t io_conf;

    //disable interrupt
    io_conf.intr_type = GPIO_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    //bit mask of the pins that you want to set,e.g.GPIO15/16
    io_conf.pin_bit_mask = (1ULL<<GPIO_NUM_2);
    //disable pull-down mode
    io_conf.pull_down_en = 0;
    //disable pull-up mode
    io_conf.pull_up_en = 0;
    //configure GPIO with the given settings
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    while (1) {
        switch (g_app_state) {
            case APP_WAITING_WIFI:
                led_blink(3, 500);
                break;
            case APP_WAITING_IP:
                led_blink(10, 100);
                break;
            case APP_RUNNING:
                gpio_set_level(GPIO_NUM_2, 0);

                vTaskDelay(1000 / portTICK_PERIOD_MS);
                break;
            default:
                break;
        }
    }
}


void app_main()
{
//    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("gpio", ESP_LOG_WARN);
    esp_log_level_set("sys_espnow", ESP_LOG_DEBUG);
    esp_log_level_set("app_gardener", ESP_LOG_DEBUG);
    esp_log_level_set("bh1750", ESP_LOG_DEBUG);

    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());
    ESP_LOGI(TAG, "[APP] Compile time: %s %s", __DATE__, __TIME__);

    ESP_ERROR_CHECK(nvs_flash_init());
//    ESP_ERROR_CHECK(esp_netif_init());
//    ESP_ERROR_CHECK(esp_event_loop_create_default());

//    xTaskCreate(led_task, "led_task", 2048, NULL, 1, NULL);


    espnow_wifi_init();
    ESP_LOGI(TAG, "[APP] wifi init done");

    espnow_task_init();
    ESP_LOGI(TAG, "[APP] espnow init done");

    app_gardener_report();
    ESP_LOGI(TAG, "[APP] report send done");
}

