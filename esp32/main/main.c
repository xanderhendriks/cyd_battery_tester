#include <stdio.h>
#include <string.h>
#include <math.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <freertos/semphr.h>

#include <esp_system.h>
#include <esp_log.h>
#include <esp_err.h>
#include <esp_check.h>
#include <esp_gdbstub.h>

#include <lvgl.h>
#include <esp_lvgl_port.h>

#include "battery.h"
#include "lcd.h"
#include "touch.h"
#include "esp_app_desc.h"
#include "user_interface.h"

static const char *TAG="main";

ui_screen_page_t screen_pages[] = {
    {
        .property_boxes = {
            {0, "State of charge", POSITION_FULL, PROPERTY_ID_STATE_OF_CHARGE, "-- %"},
            {1, "Voltage", POSITION_LEFT, PROPERTY_ID_VOLTAGE, "-.- V"},
            {1, "Current", POSITION_RIGHT, PROPERTY_ID_CURRENT, "-.- mA"},
            {2, "Safety status", POSITION_LEFT, PROPERTY_ID_SAFETY_STATUS, "-"},
            {2, "Safety alert", POSITION_RIGHT, PROPERTY_ID_SAFETY_ALERT, "-"},
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

static void button_press_cb(lv_event_t *event)
{
    lv_event_code_t event_code = lv_event_get_code(event);
    ui_btn_id_t btn = (ui_btn_id_t) lv_event_get_user_data(event);

    // ESP_LOGI(TAG, "Button action");

    if (event_code == LV_EVENT_CLICKED)
    {
        // if (btn == UI_BTN_ID_NEXT_PAGE)
        // {
            ESP_LOGI(TAG, "Next page");
            ui_next_page();
        // }
        // else if (btn == UI_BTN_ID_RESET_BATTERY)
        // {
        //     ESP_LOGI(TAG, "Resetting battery");
        //     battery_reset();
        // }
    }
}

static bool update_property_values_cb(ui_property_box_data_t* property_box_data)
{
    uint16_t register_value = 0;
    uint32_t word_register_value = 0;
    size_t length;
    uint8_t data_buffer[24];
    char value_buffer[64];
    esp_err_t error = ESP_FAIL;
    bool updated = false;

    switch(property_box_data->property_id)
    {
        case PROPERTY_ID_STATE_OF_CHARGE:
            error = battery_state_of_charge_get(&register_value);
            snprintf(value_buffer, sizeof(value_buffer), "%s %d %%", register_value >= 25 ? "#00FF00" : "#FF0000", register_value);
            break;

        case PROPERTY_ID_VOLTAGE:
            error = battery_voltage_get(&register_value);
            snprintf(value_buffer, sizeof(value_buffer), "%s %0.1f V", (register_value >= 9000) && (register_value <= 13000) ? "#00FF00" : "#FF0000", (float) register_value / 1000);
            break;

        case PROPERTY_ID_CURRENT:
            error = battery_current_get(&register_value);
            snprintf(value_buffer, sizeof(value_buffer), "%s %hd mA", (int16_t) register_value >= -50 ? "#00FF00" : "#FF0000", register_value);
            break;

        case PROPERTY_ID_SAFETY_STATUS:
            error = battery_safety_status_get(&word_register_value);
            snprintf(value_buffer, sizeof(value_buffer), "%s %08lX", word_register_value == 0 ? "#00FF00" : "#FF0000", word_register_value);
            break;

        case PROPERTY_ID_SAFETY_ALERT:
            error = battery_safety_alert_get(&word_register_value);
            snprintf(value_buffer, sizeof(value_buffer), "%s %08lX", word_register_value == 0 ? "#00FF00" : "#FF0000", word_register_value);
            break;

        case PROPERTY_ID_STATUS:
            error = battery_status_get(&register_value);
            // Read a 2nd time if the status indicates unknown
            if ((register_value & 0x0007) == 0x0007)
            {
                error = battery_status_get(&register_value);
            }
            snprintf(value_buffer, sizeof(value_buffer), "%s %04X", (register_value & 0b1011111100001111) == 0b0000000000000000 ? "#00FF00" : "#FF0000", register_value);
            break;

        case PROPERTY_ID_NAME:
            length = sizeof(data_buffer);
            error = battery_name_get((char*) data_buffer, &length);
            snprintf(value_buffer, sizeof(value_buffer), "%s", data_buffer);
            break;

        case PROPERTY_ID_VERSION:
            error = battery_version_get(&register_value);
            snprintf(value_buffer, sizeof(value_buffer), "%s %d", (register_value >= 3) && (register_value <= 4) ? "#00FF00" : "#FF0000", register_value);
            break;

        case PROPERTY_ID_FW_VERSION:
            snprintf(value_buffer, sizeof(value_buffer), "%s", esp_app_get_description()->version);
            error = ESP_OK;
            break;

        default:
            break;
    }

    if (error != ESP_OK)
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
    // Use for serial debugging
    // esp_gdbstub_init();

    ESP_ERROR_CHECK(battery_init());
    ESP_ERROR_CHECK(ui_init(screen_pages, update_property_values_cb, button_press_cb));

    for (;;)
    {
        ui_update();

        vTaskDelay(150 / portTICK_PERIOD_MS);
    }
}
