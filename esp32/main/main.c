#include <stdio.h>
#include <math.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <freertos/semphr.h>

#include <esp_system.h>
#include <esp_log.h>
#include <esp_err.h>
#include <esp_check.h>

#include <lvgl.h>
#include <esp_lvgl_port.h>

#include "driver/i2c.h"
#include "smbus.h"
#include "lcd.h"
#include "touch.h"
#include "esp_app_desc.h"
#include "user_interface.h"

#define I2C_MASTER_SCL_IO    22    // GPIO number for I2C master clock
#define I2C_MASTER_SDA_IO    27    // GPIO number for I2C master data
#define I2C_MASTER_NUM       I2C_NUM_0  // I2C port number for master dev
#define I2C_MASTER_FREQ_HZ   100000     // I2C master clock frequency
#define I2C_MASTER_TX_BUF_DISABLE   0   // I2C master doesn't need buffer
#define I2C_MASTER_RX_BUF_DISABLE   0   // I2C master doesn't need buffer
#define I2C_MASTER_TIMEOUT_MS 1000
#define I2C_BMS_ADDRESS (0x0B)

static const char *TAG="demo";
static smbus_info_t smbus_info;

ui_screen_page_t screen_pages[] = {
    {
        .property_boxes = {
            {0, "State of charge", POSITION_FULL, PROPERTY_ID_STATE_OF_CHARGE, "-- %"},
            {1, "Voltage", POSITION_FULL, PROPERTY_ID_VOLTAGE, "-.- V"},
            {2, "Current", POSITION_FULL, PROPERTY_ID_CURRENT, "-.- mA"},
            {3, "Version", POSITION_LEFT, PROPERTY_ID_VERSION, "-"},
            {3, "Status", POSITION_RIGHT, PROPERTY_ID_STATUS, "-"},
            {UI_END_OF_LIST_MARKER}
        }
    },
    {
        .property_boxes = {
            {0, "Name", POSITION_FULL, PROPERTY_ID_NAME, "-"},
            {1, "FW Version", POSITION_FULL, PROPERTY_ID_FW_VERSION, "-"},
            {UI_END_OF_LIST_MARKER}
        }
    },
    {
        .property_boxes = {
            {UI_END_OF_LIST_MARKER}
        }
    },
};

static esp_err_t i2c_master_init(void)
{
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = I2C_MASTER_SDA_IO;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = I2C_MASTER_SCL_IO;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
    conf.clk_flags = 0;
    i2c_param_config(I2C_MASTER_NUM, &conf);
    return i2c_driver_install(I2C_MASTER_NUM, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);
}

static void button_press_cb(lv_event_t *event)
{
    lv_event_code_t event_code = lv_event_get_code(event);
    lv_obj_t *btn = (lv_obj_t *)lv_event_get_user_data(event);

    btn = btn;

    if (event_code == LV_EVENT_CLICKED)
    {
        ui_next_page();
        ESP_LOGI(TAG, "button pressed");
    }
}

static bool update_property_values_cb(ui_property_box_data_t* property_box_data)
{
    uint16_t register_value;
    uint8_t length;
    uint8_t data_buffer[24];
    char value_buffer[64];
    esp_err_t error = ESP_FAIL;
    bool updated = false;

    switch(property_box_data->property_id)
    {
        case PROPERTY_ID_STATE_OF_CHARGE:
            error = smbus_read_word(&smbus_info, 0x0D, &register_value);
            snprintf(value_buffer, sizeof(value_buffer), "%d %%", register_value);
            break;

        case PROPERTY_ID_VOLTAGE:
            error = smbus_read_word(&smbus_info, 0x09, &register_value);
            snprintf(value_buffer, sizeof(value_buffer), "%0.1f V", (float) register_value / 1000);
            break;

        case PROPERTY_ID_CURRENT:
            error = smbus_read_word(&smbus_info, 0x0A, &register_value);
            snprintf(value_buffer, sizeof(value_buffer), "%hd mA", register_value);
            break;

        case PROPERTY_ID_STATUS:
            error = smbus_read_word(&smbus_info, 0x46, &register_value);
            snprintf(value_buffer, sizeof(value_buffer), "%s", (register_value & 0x0006) == 0x0006 ? "Pass" : "Fail");
            break;

        case PROPERTY_ID_NAME:
            length = sizeof(data_buffer);
            error = smbus_read_block(&smbus_info, 0x21, data_buffer, &length);
            snprintf(value_buffer, sizeof(value_buffer), "%*s", (int) length, data_buffer);
            break;

        case PROPERTY_ID_VERSION:
            error = smbus_read_word(&smbus_info, 0x0D, &register_value);
            snprintf(value_buffer, sizeof(value_buffer), "%d %%", register_value);
            break;

        case PROPERTY_ID_FW_VERSION:
            snprintf(value_buffer, sizeof(value_buffer), "%s", esp_app_get_description()->version);
            error = ESP_OK;
            break;

        default:
            break;
    }

    if (error == ESP_FAIL)
    {
        snprintf(value_buffer, sizeof(value_buffer), "%s", property_box_data->empty_value);
    }

    if (strcmp(value_buffer, property_box_data->value) != 0)
    {
        strcpy(property_box_data->value, value_buffer);
        updated = true;
    }

    return updated;
}

void app_main(void)
{
    ESP_ERROR_CHECK(i2c_master_init());
    ESP_ERROR_CHECK(ui_init(screen_pages, update_property_values_cb, button_press_cb));
    smbus_init(&smbus_info, I2C_MASTER_NUM, I2C_BMS_ADDRESS);

    for (;;)
    {
        ui_update_property_values();
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}
