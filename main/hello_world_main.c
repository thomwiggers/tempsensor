/* Hello World Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_pm.h"
#include "power_save.h"
#include "ds18b20.h"
#include "pushing.h"

static const char *TAG = "main";

void print_info() {
    /* Print chip information */
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    printf("This is ESP32 chip with %d CPU cores, WiFi%s%s, ",
            chip_info.cores,
            (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
            (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");

    printf("silicon revision %d, ", chip_info.revision);

    printf("%dMB %s flash\n", spi_flash_get_chip_size() / (1024 * 1024),
            (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");
}

/* Sourced from wifi/power_save example */
void init_device() {
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );

#if CONFIG_PM_ENABLE
    // Configure dynamic frequency scaling:
    // maximum and minimum frequencies are set in sdkconfig,
    // automatic light sleep is enabled if tickless idle support is enabled.
    esp_pm_config_esp32_t pm_config = {
            .max_freq_mhz = CONFIG_EXAMPLE_MAX_CPU_FREQ_MHZ,
            .min_freq_mhz = CONFIG_EXAMPLE_MIN_CPU_FREQ_MHZ,
#if CONFIG_FREERTOS_USE_TICKLESS_IDLE
            .light_sleep_enable = true
#endif
    };
    ESP_ERROR_CHECK( esp_pm_configure(&pm_config) );
#endif // CONFIG_PM_ENABLE
}

float measure_and_submit() {
    float temp = ds18b20_get_temp();
    ESP_LOGI(TAG, "Temp = %f", temp);

    if (temp > 45 || temp < 5) {
        ESP_LOGE(TAG, "Invalid temperature");
        return -1;
    }

    if (submit_temperature(temp) == 0) {
       ESP_LOGI(TAG, "Submitting temperature failed");
       vTaskDelay(10000 / portTICK_PERIOD_MS);
    }

    return temp;
}


void app_main()
{
    ESP_LOGI(TAG, "Temperature sensor app loading...");
    print_info();
    init_device();
    wifi_power_save();


    // read temp sensor
    ds18b20_init(23);

    ESP_LOGI(TAG, "Waiting for initialisation...");
    vTaskDelay(4000 / portTICK_PERIOD_MS);

    float temp;
    int attempts = 10;
    while(attempts > 0 || (temp = measure_and_submit()) > -1.0) {
        if (temp <= 0) {
            attempts--;
        }
        vTaskDelay(20000 / portTICK_PERIOD_MS);
    }

    ESP_LOGI(TAG, "Exited loop");
    ESP_LOGI(TAG, "Restarting now.\n");
    fflush(stdout);
    esp_restart();
}
