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

#define PROPERTY_BOX_HEIGHT 64
#define PROPERTY_BOX_WIDTH 240
#define END_OF_LIST_MARKER 0xFF

#define I2C_MASTER_SCL_IO    22    // GPIO number for I2C master clock
#define I2C_MASTER_SDA_IO    27    // GPIO number for I2C master data
#define I2C_MASTER_NUM       I2C_NUM_0  // I2C port number for master dev
#define I2C_MASTER_FREQ_HZ   100000     // I2C master clock frequency
#define I2C_MASTER_TX_BUF_DISABLE   0   // I2C master doesn't need buffer
#define I2C_MASTER_RX_BUF_DISABLE   0   // I2C master doesn't need buffer
#define I2C_MASTER_TIMEOUT_MS 1000
#define I2C_BMS_ADDRESS (0x0B)

static const char *TAG="demo";


typedef void (*GetValue)(char* value);

typedef enum
{
    POSITION_LEFT = 0,
    POSITION_RIGHT,
    POSITION_FULL
} position_t;

typedef enum
{
    PROPERTY_ID_STATE_OF_CHARGE = 0,
    PROPERTY_ID_VOLTAGE,
    PROPERTY_ID_CURRENT,
    PROPERTY_ID_STATUS,
    PROPERTY_ID_VERSION,
    PROPERTY_ID_HEALTH,
    PROPERTY_ID_FW_VERSION,
} property_id_t;


typedef struct
{
    uint8_t row;
    char description[64];
    position_t position;
    property_id_t property_id;
    char empty_value[16];
    char value[64];
    lv_obj_t *cont;
    lv_obj_t *lbl_description;
    lv_obj_t *lbl_value;
} property_box_data_t;


typedef struct
{
    lv_obj_t *cont;
    property_box_data_t property_boxes[8];
} screen_page_t;

