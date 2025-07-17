#ifndef BATTERY_H
#define BATTERY_H

#include <esp_log.h>
#include <esp_err.h>

typedef enum {
    BATTERY_TYPE_UNKNOWN = 0,
    BATTERY_TYPE_BQ20Z95,
    BATTERY_TYPE_BQ40Z50
} battery_type_t;


esp_err_t battery_init(void);
esp_err_t battery_reset(void);

esp_err_t battery_detect_type(battery_type_t *type);
battery_type_t battery_get_last_detected_type(void);


esp_err_t battery_state_of_charge_get(uint16_t *register_value);
esp_err_t battery_voltage_get(uint16_t *register_value);
esp_err_t battery_current_get(uint16_t *register_value);
esp_err_t battery_safety_status_get(uint32_t *register_value);
esp_err_t battery_safety_alert_get(uint32_t *register_value);
esp_err_t battery_status_get(uint16_t *register_value);
esp_err_t battery_name_get(char *data_buffer, size_t *length);
esp_err_t battery_version_get(uint16_t *register_value);
esp_err_t battery_operation_status_get(uint32_t *register_value);
esp_err_t battery_state_of_health(uint16_t *register_value);
esp_err_t battery_cycle_count(uint16_t *register_value);

#endif