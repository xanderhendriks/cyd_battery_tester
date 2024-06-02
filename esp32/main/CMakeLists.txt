idf_component_register(SRCS main.c lcd.c touch.c home_circle.c file_arrow_left_right.c lock_remove.c
                       INCLUDE_DIRS "."
                       REQUIRES esp_lcd driver esp32-smbus esp_app_format
                      )

add_definitions(-DLV_LVGL_H_INCLUDE_SIMPLE -DCONFIG_LV_USE_LOG -DCONFIG_LV_LOG_LEVEL=LV_LOG_LEVEL_TRACE)
