idf_component_register(SRCS main.c lcd.c touch.c
                       INCLUDE_DIRS "."
                       REQUIRES esp_lcd driver esp32-smbus bt nvs_flash
                      )
