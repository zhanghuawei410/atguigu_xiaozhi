idf_component_register(SRCS "main.c" "./button/xiaozhi_button.c" "./internet/xiaozhi_wifi.c" "./display/xiaozhi_lcd.c" "./display/xiaozhi_display.c" "./internet/xiaozhi_active.c" "./audio/xiaozhi_hard_codec.c"
                        "./audio/xiaozhi_sr.c"
                    INCLUDE_DIRS "." "./button" "./internet" "./display" "./common" "./audio")