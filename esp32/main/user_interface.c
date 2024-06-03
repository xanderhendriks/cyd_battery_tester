#include "user_interface.h"

#include <esp_check.h>
#include <esp_log.h>
#include <esp_lvgl_port.h>
#include <lvgl.h>

#include "lcd.h"
#include "touch.h"


static const char *TAG="ui";
static uint8_t ui_screen_page_index = 0;
static ui_screen_page_t *ui_screen_pages;
update_property_values_cb_t ui_update_property_values_cb;

static void page_container_create(lv_obj_t *scr, ui_screen_page_t *screen_page);

static void property_box_create(lv_obj_t *cont, ui_property_box_data_t *property_box_data);

static void page_container_create(lv_obj_t *scr, ui_screen_page_t *screen_page)
{
    static int32_t col_dsc[] = {UI_PROPERTY_BOX_WIDTH / 2, UI_PROPERTY_BOX_WIDTH / 2 - 1, LV_GRID_TEMPLATE_LAST};
    static int32_t row_dsc[] = {UI_PROPERTY_BOX_HEIGHT, UI_PROPERTY_BOX_HEIGHT, UI_PROPERTY_BOX_HEIGHT, UI_PROPERTY_BOX_HEIGHT - 1, LV_GRID_TEMPLATE_LAST};

    screen_page->cont = lv_obj_create(scr);
    lv_obj_set_grid_dsc_array(screen_page->cont, col_dsc, row_dsc);
    lv_obj_set_size(screen_page->cont, UI_PROPERTY_BOX_WIDTH, 4 * UI_PROPERTY_BOX_HEIGHT);
    lv_obj_align(screen_page->cont, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_border_width(screen_page->cont, 0, 0);
    lv_obj_set_style_pad_all(screen_page->cont, 0, 0);
    lv_obj_set_style_pad_row(screen_page->cont, 0, 0);
    lv_obj_set_style_pad_column(screen_page->cont, 0, 0);
}

static void property_box_create(lv_obj_t *cont, ui_property_box_data_t *property_box_data)
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


esp_err_t ui_init(ui_screen_page_t *screen_pages, update_property_values_cb_t update_property_values_cb, button_press_cb_t button_press_cb)
{
    lv_obj_t *scr;
    uint8_t page_index = 0;
    uint8_t box_index = 0;
    esp_lcd_panel_io_handle_t lcd_io;
    esp_lcd_panel_handle_t lcd_panel;
    esp_lcd_touch_handle_t tp;
    lvgl_port_touch_cfg_t touch_cfg;
    lv_display_t *lvgl_display = NULL;

    ui_update_property_values_cb = update_property_values_cb;
    ui_screen_pages = screen_pages;

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

    scr = lv_scr_act();

    lvgl_port_lock(0);

    while (ui_screen_pages[page_index].property_boxes[0].row != UI_END_OF_LIST_MARKER)
    {
        page_container_create(scr, &ui_screen_pages[page_index]);
        if (page_index != ui_screen_page_index)
        {
            lv_obj_add_flag(ui_screen_pages[page_index].cont, LV_OBJ_FLAG_HIDDEN);
        }

        box_index = 0;
        while (ui_screen_pages[page_index].property_boxes[box_index].row != UI_END_OF_LIST_MARKER)
        {
            property_box_create(ui_screen_pages[page_index].cont, &ui_screen_pages[page_index].property_boxes[box_index]);
            box_index++;
        }
        page_index++;
    }

    lv_obj_t *btn_page = lv_button_create(scr);
    lv_obj_align(btn_page, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_size(btn_page, 119, 62);
    lv_obj_set_style_bg_color(btn_page, lv_color_make(0xe7, 0x7b, 0x0f), 0);
    lv_obj_add_event_cb(btn_page, button_press_cb, LV_EVENT_ALL, btn_page);

    lv_obj_t *lbl_page = lv_label_create(btn_page);
    lv_label_set_text(lbl_page, "Next\npage");
    lv_obj_set_style_text_align(lbl_page, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(lbl_page, lv_color_white(), 0);
    lv_obj_align(lbl_page, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *btn_reset = lv_button_create(scr);
    lv_obj_align(btn_reset, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_set_size(btn_reset, 119, 62);
    lv_obj_set_style_bg_color(btn_reset, lv_color_make(0xe7, 0x7b, 0x0f), 0);
    lv_obj_add_event_cb(btn_reset, button_press_cb, LV_EVENT_ALL, btn_reset);

    lv_obj_t *lbl_reset = lv_label_create(btn_reset);
    lv_label_set_text(lbl_reset, "Reset\nbattery");
    lv_obj_set_style_text_align(lbl_reset, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(lbl_reset, lv_color_white(), 0);
    lv_obj_align(lbl_reset, LV_ALIGN_CENTER, 0, 0);

    lvgl_port_unlock();

    return ESP_OK;
}


void ui_next_page(void)
{
    lv_obj_add_flag(ui_screen_pages[ui_screen_page_index].cont, LV_OBJ_FLAG_HIDDEN);

    ui_screen_page_index++;
    if (ui_screen_pages[ui_screen_page_index].property_boxes[0].row == UI_END_OF_LIST_MARKER)
    {
        ui_screen_page_index = 0;
    }
    lv_obj_remove_flag(ui_screen_pages[ui_screen_page_index].cont, LV_OBJ_FLAG_HIDDEN);
}

esp_err_t ui_update_property_values(void)
{
    uint8_t box_index = 0;

    while (ui_screen_pages[ui_screen_page_index].property_boxes[box_index].row != UI_END_OF_LIST_MARKER)
    {
        if (ui_update_property_values_cb(&ui_screen_pages[ui_screen_page_index].property_boxes[box_index]))
        {
            if (lvgl_port_lock(0))
            {
                lv_label_set_text_static(ui_screen_pages[ui_screen_page_index].property_boxes[box_index].lbl_value, ui_screen_pages[ui_screen_page_index].property_boxes[box_index].value);

                lvgl_port_unlock();
            }
        }

        box_index++;

        vTaskDelay(100 / portTICK_PERIOD_MS);
    }

    return ESP_OK;
}
