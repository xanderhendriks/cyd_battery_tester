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
#include "nvs_flash.h"

// #include "esp_nimble_hci.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

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
static uint16_t gatt_state_of_charge;
static lv_obj_t *lbl_voltage;
static uint16_t gatt_voltage;
static lv_obj_t *lbl_current;
static uint16_t gatt_current;
static lv_obj_t *lbl_status;
static lv_obj_t *lbl_version;
static uint16_t gatt_version;

static const ble_uuid128_t gatt_svr_svc_battery_uuid =
        BLE_UUID128_INIT(0x40, 0xb3, 0x20, 0x90, 0x72, 0xb5, 0x80, 0xaf,
                         0xa7, 0x4f, 0x15, 0x1c, 0xaf, 0xd2, 0x61, 0xe7);

#define STATE_OF_CHARGE_TYPE 0x8001
#define STATE_OF_CHARGE_VALUE 0x8002
#define STATE_OF_CHARGE_STRING "State of charge (%)"

#define VOLTAGE_TYPE 0x8003
#define VOLTAGE_VALUE 0x8004
#define VOLTAGE_STRING "Voltage (V)"

#define CURRENT_TYPE 0x8005
#define CURRENT_VALUE 0x8006
#define CURRENT_STRING "Current (mA)"

#define VERSION_TYPE 0x8009
#define VERSION_VALUE 0x800A
#define VERSION_STRING "Version"


uint8_t ble_addr_type;
void ble_app_advertise(void);

static int
gatt_svr_chr_write(struct os_mbuf *om, uint16_t min_len, uint16_t max_len,
                   void *dst, uint16_t *len)
{
    uint16_t om_len;
    int rc;

    om_len = OS_MBUF_PKTLEN(om);
    if (om_len < min_len || om_len > max_len) {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }

    rc = ble_hs_mbuf_to_flat(om, dst, max_len, len);
    if (rc != 0) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    return 0;
}

// Write data to ESP32 defined as server
static int device_write(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    // printf("Data from the client: %.*s\n", ctxt->om->om_len, ctxt->om->om_data);

    char * data = (char *)ctxt->om->om_data;
    printf("%d\n",strcmp(data, (char *)"LIGHT ON")==0);
    if (strcmp(data, (char *)"LIGHT ON\0")==0)
    {
       printf("LIGHT ON\n");
    }
    else if (strcmp(data, (char *)"LIGHT OFF\0")==0)
    {
        printf("LIGHT OFF\n");
    }
    else if (strcmp(data, (char *)"FAN ON\0")==0)
    {
        printf("FAN ON\n");
    }
    else if (strcmp(data, (char *)"FAN OFF\0")==0)
    {
        printf("FAN OFF\n");
    }
    else{
        printf("Data from the client: %.*s\n", ctxt->om->om_len, ctxt->om->om_data);
    }


    return 0;
}

// Read data from ESP32 defined as server
static int device_read(uint16_t con_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    os_mbuf_append(ctxt->om, "Data from the server", strlen("Data from the server"));
    return 0;
}

