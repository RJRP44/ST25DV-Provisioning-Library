#include <stdio.h>
#include <nvs_flash.h>
#include <st25dv_provisioning.h>
#include <st25dv_ndef.h>
#include <memory.h>
#include "led_strip.h"

//If you are using an esp32s3 devborad with a LED
#define EXAMPLE_ESP32S3_MINI_DEVBOARD true

#if EXAMPLE_ESP32S3_MINI_DEVBOARD
static led_strip_handle_t led_strip;
#endif

void app_main(void) {
    //Init NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
#if EXAMPLE_ESP32S3_MINI_DEVBOARD
    led_strip_config_t strip_config = {
            .strip_gpio_num = GPIO_NUM_48,
            .max_leds = 1, // at least one LED on board
    };

    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
        .flags.with_dma = false,
    };
    led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip);

    led_strip_set_pixel(led_strip, 0, 0, 0, 0);
    //Refresh the strip to send data
    led_strip_refresh(led_strip);
#endif

    //Define the i2c bus configuration
    i2c_config_t i2c_config = {
            .mode = I2C_MODE_MASTER,
            .sda_io_num = 1,
            .scl_io_num = 2,
            .sda_pullup_en = GPIO_PULLUP_ENABLE,
            .scl_pullup_en = GPIO_PULLUP_ENABLE,
            .master.clk_speed = ST25DV_MAX_CLK_SPEED,
    };

    st25dv_config st25dv = {
            ST25DV_USER_ADDRESS,
            ST25DV_SYSTEM_ADDRESS
    };

    //Apply, init the configuration to the bus
    st25dv_init_i2c(I2C_NUM_1, i2c_config);

    //Clear memory
    uint8_t *blank = malloc(512);
    memset(blank, 0x00, 512);
    st25dv_write(ST25DV_USER_ADDRESS, 0x00, blank, 512);
    free(blank);

    //Wait before writing again
    vTaskDelay(300 / portTICK_PERIOD_MS);

    st25dv_ndef_write_ccfile(0x00040000010040E2);

    //Wait before writing again
    vTaskDelay(300 / portTICK_PERIOD_MS);

    //Start I²C session
    st25dv_open_session(ST25DV_SYSTEM_ADDRESS, 0x0000000000000000);

    bool session = false;
    st25dv_is_session_opened(ST25DV_USER_ADDRESS, &session);

    if (!session) {
        printf("Incorrect I2C password, unable to continue...");
        return;
    }
    st25dv_prov_setup();

    vTaskDelay(1000 / portTICK_PERIOD_MS);

    st25dv_prov_write_aps(st25dv);

#if EXAMPLE_ESP32S3_MINI_DEVBOARD
    led_strip_set_pixel(led_strip, 0, 0, 16, 16);
    //Refresh the strip to send data
    led_strip_refresh(led_strip);
#endif

    //Wait for the user to write the wanted Wi-Fi
    st25dv_prov_interrupt(&st25dv, GPIO_NUM_17);

    bool connected = false;

    while (1) {
        st25dv_wait_wifi(&connected);
        if (connected){
            printf("Connected !\n");
#if EXAMPLE_ESP32S3_MINI_DEVBOARD
            led_strip_set_pixel(led_strip, 0, 0, 16, 0);
            //Refresh the strip to send data
            led_strip_refresh(led_strip);
#endif
            return;
        }
        printf("Connection failed, try again...\n");
#if EXAMPLE_ESP32S3_MINI_DEVBOARD
        led_strip_set_pixel(led_strip, 0, 16, 0, 0);
        //Refresh the strip to send data
        led_strip_refresh(led_strip);
#endif
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
