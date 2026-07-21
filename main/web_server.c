#include "web_server.h"
#include <esp_http_server.h>
#include <string.h>
#include "esp_camera.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

// Cola global definida en main.c
extern QueueHandle_t cmd_queue;

typedef enum { CMD_STOP = 0, CMD_FWD, CMD_REV, CMD_LEFT, CMD_RIGHT, CMD_ARM } rover_cmd_t;

// HTML ultra moderno (estilo premium) y optimizado para móvil
static const char* HTML_CONTENT = 
"<!DOCTYPE html><html lang='es'><head><meta charset='utf-8'>"
"<meta name='viewport' content='width=device-width, initial-scale=1, maximum-scale=1, user-scalable=0'>"
"<title>Rover Control</title>"
"<style>"
"body { background: #0f172a; color: #f8fafc; font-family: 'Inter', sans-serif; text-align: center; touch-action: none; margin: 0; padding: 20px; }"
"h1 { margin-top: 10px; font-weight: 600; letter-spacing: 1px; color: #38bdf8; }"
".container { max-width: 400px; margin: 0 auto; }"
".stream { width: 100%; border-radius: 12px; margin-bottom: 20px; box-shadow: 0 8px 30px rgba(0,0,0,0.5); min-height: 200px; background: #1e293b; }"
".arm-btn { width: 100%; max-width: 200px; background: #e11d48; border: none; border-radius: 12px; color: #fff; font-size: 20px; font-weight: bold; padding: 20px; box-shadow: 0 4px 15px rgba(225, 29, 72, 0.4); margin-bottom: 40px; transition: all 0.2s; }"
".arm-btn:active { background: #be123c; transform: scale(0.95); }"
".grid { display: grid; grid-template-columns: 1fr 1fr 1fr; gap: 15px; align-items: center; justify-items: center; }"
".dir-btn { width: 80px; height: 80px; background: rgba(56, 189, 248, 0.1); border: 2px solid #38bdf8; border-radius: 20px; color: #38bdf8; font-size: 32px; display: flex; align-items: center; justify-content: center; box-shadow: 0 4px 15px rgba(56, 189, 248, 0.15); transition: all 0.2s; user-select: none; }"
".dir-btn:active { background: #38bdf8; color: #0f172a; transform: scale(0.90); box-shadow: 0 0 20px rgba(56, 189, 248, 0.5); }"
".empty { visibility: hidden; }"
"#status { display: inline-block; width: 12px; height: 12px; border-radius: 50%; background: #e11d48; margin-left: 10px; }"
".connected { background: #10b981 !important; box-shadow: 0 0 10px #10b981; }"
"</style>"
"<script>"
"let ws = null; let intv = null;"
"function connect() {"
"  ws = new WebSocket('ws://' + window.location.host + '/ws');"
"  ws.onopen = () => document.getElementById('status').classList.add('connected');"
"  ws.onclose = () => { document.getElementById('status').classList.remove('connected'); setTimeout(connect, 1000); };"
"}"
"function snd(dir) { if(ws && ws.readyState === WebSocket.OPEN) ws.send(dir); }"
"function setupBtn(id, dir) {"
"  let b = document.getElementById(id);"
"  let start = function(e) { e.preventDefault(); snd(dir); intv = setInterval(()=>snd(dir), 100); };" // 100ms vía WS es baratísimo
"  let end = function(e) { e.preventDefault(); clearInterval(intv); snd('stop'); };"
"  b.addEventListener('mousedown', start); b.addEventListener('mouseup', end);"
"  b.addEventListener('touchstart', start); b.addEventListener('touchend', end);"
"}"
"window.onload = function() {"
"  connect();"
"  setupBtn('up', 'fwd'); setupBtn('dn', 'rev');"
"  setupBtn('lt', 'left'); setupBtn('rt', 'right');"
"  document.getElementById('arm').onclick = function() { snd('arm'); };"
"};"
"</script></head><body>"
"<div class='container'>"
"<h1>ROVER <span id='status'></span></h1>"
"<img src='http://192.168.4.1:81/stream' class='stream' />"
"<button id='arm' class='arm-btn'>ARM MOTORS</button>"
"<div class='grid'>"
"<div class='empty'></div><div id='up' class='dir-btn'>&#8593;</div><div class='empty'></div>"
"<div id='lt' class='dir-btn'>&#8592;</div><div id='dn' class='dir-btn'>&#8595;</div><div id='rt' class='dir-btn'>&#8594;</div>"
"</div>"
"</div>"
"</body></html>";

