#ifndef USER_INTERFACE_H
#define USER_INTERFACE_H

#include <esp_err.h>
#include <lvgl.h>

#define UI_PROPERTY_BOX_HEIGHT 64
#define UI_PROPERTY_BOX_WIDTH 240
#define UI_END_OF_LIST_MARKER 0xFF

typedef enum
{
    PROPERTY_ID_STATE_OF_CHARGE = 0,
    PROPERTY_ID_VOLTAGE,
    PROPERTY_ID_CURRENT,
    PROPERTY_ID_SAFETY_STATUS,
    PROPERTY_ID_SAFETY_ALERT,
    PROPERTY_ID_STATUS,
    PROPERTY_ID_VERSION,
    PROPERTY_ID_OPERATION_STATUS,
    PROPERTY_ID_HEALTH,
    PROPERTY_ID_CYCLE_COUNT,
    PROPERTY_ID_NAME,
    PROPERTY_ID_FW_VERSION,
} ui_property_id_t;

typedef enum
{
    POSITION_LEFT = 0,
    POSITION_RIGHT,
    POSITION_FULL
} ui_position_t;

typedef enum
{
    UI_BTN_ID_NEXT_PAGE = 0,
    UI_BTN_ID_RESET_BATTERY,
} ui_btn_id_t;

typedef struct
{
    uint8_t row;
    char description[64];
    ui_position_t position;
    ui_property_id_t property_id;
    char empty_value[16];
    char value[64];
    lv_obj_t *cont;
    lv_obj_t *lbl_description;
    lv_obj_t *lbl_value;
} ui_property_box_data_t;


typedef struct
{
    lv_obj_t *cont;
    ui_property_box_data_t property_boxes[8];
} ui_screen_page_t;

typedef bool (*update_property_values_cb_t)(ui_property_box_data_t* property_box_data);

typedef void (*button_press_cb_t)(lv_event_t *event);

esp_err_t ui_init(ui_screen_page_t *screen_pages, update_property_values_cb_t update_property_values_cb, button_press_cb_t button_press_cb);

void ui_next_page(void);

esp_err_t ui_update(void);

#endif