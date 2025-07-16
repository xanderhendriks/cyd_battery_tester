#include "battery.h"

#include <string.h>

#include <esp_check.h>

#include "driver/i2c_master.h"
#include "smbus.h"

#define I2C_MASTER_SCL_IO    22    // GPIO number for I2C master clock
#define I2C_MASTER_SDA_IO    27    // GPIO number for I2C master data
#define I2C_MASTER_NUM       I2C_NUM_0  // I2C port number for master dev
#define I2C_MASTER_FREQ_HZ   100000     // I2C master clock frequency
#define I2C_BMS_ADDRESS (0x0B)

static smbus_info_t smbus_info;
static const char *TAG="battery";

esp_err_t battery_init(void)
{
    ESP_LOGI(TAG, "Battery initialization");

    /* ---------- allocate the bus ---------- */
    i2c_master_bus_handle_t bus;
    i2c_master_bus_config_t bus_cfg = {
        .clk_source             = I2C_CLK_SRC_DEFAULT,
        .i2c_port               = I2C_MASTER_NUM,
        .scl_io_num             = I2C_MASTER_SCL_IO,
        .sda_io_num             = I2C_MASTER_SDA_IO,
        .glitch_ignore_cnt      = 7,                /* ≤9 is fine for 100 kHz */
        .flags = {
            .enable_internal_pullup = true
        }
    };
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_cfg, &bus), TAG, "bus init failed");

    /* ---------- add the battery-gauge device ---------- */
    i2c_master_dev_handle_t dev;
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = I2C_BMS_ADDRESS,
        .scl_speed_hz    = I2C_MASTER_FREQ_HZ
    };
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(bus, &dev_cfg, &dev), TAG, "add dev failed");

    /* ---------- hand device handle to SMBus wrapper ---------- */
    smbus_init_device_handle(&smbus_info, dev);     /* helper you add in smbus.c */

    return ESP_OK;
}

esp_err_t battery_reset(void)
{
    return ESP_OK;
}

esp_err_t battery_detect_type(battery_type_t *type)
{
    const uint8_t name_bufer_size = 10;
    char data_buffer[name_bufer_size];
    size_t length = name_bufer_size;
    esp_err_t error;

    error = smbus_read_block(&smbus_info, 0x21, (uint8_t*) data_buffer, (uint8_t*) &length);
    data_buffer[name_bufer_size] = '\0';
    strupr(data_buffer);

    if (strstr(data_buffer, "BQ20Z95") != NULL)
    {
        *type = BATTERY_TYPE_BQ20Z95;
    }
    else if (strstr(data_buffer, "BQ40Z50") != NULL)
    {
        *type = BATTERY_TYPE_BQ40Z50;
    }
    else if (strstr(data_buffer, "RL1458C") != NULL)
    {
        *type = BATTERY_TYPE_BQ40Z50;
    }
    else
    {
        *type = BATTERY_TYPE_UNKNOWN;
    }

    return error;
}

esp_err_t battery_state_of_charge_get(uint16_t *register_value)
{
    return smbus_read_word(&smbus_info, 0x0D, register_value);
}

esp_err_t battery_voltage_get(uint16_t *register_value)
{
    return smbus_read_word(&smbus_info, 0x09, register_value);
}

esp_err_t battery_current_get(uint16_t *register_value)
{
    return smbus_read_word(&smbus_info, 0x0A, register_value);
}

esp_err_t battery_safety_status_get(uint32_t *register_value)
{
    esp_err_t error = ESP_FAIL;
    battery_type_t type = BATTERY_TYPE_UNKNOWN;
    uint8_t data_buffer[5] = {0};

    *register_value = 0;

    battery_detect_type(&type);

    if (type == BATTERY_TYPE_BQ20Z95)
    {
        smbus_read_word(&smbus_info, 0x50, (uint16_t*) register_value);
    }
    else if (type == BATTERY_TYPE_BQ40Z50)
    {
        error = smbus_i2c_read_block(&smbus_info, 0x50, data_buffer, 5);
        if (error == ESP_OK)
        {
            *register_value = (data_buffer[1] << 24) | (data_buffer[2] << 16) | (data_buffer[3] << 8) | data_buffer[4];
        }
    }

    return error;
}

esp_err_t battery_safety_alert_get(uint32_t *register_value)
{
    esp_err_t error = ESP_FAIL;
    battery_type_t type = BATTERY_TYPE_UNKNOWN;
    uint8_t data_buffer[5] = {0};

    *register_value = 0;

    battery_detect_type(&type);

    if (type == BATTERY_TYPE_BQ20Z95)
    {
        smbus_read_word(&smbus_info, 0x51, (uint16_t*) register_value);
    }
    else if (type == BATTERY_TYPE_BQ40Z50)
    {
        error = smbus_i2c_read_block(&smbus_info, 0x51, data_buffer, 5);
        if (error == ESP_OK)
        {
            *register_value = (data_buffer[1] << 24) | (data_buffer[2] << 16) | (data_buffer[3] << 8) | data_buffer[4];
        }
    }

    return error;
}

esp_err_t battery_status_get(uint16_t *register_value)
{
    return smbus_read_word(&smbus_info, 0x16, register_value);
}

esp_err_t battery_name_get(char *data_buffer, size_t *length)
{
    esp_err_t error = ESP_OK;
    const uint8_t name_bufer_size = 9;

    if (*length >= name_bufer_size)
    {
        error = smbus_read_block(&smbus_info, 0x21, (uint8_t*) data_buffer, (uint8_t*) length);
        data_buffer[name_bufer_size - 1] = '\0';
    }
    else
    {
        error = ESP_ERR_INVALID_SIZE;
        data_buffer[0] = '\0';
    }

    return error;
}

esp_err_t battery_version_get(uint16_t *register_value)
{
    esp_err_t error = ESP_FAIL;
    battery_type_t type = BATTERY_TYPE_UNKNOWN;
    uint8_t data_buffer[12] = {0};
    uint8_t length;

    *register_value = 0;

    battery_detect_type(&type);

    if (type == BATTERY_TYPE_BQ20Z95)
    {
        length = 8;
        error = smbus_read_block(&smbus_info, 0x23, data_buffer, &length);
        *register_value = (data_buffer[7] << 8) | data_buffer[6];

    }
    else if (type == BATTERY_TYPE_BQ40Z50)
    {
        char* pos;
        length = 15;
        error = smbus_i2c_read_block(&smbus_info, 0x21, data_buffer, length);
        data_buffer[length - 1] = '\0';

        pos = strstr((char*) data_buffer, "_v");
        if (pos != NULL)
        {
            sscanf(pos, "_v%u", (unsigned int*) register_value);
        }
    }

    return error;
}
