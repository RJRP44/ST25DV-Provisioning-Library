/*
 * Written by RJRP - 30/10/2023
 * ST25 Provisioning Library for idf framework
 * This program is distributed under the MIT License
 */

#include <memory.h>
#include <st25dv_provisioning.h>
#include <st25dv_ndef.h>
#include <st25dv.h>
#include <st25dv_registers.h>
#include <esp_wifi.h>
#include <esp_log.h>
#include <esp_event.h>

esp_err_t st25dv_prov_write_gpo_register(uint8_t st25dv_address, uint8_t config) {
    return st25dv_write_byte(st25dv_address, REG_GPO1, config);
}

esp_err_t st25dv_prov_write_aps() {
    //Init Wi-Fi
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    uint16_t number = DEFAULT_SCAN_LIST_SIZE;
    wifi_ap_record_t ap_info[DEFAULT_SCAN_LIST_SIZE];
    uint16_t ap_count = 0;
    memset(ap_info, 0, sizeof(ap_info));

    //Start the scan
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();
    esp_wifi_scan_start(NULL, true);
    esp_wifi_scan_get_ap_records(&number, ap_info);
    esp_wifi_scan_get_ap_num(&ap_count);

    //Create a json with all the data
    uint16_t address = CCFILE_LENGTH;
    cJSON *aps = cJSON_CreateArray();

    for (int i = 0; i < number; i++) {
        cJSON *ap = cJSON_CreateObject();
        cJSON_AddStringToObject(ap, "ssid", (const char *) ap_info[i].ssid);
        cJSON_AddNumberToObject(ap, "rssi", ap_info[i].rssi);
        cJSON_AddNumberToObject(ap, "auth", ap_info[i].authmode);
        cJSON_AddItemToArray(aps,ap);
    }

    //Write the json in st25dv memory
    st25dv_ndef_write_json_record(ST25DV_USER_ADDRESS, &address, true,true, aps);

    cJSON_Delete(aps);
    return ESP_OK;
}