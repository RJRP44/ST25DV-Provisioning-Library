/*
 * Written by RJRP - 30/10/2023
 * ST25 Provisioning Library for idf framework
 * This program is distributed under the MIT License
 */

#include <esp_err.h>
#include "st25dv_ndef.h"

#define DEFAULT_SCAN_LIST_SIZE 20
#define ST25DV_WRITE_EVENT 0x01

#define ST25DV_PROV_MAX_RETRY 2
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

#define ST25DV_PROV_RESPONSE_NDEF_TYPE "st25dv-prov/json"

esp_err_t st25dv_prov_setup();
esp_err_t st25dv_prov_interrupt(st25dv_config *st25dv, gpio_num_t gpio_num);
esp_err_t st25dv_prov_write_gpo_register(uint8_t st25dv_address, uint8_t config);
esp_err_t st25dv_prov_write_aps(st25dv_config st25dv);
void IRAM_ATTR st25dv_prov_isr_handler(void* arg);
void st25dv_prov_task(void* arg);
esp_err_t st25dv_wait_wifi(bool *connected);