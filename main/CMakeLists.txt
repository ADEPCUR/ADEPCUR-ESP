idf_component_register(SRCS "main.c" "web_server.c" "uart_link.c" "rover_protocol.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp_wifi esp_http_server nvs_flash esp_event driver esp32-camera)