static int
gatt_svr_bat_access(uint16_t conn_handle, uint16_t attr_handle,
                          struct ble_gatt_access_ctxt *ctxt,
                          void *arg)
{
    uint16_t uuid16;
    int rc;

    uuid16 = ble_uuid_u16(ctxt->chr->uuid);

    switch (uuid16) {
    case STATE_OF_CHARGE_TYPE:
        assert(ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR);
        rc = os_mbuf_append(ctxt->om, STATE_OF_CHARGE_STRING, strlen(STATE_OF_CHARGE_STRING));
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;

    case STATE_OF_CHARGE_VALUE:
        if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
            rc = os_mbuf_append(ctxt->om, &gatt_state_of_charge,
                                sizeof(gatt_state_of_charge));
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        }

    case VOLTAGE_TYPE:
        assert(ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR);
        rc = os_mbuf_append(ctxt->om, VOLTAGE_STRING, strlen(VOLTAGE_STRING));
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;

    case VOLTAGE_VALUE:
        if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
            rc = os_mbuf_append(ctxt->om, &gatt_voltage,
                                sizeof(gatt_voltage));
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        }

    case CURRENT_TYPE:
        assert(ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR);
        rc = os_mbuf_append(ctxt->om, CURRENT_STRING, strlen(CURRENT_STRING));
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;

    case CURRENT_VALUE:
        if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
            rc = os_mbuf_append(ctxt->om, &gatt_current,
                                sizeof(gatt_current));
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        }

    case VERSION_TYPE:
        assert(ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR);
        rc = os_mbuf_append(ctxt->om, VERSION_STRING, strlen(VERSION_STRING));
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;

    case VERSION_VALUE:
        if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
            rc = os_mbuf_append(ctxt->om, &gatt_version,
                                sizeof(gatt_version));
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        }

    default:
        assert(0);
        return BLE_ATT_ERR_UNLIKELY;
    }
}
// Array of pointers to other service definitions
// UUID - Universal Unique Identifier
static const struct ble_gatt_svc_def gatt_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(0x180),                 // Define UUID for device type
        .characteristics = (struct ble_gatt_chr_def[]) {{
            .uuid = BLE_UUID16_DECLARE(0xFEF4),           // Define UUID for reading
            .flags = BLE_GATT_CHR_F_READ,
            .access_cb = device_read
        }, {
            .uuid = BLE_UUID16_DECLARE(0xDEAD),           // Define UUID for writing
            .flags = BLE_GATT_CHR_F_WRITE,
            .access_cb = device_write
        }, {
            0
        }},
    },
    {
        /*** ADC Level Notification Service. */
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &gatt_svr_svc_battery_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) {{
            .uuid = BLE_UUID16_DECLARE(STATE_OF_CHARGE_TYPE),
            .access_cb = gatt_svr_bat_access,
            .flags = BLE_GATT_CHR_F_READ,
        }, {
            .uuid = BLE_UUID16_DECLARE(STATE_OF_CHARGE_VALUE),
            .access_cb = gatt_svr_bat_access,
            .flags = BLE_GATT_CHR_F_NOTIFY,
        }, {
            .uuid = BLE_UUID16_DECLARE(VOLTAGE_TYPE),
            .access_cb = gatt_svr_bat_access,
            .flags = BLE_GATT_CHR_F_READ,
        }, {
            .uuid = BLE_UUID16_DECLARE(VOLTAGE_VALUE),
            .access_cb = gatt_svr_bat_access,
            .flags = BLE_GATT_CHR_F_NOTIFY,
        }, {
            .uuid = BLE_UUID16_DECLARE(CURRENT_TYPE),
            .access_cb = gatt_svr_bat_access,
            .flags = BLE_GATT_CHR_F_READ,
        }, {
            .uuid = BLE_UUID16_DECLARE(CURRENT_VALUE),
            .access_cb = gatt_svr_bat_access,
            .flags = BLE_GATT_CHR_F_NOTIFY,
        },{
            .uuid = BLE_UUID16_DECLARE(VERSION_TYPE),
            .access_cb = gatt_svr_bat_access,
            .flags = BLE_GATT_CHR_F_READ,
        }, {
            .uuid = BLE_UUID16_DECLARE(VERSION_VALUE),
            .access_cb = gatt_svr_bat_access,
            .flags = BLE_GATT_CHR_F_NOTIFY,
        }, {
            0, /* No more characteristics in this service. */
        }},
    },
    {0}};

// BLE event handling
static int ble_gap_event(struct ble_gap_event *event, void *arg)
{
    switch (event->type)
    {
    // Advertise if connected
    case BLE_GAP_EVENT_CONNECT:
        ESP_LOGI("GAP", "BLE GAP EVENT CONNECT %s", event->connect.status == 0 ? "OK!" : "FAILED!");
        if (event->connect.status != 0)
        {
            ble_app_advertise();
        }
        break;
    // Advertise again after completion of the event
    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI("GAP", "BLE GAP EVENT DISCONNECTED");
        break;
    case BLE_GAP_EVENT_ADV_COMPLETE:
        ESP_LOGI("GAP", "BLE GAP EVENT");
        ble_app_advertise();
        break;
    default:
        break;
    }
    return 0;
}

