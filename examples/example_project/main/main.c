#include <stdio.h>
#include <nvs_flash.h>
#include <st25dv.h>
#include <st25dv_provisioning.h>
#include <st25dv_registers.h>

void app_main(void)
{
    //Init NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    //Define the i2c bus configuration
    i2c_config_t i2c_config = {
            .mode = I2C_MODE_MASTER,
            .sda_io_num = 1,
            .scl_io_num = 2,
            .sda_pullup_en = GPIO_PULLUP_ENABLE,
            .scl_pullup_en = GPIO_PULLUP_ENABLE,
            .master.clk_speed = ST25DV_MAX_CLK_SPEED,
    };

    //Apply, init the configuration to the bus
    st25dv_init_i2c(I2C_NUM_1, i2c_config);

    //Start I²C session
    st25dv_open_session(ST25DV_SYSTEM_ADDRESS, 0x0000000000000000);

    bool session = false;
    st25dv_is_session_opened(ST25DV_USER_ADDRESS, &session);

    if (!session){
        printf("Incorrect I2C password, unable to continue...");
        return;
    }

    //Set GPO interrupts on ndef write
    uint8_t gpo_register = 0;
    gpo_register |= BIT_GPO1_RF_USER_EN;
    gpo_register |= BIT_GPO1_RF_ACTIVITY_EN;

    st25dv_prov_write_gpo_register(ST25DV_SYSTEM_ADDRESS, gpo_register);

    st25dv_prov_write_aps();

}