static esp_err_t root_get_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, HTML_CONTENT, HTTPD_RESP_USE_STRLEN);
}

#define PART_BOUNDARY "123456789000000000000987654321"
static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

static esp_err_t stream_handler(httpd_req_t *req) {
    camera_fb_t * fb = NULL;
    esp_err_t res = ESP_OK;
    size_t _jpg_buf_len = 0;
    uint8_t * _jpg_buf = NULL;
    char part_buf[64];

    res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
    if(res != ESP_OK) return res;

    while(true){
        fb = esp_camera_fb_get();
        if (!fb) {
            res = ESP_FAIL;
            break;
        }
        _jpg_buf_len = fb->len;
        _jpg_buf = fb->buf;

        if(res == ESP_OK){
            size_t hlen = snprintf(part_buf, 64, _STREAM_PART, _jpg_buf_len);
            res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
        }
        if(res == ESP_OK){
            res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
        }
        if(res == ESP_OK){
            res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
        }
        esp_camera_fb_return(fb);
        if(res != ESP_OK) break;
    }
    return res;
}

static esp_err_t ws_handler(httpd_req_t *req) {
    if (req->method == HTTP_GET) {
        // Handshake inicial
        return ESP_OK;
    }
    
    httpd_ws_frame_t ws_pkt;
    uint8_t buf[128] = { 0 };
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = buf;
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 127);
    if (ret != ESP_OK) {
        return ret;
    }
    
    if (ws_pkt.type == HTTPD_WS_TYPE_TEXT && ws_pkt.len > 0) {
        rover_cmd_t c = CMD_STOP;
        if (strncmp((char*)ws_pkt.payload, "fwd", ws_pkt.len) == 0) c = CMD_FWD;
        else if (strncmp((char*)ws_pkt.payload, "rev", ws_pkt.len) == 0) c = CMD_REV;
        else if (strncmp((char*)ws_pkt.payload, "left", ws_pkt.len) == 0) c = CMD_LEFT;
        else if (strncmp((char*)ws_pkt.payload, "right", ws_pkt.len) == 0) c = CMD_RIGHT;
        else if (strncmp((char*)ws_pkt.payload, "arm", ws_pkt.len) == 0) c = CMD_ARM;
        
        xQueueSend(cmd_queue, &c, 0);
    }
    return ESP_OK;
}

static esp_err_t stream_redirect_handler(httpd_req_t *req) {
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "http://192.168.4.1:81/stream");
    return httpd_resp_send(req, NULL, 0);
}

void start_webserver(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 8;
    
    // Servidor 1: Web y Comandos (Puerto 80)
    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t root = { .uri = "/", .method = HTTP_GET, .handler = root_get_handler, .user_ctx = NULL };
        httpd_uri_t ws = { .uri = "/ws", .method = HTTP_GET, .handler = ws_handler, .user_ctx = NULL, .is_websocket = true };
        httpd_uri_t stream_redir = { .uri = "/stream", .method = HTTP_GET, .handler = stream_redirect_handler, .user_ctx = NULL };
        httpd_register_uri_handler(server, &root);
        httpd_register_uri_handler(server, &ws);
        httpd_register_uri_handler(server, &stream_redir);
    }
    
    // Servidor 2: Stream de Video (Puerto 81)
    // ESP-IDF httpd es de un solo hilo. Si el stream bloquea el puerto 80, 
    // los comandos de las flechas nunca entran. Al poner el video en el 81,
    // pueden correr en paralelo sin estorbarse.
    config.server_port += 1;
    config.ctrl_port += 1;
    httpd_handle_t stream_server = NULL;
    if (httpd_start(&stream_server, &config) == ESP_OK) {
        httpd_uri_t stream = { .uri = "/stream", .method = HTTP_GET, .handler = stream_handler, .user_ctx = NULL };
        httpd_register_uri_handler(stream_server, &stream);
    }
}