// Define the BLE connection
void ble_app_advertise(void)
{
    // GAP - device name definition
    struct ble_hs_adv_fields fields;
    const char *device_name;
    memset(&fields, 0, sizeof(fields));
    device_name = ble_svc_gap_device_name(); // Read the BLE device name
    fields.name = (uint8_t *)device_name;
    fields.name_len = strlen(device_name);
    fields.name_is_complete = 1;
    ble_gap_adv_set_fields(&fields);

    // GAP - device connectivity definition
    struct ble_gap_adv_params adv_params;
    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND; // connectable or non-connectable
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN; // discoverable or non-discoverable
    ble_gap_adv_start(ble_addr_type, NULL, BLE_HS_FOREVER, &adv_params, ble_gap_event, NULL);
}

// The application
void ble_app_on_sync(void)
{
    ble_hs_id_infer_auto(0, &ble_addr_type); // Determines the best address type automatically
    ble_app_advertise();                     // Define the BLE connection
}

// The infinite task
void host_task(void *param)
{
    nimble_port_run(); // This function will return only when nimble_port_stop() is executed
}

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

    nvs_flash_init();
    nimble_port_init();                                 // Initialize the host stack
    ble_svc_gap_device_name_set("cyd_battery_tester");  // Initialize NimBLE configuration - server name
    ble_svc_gap_init();                                 // Initialize NimBLE configuration - gap service
    ble_svc_gatt_init();                                // Initialize NimBLE configuration - gatt service
    ble_gatts_count_cfg(gatt_svcs);                     // Initialize NimBLE configuration - config gatt services
    ble_gatts_add_svcs(gatt_svcs);                      // Initialize NimBLE configuration - queues gatt services.
    ble_hs_cfg.sync_cb = ble_app_on_sync;               // Initialize application
    nimble_port_freertos_init(host_task);               // Run the thread

    gatt_state_of_charge = 0;

    while(42)
    {
        if (lvgl_port_lock(0))
        {
            uint16_t register_value;
            uint8_t length;
            uint8_t data_buffer[24];
            char buffer[24];
            uint16_t chr_val_handle;
            int rc;

            smbus_read_word(&smbus_info, 0x0D, &register_value);
            snprintf(buffer, sizeof(buffer), "%d %%", register_value);
            lv_label_set_text(lbl_state_of_charge, buffer);

            gatt_state_of_charge = register_value;
            rc = ble_gatts_find_chr(&gatt_svr_svc_battery_uuid.u, BLE_UUID16_DECLARE(STATE_OF_CHARGE_VALUE), NULL, &chr_val_handle);
            assert(rc == 0);
            ble_gatts_chr_updated(chr_val_handle);

            smbus_read_word(&smbus_info, 0x09, &register_value);
            snprintf(buffer, sizeof(buffer), "%0.1f V", (float) register_value / 1000);
            lv_label_set_text(lbl_voltage, buffer);

            gatt_voltage = register_value;
            rc = ble_gatts_find_chr(&gatt_svr_svc_battery_uuid.u, BLE_UUID16_DECLARE(VOLTAGE_VALUE), NULL, &chr_val_handle);
            assert(rc == 0);
            ble_gatts_chr_updated(chr_val_handle);

            smbus_read_word(&smbus_info, 0x0A, &register_value);
            snprintf(buffer, sizeof(buffer), "%hd mA", register_value);
            lv_label_set_text(lbl_current, buffer);

            gatt_current = register_value;
            rc = ble_gatts_find_chr(&gatt_svr_svc_battery_uuid.u, BLE_UUID16_DECLARE(CURRENT_VALUE), NULL, &chr_val_handle);
            assert(rc == 0);
            ble_gatts_chr_updated(chr_val_handle);

            smbus_read_word(&smbus_info, 0x46, &register_value);
            snprintf(buffer, sizeof(buffer), "%s", (register_value & 0x0006) == 0x0006 ? "Pass" : "Fail");
            lv_label_set_text(lbl_status, buffer);

            length = sizeof(data_buffer);
            smbus_read_block(&smbus_info, 0x23, data_buffer, &length);
            snprintf(buffer, sizeof(buffer), "%d", (data_buffer[6] << 8) | data_buffer[7]);
            lv_label_set_text(lbl_version, buffer);

            gatt_current = register_value;
            rc = ble_gatts_find_chr(&gatt_svr_svc_battery_uuid.u, BLE_UUID16_DECLARE(VERSION_VALUE), NULL, &chr_val_handle);
            assert(rc == 0);
            ble_gatts_chr_updated(chr_val_handle);

            lvgl_port_unlock();
        }

        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    vTaskDelay(portMAX_DELAY);
}
