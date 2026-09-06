idf_component_register(SRCS "password_manager.c" "app_state.c" "http_server.c" "wifi_manager.c" "button.c" "led.c" "main.c" "display/lcd_driver.c" "display/touch_driver.c" "display/ui_screens.c" "face_recognition.cpp" "i2c_bus.c" "bme280.c" "sensor_task.c" "password_manager.c" "display/keypad_screen.c"
                    INCLUDE_DIRS "." "display"
                    EMBED_TXTFILES "certs/servercert.pem" "certs/prvtkey.pem")