uint8_t screen_page_index = 0;
screen_page_t screen_pages[] = {
    {
        .property_boxes = {
            {0, "State of charge", POSITION_FULL, PROPERTY_ID_STATE_OF_CHARGE, "-- %"},
            {1, "Voltage", POSITION_FULL, PROPERTY_ID_VOLTAGE, "-.- V"},
            {2, "Current", POSITION_FULL, PROPERTY_ID_CURRENT, "-.- mA"},
            {3, "Health", POSITION_LEFT, PROPERTY_ID_HEALTH, "-"},
            {3, "Status", POSITION_RIGHT, PROPERTY_ID_STATUS, "-"},
            {END_OF_LIST_MARKER}
        }
    },
    {
        .property_boxes = {
            {0, "Version", POSITION_FULL, PROPERTY_ID_VERSION, "-"},
            {1, "FW Version", POSITION_FULL, PROPERTY_ID_FW_VERSION, "-"},
            {END_OF_LIST_MARKER}
        }
    },
    {
        .property_boxes = {
            {END_OF_LIST_MARKER}
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


void ui_event_Screen(lv_event_t *e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t *btn = (lv_obj_t *)lv_event_get_user_data(e);

    btn = btn;

    if (event_code == LV_EVENT_CLICKED)
    {
        lv_obj_add_flag(screen_pages[screen_page_index].cont, LV_OBJ_FLAG_HIDDEN);

        screen_page_index++;
        if (screen_pages[screen_page_index].property_boxes[0].row == END_OF_LIST_MARKER)
        {
            screen_page_index = 0;
        }
        lv_obj_remove_flag(screen_pages[screen_page_index].cont, LV_OBJ_FLAG_HIDDEN);

        ESP_LOGI(TAG, "button pressed: %d", screen_page_index);
    }
}

void page_container_create(lv_obj_t *scr, screen_page_t *screen_page)
{
    static int32_t col_dsc[] = {PROPERTY_BOX_WIDTH / 2, PROPERTY_BOX_WIDTH / 2 - 1, LV_GRID_TEMPLATE_LAST};
    static int32_t row_dsc[] = {PROPERTY_BOX_HEIGHT, PROPERTY_BOX_HEIGHT, PROPERTY_BOX_HEIGHT, PROPERTY_BOX_HEIGHT - 1, LV_GRID_TEMPLATE_LAST};

    screen_page->cont = lv_obj_create(scr);
    lv_obj_set_grid_dsc_array(screen_page->cont, col_dsc, row_dsc);
    lv_obj_set_size(screen_page->cont, PROPERTY_BOX_WIDTH, 4 * PROPERTY_BOX_HEIGHT);
    lv_obj_align(screen_page->cont, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_border_width(screen_page->cont, 0, 0);
    lv_obj_set_style_pad_all(screen_page->cont, 0, 0);
    lv_obj_set_style_pad_row(screen_page->cont, 0, 0);
    lv_obj_set_style_pad_column(screen_page->cont, 0, 0);
}

void property_box_create(lv_obj_t *cont, property_box_data_t *property_box_data)
{
    int32_t col_pos;
    int32_t col_span;

    switch(property_box_data->position)
    {
        case POSITION_LEFT:
            col_span = 1;
            col_pos = 0;
            break;

        case POSITION_RIGHT:
            col_span = 1;
            col_pos = 1;
            break;

        default:
        case POSITION_FULL:
            col_span = 2;
            col_pos = 0;
            break;
    };

    property_box_data->cont = lv_obj_create(cont);
    lv_obj_set_size(property_box_data->cont, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_grid_cell(property_box_data->cont, LV_GRID_ALIGN_STRETCH, col_pos, col_span, LV_GRID_ALIGN_STRETCH, property_box_data->row, 1);
    lv_obj_set_style_bg_color(property_box_data->cont, lv_color_hex(0x15171A), 0);
    lv_obj_set_style_border_width(property_box_data->cont, 2, 0);
    lv_obj_set_style_border_color(property_box_data->cont, lv_color_white(), 0);
    lv_obj_set_style_pad_all(property_box_data->cont, 10, 0);

    property_box_data->lbl_description = lv_label_create(property_box_data->cont);
    lv_label_set_text_static(property_box_data->lbl_description, property_box_data->description);
    lv_obj_set_style_text_color(property_box_data->lbl_description, lv_color_white(), 0);
    lv_obj_align(property_box_data->lbl_description, LV_ALIGN_TOP_MID, 0, 0);

    property_box_data->lbl_value = lv_label_create(property_box_data->cont);
    lv_label_set_text_static(property_box_data->lbl_value, property_box_data->value);
    lv_obj_set_style_text_color(property_box_data->lbl_value, lv_color_white(), 0);
    lv_obj_align(property_box_data->lbl_value, LV_ALIGN_BOTTOM_MID, 0, 0);
}


static esp_err_t app_lvgl_main()
{
    lv_obj_t *scr = lv_scr_act();
    uint8_t page_index = 0;
    uint8_t box_index = 0;

    lvgl_port_lock(0);

    while (screen_pages[page_index].property_boxes[0].row != END_OF_LIST_MARKER)
    {
        page_container_create(scr, &screen_pages[page_index]);
        if (page_index != screen_page_index)
        {
            lv_obj_add_flag(screen_pages[page_index].cont, LV_OBJ_FLAG_HIDDEN);
        }

        box_index = 0;
        while (screen_pages[page_index].property_boxes[box_index].row != END_OF_LIST_MARKER)
        {
            property_box_create(screen_pages[page_index].cont, &screen_pages[page_index].property_boxes[box_index]);
            box_index++;
        }
        page_index++;
    }

    lv_obj_t *btn_page = lv_button_create(scr);
    lv_obj_align(btn_page, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_size(btn_page, 119, 62);
    lv_obj_set_style_bg_color(btn_page, lv_color_make(0xe7, 0x7b, 0x0f), 0);
    lv_obj_add_event_cb(btn_page, ui_event_Screen, LV_EVENT_ALL, btn_page);

    lv_obj_t *lbl_page = lv_label_create(btn_page);
    lv_label_set_text(lbl_page, "Next\npage");
    lv_obj_set_style_text_align(lbl_page, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(lbl_page, lv_color_white(), 0);
    lv_obj_align(lbl_page, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *btn_reset = lv_button_create(scr);
    lv_obj_align(btn_reset, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_set_size(btn_reset, 119, 62);
    lv_obj_set_style_bg_color(btn_reset, lv_color_make(0xe7, 0x7b, 0x0f), 0);
    lv_obj_add_event_cb(btn_reset, ui_event_Screen, LV_EVENT_ALL, btn_reset);

    lv_obj_t *lbl_reset = lv_label_create(btn_reset);
    lv_label_set_text(lbl_reset, "Reset\nbattery");
    lv_obj_set_style_text_align(lbl_reset, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(lbl_reset, lv_color_white(), 0);
    lv_obj_align(lbl_reset, LV_ALIGN_CENTER, 0, 0);

    lvgl_port_unlock();

    return ESP_OK;
}

static esp_err_t update_property_values(smbus_info_t* smbus_info, uint8_t page_index)
{
    uint8_t box_index = 0;

    while (screen_pages[page_index].property_boxes[box_index].row != END_OF_LIST_MARKER)
    {
        uint16_t register_value;
        uint8_t length;
        uint8_t data_buffer[24];
        char value_buffer[64];
        esp_err_t error;

        switch(screen_pages[page_index].property_boxes[box_index].property_id)
        {
            case PROPERTY_ID_STATE_OF_CHARGE:
                error = smbus_read_word(smbus_info, 0x0D, &register_value);
                snprintf(value_buffer, sizeof(value_buffer), "%d %% : %d", register_value, error);
                break;

            case PROPERTY_ID_VOLTAGE:
                error = smbus_read_word(smbus_info, 0x09, &register_value);
                snprintf(value_buffer, sizeof(value_buffer), "%0.1f V : %d", (float) register_value / 1000, error);
                break;

            case PROPERTY_ID_CURRENT:
                error = smbus_read_word(smbus_info, 0x0A, &register_value);
                snprintf(value_buffer, sizeof(value_buffer), "%hd mA : %d", register_value, error);
                break;

            case PROPERTY_ID_STATUS:
                error = smbus_read_word(smbus_info, 0x46, &register_value);
                snprintf(value_buffer, sizeof(value_buffer), "%s : %d", (register_value & 0x0006) == 0x0006 ? "Pass" : "Fail", error);
                break;

            case PROPERTY_ID_VERSION:
                length = sizeof(data_buffer);
                error = smbus_read_block(smbus_info, 0x21, data_buffer, &length);
                snprintf(value_buffer, sizeof(value_buffer), "%*s : %d", (int) length, data_buffer, error);
                break;

            case PROPERTY_ID_HEALTH:
                error = smbus_read_word(smbus_info, 0x0D, &register_value);
                snprintf(value_buffer, sizeof(value_buffer), "%d %% : %d", register_value, error);
                break;

            case PROPERTY_ID_FW_VERSION:
                snprintf(value_buffer, sizeof(value_buffer), "%s", esp_app_get_description()->version);
                break;

            default:
                break;
        }

        if (strcmp(value_buffer, screen_pages[page_index].property_boxes[box_index].value) != 0)
        {
            strcpy(screen_pages[page_index].property_boxes[box_index].value, value_buffer);

            if (lvgl_port_lock(0))
            {
                lv_label_set_text_static(screen_pages[page_index].property_boxes[box_index].lbl_value, screen_pages[page_index].property_boxes[box_index].value);

                lvgl_port_unlock();
            }
        }

        box_index++;

        vTaskDelay(10 / portTICK_PERIOD_MS);
    }

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

    ESP_ERROR_CHECK(app_touch_init(&tp));
    touch_cfg.disp = lvgl_display;
    touch_cfg.handle = tp;
    lvgl_port_add_touch(&touch_cfg);

    ESP_ERROR_CHECK(lcd_display_brightness_set(75));
    ESP_ERROR_CHECK(lcd_display_rotate(lvgl_display, LV_DISPLAY_ROTATION_180));
    ESP_ERROR_CHECK(app_lvgl_main());

    ESP_ERROR_CHECK(i2c_master_init());
    smbus_info_t smbus_info;
    smbus_init(&smbus_info, I2C_MASTER_NUM, I2C_BMS_ADDRESS);

    for (;;)
    {
        update_property_values(&smbus_info, screen_page_index);
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}
