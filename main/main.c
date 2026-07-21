#include <stdio.h>
#include <string.h>
#include "esp_camera.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "uart_link.h"
#include "rover_protocol.h"
#include "web_server.h"

// Definición de comandos de la UI
typedef enum { CMD_STOP = 0, CMD_FWD, CMD_REV, CMD_LEFT, CMD_RIGHT, CMD_ARM } rover_cmd_t;

QueueHandle_t cmd_queue;

// Pines para la cámara OV2640 (Modelo AI-Thinker ESP32-CAM)
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

static void init_camera(void) {
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sccb_sda = SIOD_GPIO_NUM;
    config.pin_sccb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.frame_size = FRAMESIZE_QVGA; // 320x240 (Baja latencia)
    config.pixel_format = PIXFORMAT_JPEG; // Stream JPEG directo
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
    config.fb_location = CAMERA_FB_IN_DRAM; // RAM interna
    config.jpeg_quality = 12; // 0-63 (menor es mejor calidad)
    config.fb_count = 1;

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        // En producción podríamos encender un LED de error
        return;
    }
}


static void wifi_init_softap(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = "ROVER_WIFI",
            .ssid_len = strlen("ROVER_WIFI"),
            .channel = 1,
            .password = "",
            .max_connection = 4,
            .authmode = WIFI_AUTH_OPEN,
            .pmf_cfg = {
                .required = false,
            },
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    // Forzar antena al máximo para evitar cortes de telemetría y video
    esp_wifi_set_max_tx_power(78);
}

static void rover_control_task(void *arg) {
    rover_cmd_t current_cmd = CMD_STOP;
    uint8_t frame_buf[32];
    int frame_len;
    uint32_t last_web_cmd_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    while(1) {
        rover_cmd_t new_cmd;
        // Esperamos un comando nuevo hasta 100ms
        if (xQueueReceive(cmd_queue, &new_cmd, pdMS_TO_TICKS(100))) {
            last_web_cmd_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
            if (new_cmd == CMD_ARM) {
                frame_len = simpleprotocol_encode_tc_arm(frame_buf, true);
                uart_link_send(frame_buf, frame_len);
                current_cmd = CMD_STOP;
            } else {
                current_cmd = new_cmd;
            }
        } else {
            // Failsafe de WiFi: Si pasaron > 500ms sin recibir NINGUN comando de la web, cortamos motores.
            uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
            if ((now - last_web_cmd_ms) > 500) {
                current_cmd = CMD_STOP;
            }
        }
        
        // Mapear comandos de dirección a valores L/R del rover
        int16_t left = 0;
        int16_t right = 0;
        switch(current_cmd) {
            case CMD_FWD: left = 1000; right = 1000; break;
            case CMD_REV: left = -1000; right = -1000; break;
            // Debido a cómo están conectados los cables en el chasis (motores invertidos),
            // intercambiamos el giro para la izquierda y la derecha:
            case CMD_LEFT: left = 1000; right = -1000; break;
            case CMD_RIGHT: left = -1000; right = 1000; break;
            case CMD_STOP:
            default: left = 0; right = 0; break;
        }
        
        // Enviar trama de throttle a la placa Blackpill
        frame_len = simpleprotocol_encode_tc_throttle(frame_buf, left, right);
        uart_link_send(frame_buf, frame_len);
    }
}

void app_main(void) {
    // Inicializar NVS (requerido para Wi-Fi)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Crear cola para pasar comandos del hilo Web al hilo de Control (10 elementos)
    cmd_queue = xQueueCreate(10, sizeof(rover_cmd_t));

    // Inicializar periféricos
    uart_link_init();
    wifi_init_softap();
    init_camera();
    start_webserver();

    // Arrancar la tarea principal de control, enviando tramas periódicas al STM32
    xTaskCreate(rover_control_task, "rover_ctrl", 4096, NULL, 5, NULL);
}
