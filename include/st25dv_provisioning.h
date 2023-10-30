/*
 * Written by RJRP - 30/10/2023
 * ST25 Provisioning Library for idf framework
 * This program is distributed under the MIT License
 */

#include <esp_err.h>
#define DEFAULT_SCAN_LIST_SIZE 20

esp_err_t st25dv_prov_write_gpo_register(uint8_t st25dv_address, uint8_t config);
esp_err_t st25dv_prov_write_aps();