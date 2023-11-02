/*
 * Written by RJRP - 30/10/2023
 * ST25 Provisioning Library for idf framework
 * This program is distributed under the MIT License
 */

#include <memory.h>
#include <st25dv_provisioning.h>
#include <st25dv_ndef.h>
#include <st25dv_registers.h>
#include <esp_wifi.h>
#include <esp_log.h>
#include <esp_event.h>
#include <freertos/event_groups.h>

static EventGroupHandle_t wifi_event_group;
static QueueHandle_t st25dv_prov_queue = NULL;
static uint8_t retry_num = 0;

static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    //Handle event to update event bits
    if (event_base == WIFI_EVENT){
        if (event_id == WIFI_EVENT_STA_START) {
            //Connect Wi-Fi
            esp_wifi_connect();

            return;
        }
        if (event_id == WIFI_EVENT_STA_DISCONNECTED) {

            if (retry_num < ST25DV_PROV_MAX_RETRY) {
                //Retry
                esp_wifi_connect();
                retry_num++;
            } else {
                xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
            }

            return;
        }
    }
    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP){
        //Connected to Wi-Fi !
        retry_num = 0;
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);

        return;
    }
}

esp_err_t st25dv_prov_setup() {

    //Set GPO interrupts on ndef write
    uint8_t gpo_register = 0;
    //gpo_register |= BIT_GPO1_RF_USER_EN;
    //gpo_register |= BIT_GPO1_RF_ACTIVITY_EN;
    gpo_register |= BIT_GPO1_GPO_EN;
    gpo_register |= BIT_GPO1_RF_WRITE_EN;

    st25dv_prov_write_gpo_register(ST25DV_SYSTEM_ADDRESS, gpo_register);

    //Init the message queue
    st25dv_prov_queue = xQueueCreate(10, sizeof(uint32_t));
    wifi_event_group = xEventGroupCreate();

    return ESP_OK;
}

esp_err_t st25dv_prov_interrupt(st25dv_config *st25dv, gpio_num_t gpio_num) {
    gpio_set_intr_type(gpio_num, GPIO_INTR_NEGEDGE);
    xTaskCreate(st25dv_prov_task, "st25dv_prov_task", 4096, st25dv, 10, NULL);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(gpio_num, st25dv_prov_isr_handler, (void *) ST25DV_WRITE_EVENT);

    return ESP_OK;
}

esp_err_t st25dv_prov_write_gpo_register(uint8_t st25dv_address, uint8_t config) {
    return st25dv_write_byte(st25dv_address, REG_GPO1, config);
}

esp_err_t st25dv_prov_write_aps(st25dv_config st25dv) {
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
    esp_wifi_disconnect();
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
        cJSON_AddItemToArray(aps, ap);
    }

    //Write the json in st25dv memory
    st25dv_ndef_write_json_record(st25dv, &address, true, true, aps);
    cJSON_Delete(aps);
    return ESP_OK;
}


void st25dv_prov_isr_handler(void *arg) {
    uint32_t event = (uint32_t) arg;
    xQueueSendFromISR(st25dv_prov_queue, &event, NULL);
}

void st25dv_prov_task(void *arg) {
    st25dv_config *st25dv = (st25dv_config *) arg;
    uint32_t event = 0;
    while (true) {
        if (xQueueReceive(st25dv_prov_queue, &event, portMAX_DELAY)) {
            if (event != ST25DV_WRITE_EVENT) {
                event = 0;
                continue;
            }

            vTaskDelay(5000 / portTICK_PERIOD_MS);
            event = 0;

            //Read the ndef data from the st25dv
            std25dv_ndef_record *record = malloc(sizeof(std25dv_ndef_record));
            memset(record, 0, sizeof(std25dv_ndef_record));

            uint8_t record_num = 1;
            uint8_t record_count = 0;
            st25dv_ndef_read(*st25dv, record_num, record, &record_count);

            char ndef_type[] = ST25DV_PROV_RESPONSE_NDEF_TYPE;

            //Check if the data written is for provisioning
            if (record->tnf != NDEF_ST25DV_TNF_MIME || strcmp(ndef_type, record->type) != 0) {
                continue;
            }

            cJSON *response = cJSON_Parse(record->payload);
            if (response == NULL) {
                continue;
            }

            //Get the provisioning data
            cJSON *ssid = cJSON_GetObjectItemCaseSensitive(response, "ssid");
            cJSON *password = cJSON_GetObjectItemCaseSensitive(response, "password");
            cJSON *auth = cJSON_GetObjectItemCaseSensitive(response, "auth");

            if (!cJSON_IsString(ssid) || !cJSON_IsString(password) || !cJSON_IsNumber(auth)) {
                continue;
            }

            //Stop and restart Wi-Fi
            esp_wifi_stop();

            wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
            esp_wifi_init(&cfg);

            //Start handlers
            esp_event_handler_instance_t instance_any_id;
            esp_event_handler_instance_t instance_got_ip;
            esp_event_handler_instance_register(WIFI_EVENT,ESP_EVENT_ANY_ID,&event_handler,NULL,&instance_any_id);
            esp_event_handler_instance_register(IP_EVENT,IP_EVENT_STA_GOT_IP,&event_handler,NULL,&instance_got_ip);

            //Apply readed configuration
            wifi_config_t wifi_config = {};


            if (strlen(ssid->valuestring) > 32){
                ESP_LOGE("St25dv-Prov","SSID length is too long, > 32 characters");
                continue;
            }

            if (strlen(password->valuestring) > 64){
                ESP_LOGE("St25dv-Prov","Password length is too long, > 64 characters");
                continue;
            }

            memcpy(wifi_config.sta.ssid, ssid->valuestring, strlen(ssid->valuestring));
            memcpy(wifi_config.sta.password, password->valuestring, strlen(password->valuestring));
            wifi_config.sta.threshold.authmode = auth->valueint;
            esp_wifi_set_mode(WIFI_MODE_STA);
            esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
            esp_wifi_start();

            //Delete json & record after use
            cJSON_Delete(response);
            st25dv_ndef_delete_records(record);
            vTaskDelete(NULL);
        }
    }
}

esp_err_t st25dv_wait_wifi(bool *connected){
    //Wait event
    EventBits_t bits = xEventGroupWaitBits(wifi_event_group,WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,pdFALSE,pdFALSE,portMAX_DELAY);

    //Update the value
    *connected = bits & WIFI_CONNECTED_BIT;

    if (bits & WIFI_CONNECTED_BIT ) {
        xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
        return ESP_OK;
    }

    if (bits & WIFI_FAIL_BIT) {
        xEventGroupClearBits(wifi_event_group, WIFI_FAIL_BIT);
        return ESP_OK;
    }
    return ESP_FAIL;
}