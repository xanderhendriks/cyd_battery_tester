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

#define I2C_MASTER_SCL_IO    22    // GPIO number for I2C master clock
#define I2C_MASTER_SDA_IO    27    // GPIO number for I2C master data
#define I2C_MASTER_NUM       I2C_NUM_0  // I2C port number for master dev
#define I2C_MASTER_FREQ_HZ   100000     // I2C master clock frequency
#define I2C_MASTER_TX_BUF_DISABLE   0   // I2C master doesn't need buffer
#define I2C_MASTER_RX_BUF_DISABLE   0   // I2C master doesn't need buffer
#define I2C_MASTER_TIMEOUT_MS 1000
#define I2C_BMS_ADDRESS (0x0B)

static const char *TAG="demo";
static lv_obj_t *lbl_state_of_charge;
static lv_obj_t *lbl_voltage;
static lv_obj_t *lbl_current;
static lv_obj_t *lbl_status;
static lv_obj_t *lbl_version;

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



void ui_event_Screen(lv_event_t *e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t *btn = (lv_obj_t *)lv_event_get_user_data(e);

    btn = btn;

    if (event_code == LV_EVENT_CLICKED)
    {
        ESP_LOGI(TAG, "button pressed");
    }
}


static esp_err_t app_lvgl_main(void)
{
    uint32_t height = 67;
    lv_obj_t *scr = lv_scr_act();
    lv_style_t style_font_24;
    lv_style_t style_font_36;

    lvgl_port_lock(0);

    lv_style_init(&style_font_24);
    lv_style_set_text_font(&style_font_24, &lv_font_montserrat_24);

    lv_style_init(&style_font_36);
    lv_style_set_text_font(&style_font_36, &lv_font_montserrat_36);

    lv_obj_t *btn_state_of_charge = lv_button_create(scr);
    lv_obj_align(btn_state_of_charge, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_size(btn_state_of_charge, 240, height);
    lv_obj_set_style_bg_color(btn_state_of_charge, lv_color_hex(0x15171A), LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(btn_state_of_charge, 2, LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(btn_state_of_charge, lv_color_white(), LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(btn_state_of_charge, 10, LV_STATE_DEFAULT);

    lv_obj_t *lbl_state_of_charge_text = lv_label_create(btn_state_of_charge);
    lv_label_set_text(lbl_state_of_charge_text, "State of charge");
    lv_obj_set_style_text_color(lbl_state_of_charge_text, lv_color_white(), LV_STATE_DEFAULT);
    lv_obj_align(lbl_state_of_charge_text, LV_ALIGN_TOP_MID, 0, 0);

    lbl_state_of_charge = lv_label_create(btn_state_of_charge);
    lv_label_set_text(lbl_state_of_charge, "-- %");
    lv_obj_set_style_text_color(lbl_state_of_charge, lv_color_white(), LV_STATE_DEFAULT);
    lv_obj_add_style(lbl_state_of_charge, &style_font_24, LV_PART_MAIN);
    lv_obj_align(lbl_state_of_charge, LV_ALIGN_BOTTOM_MID, 0, 0);

    lv_obj_t *btn_voltage = lv_button_create(scr);
    lv_obj_align(btn_voltage, LV_ALIGN_TOP_MID, 0, height);
    lv_obj_set_size(btn_voltage, 240, height);
    lv_obj_set_style_bg_color(btn_voltage, lv_color_hex(0x15171A), LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(btn_voltage, 2, LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(btn_voltage, lv_color_white(), LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(btn_voltage, 10, LV_STATE_DEFAULT);

    lv_obj_t *lbl_voltage_text = lv_label_create(btn_voltage);
    lv_label_set_text(lbl_voltage_text, "Voltage");
    lv_obj_set_style_text_color(lbl_voltage_text, lv_color_white(), LV_STATE_DEFAULT);
    lv_obj_align(lbl_voltage_text, LV_ALIGN_TOP_MID, 0, 0);

    lbl_voltage = lv_label_create(btn_voltage);
    lv_label_set_text(lbl_voltage, "-.- V");
    lv_obj_set_style_text_color(lbl_voltage, lv_color_white(), LV_STATE_DEFAULT);
    lv_obj_add_style(lbl_voltage, &style_font_24, LV_PART_MAIN);
    lv_obj_align(lbl_voltage, LV_ALIGN_BOTTOM_MID, 0, 0);

    lv_obj_t *btn_current = lv_button_create(scr);
    lv_obj_align(btn_current, LV_ALIGN_TOP_MID, 0, 2*height);
    lv_obj_set_size(btn_current, 240, height);
    lv_obj_set_style_bg_color(btn_current, lv_color_hex(0x15171A), LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(btn_current, 2, LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(btn_current, lv_color_white(), LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(btn_current, 10, LV_STATE_DEFAULT);

    lv_obj_t *lbl_current_text = lv_label_create(btn_current);
    lv_label_set_text(lbl_current_text, "Current");
    lv_obj_set_style_text_color(lbl_current_text, lv_color_white(), LV_STATE_DEFAULT);
    lv_obj_align(lbl_current_text, LV_ALIGN_TOP_MID, 0, 0);

    lbl_current = lv_label_create(btn_current);
    lv_label_set_text(lbl_current, "-.- mA");
    lv_obj_set_style_text_color(lbl_current, lv_color_white(), LV_STATE_DEFAULT);
    lv_obj_add_style(lbl_current, &style_font_24, LV_PART_MAIN);
    lv_obj_align(lbl_current, LV_ALIGN_BOTTOM_MID, 0, 0);

    lv_obj_t *btn_status = lv_button_create(scr);
    lv_obj_align(btn_status, LV_ALIGN_TOP_LEFT, 0, 3*height);
    lv_obj_set_size(btn_status, 120, height);
    lv_obj_set_style_bg_color(btn_status, lv_color_hex(0x15171A), LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(btn_status, 2, LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(btn_status, lv_color_white(), LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(btn_status, 10, LV_STATE_DEFAULT);

    lv_obj_t *lbl_status_text = lv_label_create(btn_status);
    lv_label_set_text(lbl_status_text, "Status");
    lv_obj_set_style_text_color(lbl_status_text, lv_color_white(), LV_STATE_DEFAULT);
    lv_obj_align(lbl_status_text, LV_ALIGN_TOP_MID, 0, 0);

    lbl_status = lv_label_create(btn_status);
    lv_label_set_text(lbl_status, "-");
    lv_obj_set_style_text_color(lbl_status, lv_color_white(), LV_STATE_DEFAULT);
    lv_obj_add_style(lbl_status, &style_font_24, LV_PART_MAIN);
    lv_obj_align(lbl_status, LV_ALIGN_BOTTOM_MID, 0, 0);

    lv_obj_t *btn_version = lv_button_create(scr);
    lv_obj_align(btn_version, LV_ALIGN_TOP_RIGHT, 0, 3*height);
    lv_obj_set_size(btn_version, 120, height);
    lv_obj_set_style_bg_color(btn_version, lv_color_hex(0x15171A), LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(btn_version, 2, LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(btn_version, lv_color_white(), LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(btn_version, 10, LV_STATE_DEFAULT);

    lv_obj_t *lbl_version_text = lv_label_create(btn_version);
    lv_label_set_text(lbl_version_text, "version");
    lv_obj_set_style_text_color(lbl_version_text, lv_color_white(), LV_STATE_DEFAULT);
    lv_obj_align(lbl_version_text, LV_ALIGN_TOP_MID, 0, 0);

    lbl_version = lv_label_create(btn_version);
    lv_label_set_text(lbl_version, "-");
    lv_obj_set_style_text_color(lbl_version, lv_color_white(), LV_STATE_DEFAULT);
    lv_obj_add_style(lbl_version, &style_font_24, LV_PART_MAIN);
    lv_obj_align(lbl_version, LV_ALIGN_BOTTOM_MID, 0, 0);

    lv_obj_t *btn_reset = lv_button_create(scr);
    lv_obj_align(btn_reset, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_size(btn_reset, 240, 50);
    lv_obj_set_style_bg_color(btn_reset, lv_color_make(0xe7, 0x7b, 0x0f), LV_STATE_DEFAULT);
    lv_obj_add_event_cb(btn_reset, ui_event_Screen, LV_EVENT_ALL, btn_reset);

    lv_obj_t *lbl_reset = lv_label_create(btn_reset);
    lv_label_set_text(lbl_reset, "reset battery");
    lv_obj_set_style_text_color(lbl_reset, lv_color_white(), LV_STATE_DEFAULT);
    lv_obj_add_style(lbl_reset, &style_font_24, LV_PART_MAIN);
    lv_obj_align(lbl_reset, LV_ALIGN_CENTER, 0, 0);

    lvgl_port_unlock();

    return ESP_OK;
}


void app_main(void)
{
    esp_lcd_panel_io_handle_t lcd_io;
    esp_lcd_panel_handle_t lcd_panel;
    esp_lcd_touch_handle_t tp;
    lvgl_port_touch_cfg_t touch_cfg;
    lv_display_t *lvgl_display = NULL;

    ESP_ERROR_CHECK(lcd_display_brightness_init());

    ESP_ERROR_CHECK(app_lcd_init(&lcd_io, &lcd_panel));
    lvgl_display = app_lvgl_init(lcd_io, lcd_panel);
    if (lvgl_display == NULL)
    {
        ESP_LOGI(TAG, "fatal error in app_lvgl_init");
        esp_restart();
    }

    ESP_ERROR_CHECK(touch_init(&tp));
    touch_cfg.disp = lvgl_display;
    touch_cfg.handle = tp;
    lvgl_port_add_touch(&touch_cfg);

    ESP_ERROR_CHECK(lcd_display_brightness_set(75));
    ESP_ERROR_CHECK(lcd_display_rotate(lvgl_display, LV_DISPLAY_ROTATION_180));
    ESP_ERROR_CHECK(app_lvgl_main());

    ESP_ERROR_CHECK(i2c_master_init());
    smbus_info_t smbus_info;
    smbus_init(&smbus_info, I2C_MASTER_NUM, I2C_BMS_ADDRESS);

    while(42)
    {
        if (lvgl_port_lock(0))
        {
            uint16_t register_value;
            uint8_t length;
            uint8_t data_buffer[24];
            char buffer[24];

            smbus_read_word(&smbus_info, 0x0D, &register_value);
            snprintf(buffer, sizeof(buffer), "%d %%", register_value);
            lv_label_set_text(lbl_state_of_charge, buffer);

            smbus_read_word(&smbus_info, 0x09, &register_value);
            snprintf(buffer, sizeof(buffer), "%0.1f V", (float) register_value / 1000);
            lv_label_set_text(lbl_voltage, buffer);

            smbus_read_word(&smbus_info, 0x0A, &register_value);
            snprintf(buffer, sizeof(buffer), "%d mA", register_value);
            lv_label_set_text(lbl_current, buffer);

            smbus_read_word(&smbus_info, 0x46, &register_value);
            snprintf(buffer, sizeof(buffer), "%s", register_value & 0x06 ? "Pass" : "Fail");
            lv_label_set_text(lbl_status, buffer);

            length = sizeof(data_buffer);
            smbus_read_block(&smbus_info, 0x23, data_buffer, &length);
            snprintf(buffer, sizeof(buffer), "%d", (data_buffer[6] << 8) | data_buffer[7]);
            lv_label_set_text(lbl_version, buffer);

            lvgl_port_unlock();
        }

        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    vTaskDelay(portMAX_DELAY);
}
