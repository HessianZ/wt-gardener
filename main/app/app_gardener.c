//
// Created by Hessian on 2023/9/27.
//

#include <gpio.h>
#include <adc.h>
#include <aht.h>
#include <bh1750.h>
#include <esp_log.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "system/esp_check.h"
#include "app_gardener.h"
#include "espnow.h"

#define I2C_SCL GPIO_NUM_5
#define I2C_SDA GPIO_NUM_4

static const char *TAG = "app_gardener";


typedef struct {
    struct {
        uint8_t type;
        uint8_t version;
    } header;
    uint8_t battery;
    float temperature;
    float humidity;
    uint16_t light;
    uint16_t earthHumidity;
} wt_homegw_report_data_t;

void app_gardener_report(void)
{
    ESP_LOGI(TAG, "app_gardener_report start");

    // Init gpio 14, then make it high to power on the sensors
    gpio_config_t io_conf = {
            .intr_type = GPIO_INTR_DISABLE,
            .mode = GPIO_MODE_OUTPUT,
            .pin_bit_mask = (1ULL << GPIO_NUM_14),
            .pull_down_en = 0,
            .pull_up_en = 1,
    };
    gpio_config(&io_conf);
    gpio_set_level(GPIO_NUM_14, 1);

    wt_homegw_report_data_t reportData = {0};
    reportData.header.type = 0;
    reportData.header.version = 1;
    reportData.battery = 100;

//    i2c_master_init();

    esp_err_t ret = ESP_OK;
    i2c_dev_t dev;
    i2cdev_init();

    // read bh1750 light sensor
    ret = bh1750_init_desc(&dev, BH1750_ADDR_LO, I2C_NUM_0, I2C_SDA, I2C_SCL);
    if (ret == ESP_OK) {
//        ret = bh1750_power_on(&dev);
//        ESP_GOTO_ON_ERROR(ret, bh1750_free, TAG, "bh1750_power_on failed");

        ret = bh1750_setup(&dev, BH1750_MODE_ONE_TIME, BH1750_RES_LOW);
        ESP_GOTO_ON_ERROR(ret, bh1750_free, TAG, "bh1750_setup failed");

        vTaskDelay(30 / portTICK_PERIOD_MS);

        ret = bh1750_read(&dev, &reportData.light);
        ESP_GOTO_ON_ERROR(ret, bh1750_free, TAG, "bh1750_read failed");

//        ret = bh1750_power_down(&dev);
//        ESP_GOTO_ON_ERROR(ret, bh1750_free, TAG, "bh1750_power_down failed");

        bh1750_free:
        bh1750_free_desc(&dev);
    } else {
        ESP_LOGE(TAG, "bh1750_init_desc failed");
    }

    ESP_LOGI(TAG, "light: %d", reportData.light);


    // Read temps and humidity
    aht_t aht_dev;

    ret = aht_init_desc(&aht_dev, AHT_I2C_ADDRESS_GND, I2C_NUM_0, I2C_SDA, I2C_SCL);
    if (ret == ESP_OK) {
        ret = aht_init(&aht_dev);
        ESP_GOTO_ON_ERROR(ret, aht_free, TAG, "aht_init failed");

        vTaskDelay(30 / portTICK_PERIOD_MS);

        ret = aht_get_data(&aht_dev, &reportData.temperature, &reportData.humidity);
        ESP_GOTO_ON_ERROR(ret, aht_free, TAG, "Could not read data from AHT");

        aht_free:
        aht_free_desc(&aht_dev);
    } else {
        ESP_LOGE(TAG, "aht_init_desc failed");
    }

    ESP_LOGI(TAG, "temperature: %.2f, humidity: %.2f", reportData.temperature, reportData.humidity);

    // read earth humidity sensor

    adc_config_t adc_config = {
            .mode = ADC_READ_TOUT_MODE,
            .clk_div = 8,
    };

    ret = adc_init(&adc_config);
    if (ret == ESP_OK) {
        vTaskDelay(30 / portTICK_PERIOD_MS);
        ret = adc_read(&reportData.earthHumidity);
        ESP_GOTO_ON_ERROR(ret, adc_free, TAG, "earthHumidity adc read failed");

        adc_free:
        adc_deinit();
    } else {
        ESP_LOGE(TAG, "adc_init failed");
    }

    ESP_LOGI(TAG, "earthHumidity: %d", reportData.earthHumidity);

    ESP_ERROR_CHECK_WITHOUT_ABORT(espnow_send_data(&reportData, sizeof(reportData)));

    ESP_LOGI(TAG, "app_gardener_report end");
}