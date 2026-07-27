#include <Arduino.h>
#include <WiFi.h>
#include <esp_camera.h>
#include <WebSocketsClient.h>
#define DEBUG_WEBSOCKETS_CLIENT

const char* ssid = "Ha Nguyen";
const char* password = "12345678";
const char* ws_host = "kakaytbrr-projectaiproplay.hf.space";
const int ws_port = 443;
const char* ws_path = "/esp32stream";
const char* api_key = "memaybeo3667";

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

WebSocketsClient webSocket;
unsigned long lastCaptureTime = 0;
volatile int currentCaptureInterval = 100;
unsigned long lastWiFiCheck = 0;
const unsigned long wifiCheckInterval = 5000;

unsigned long lastConnectedAt = 0;

int extractJsonIntValue(const String& json, const char* key) {
    String searchKey = String("\"") + key + "\":";
    int idx = json.indexOf(searchKey);
    if (idx == -1) return -1;
    return json.substring(idx + searchKey.length()).toInt();
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
    switch(type) {
        case WStype_DISCONNECTED: {
            unsigned long now = millis();
            unsigned long aliveFor = lastConnectedAt ? (now - lastConnectedAt) : 0;
            Serial.printf("[WS] Disconnected @ %lu ms | song duoc %lu ms | RSSI=%d\n", now, aliveFor, WiFi.RSSI());
            break;
        }
        case WStype_CONNECTED: {
            unsigned long now = millis();
            Serial.printf("[WS] Connected @ %lu ms | RSSI=%d\n", now, WiFi.RSSI());
            lastConnectedAt = now;
            break;
        }
        case WStype_PING:
            Serial.printf("[WS] Ping nhan @ %lu ms\n", millis());
            break;
        case WStype_PONG:
            Serial.printf("[WS] Pong nhan @ %lu ms\n", millis());
            break;
        case WStype_TEXT: {
            String msg = String((char*)payload).substring(0, length);
            if (msg.indexOf("SET_FPS") != -1) {
                int fps = extractJsonIntValue(msg, "value");
                if (fps > 0 && fps <= 30) currentCaptureInterval = 1000 / fps;
            }
            break;
        }
        default: break;
    }
}

bool initCamera() {
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM; config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM; config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM; config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM; config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM; config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM; config.pin_href = HREF_GPIO_NUM;
    config.pin_sccb_sda = SIOD_GPIO_NUM; config.pin_sccb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM; config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 2;
    config.fb_location = CAMERA_FB_IN_PSRAM;

    return esp_camera_init(&config) == ESP_OK;
}

#define WEBSOCKETS_DEBUG_LEVEL      WStype_ERROR
void setup() {
    Serial.begin(115200);
    if (!initCamera()) { Serial.println("Camera init failed!"); return; }

    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) { delay(500); }
    Serial.printf("[WiFi] Connected, RSSI=%d\n", WiFi.RSSI());

    String authHeader = "x-api-key: " + String(api_key);
    webSocket.setExtraHeaders(authHeader.c_str());
    webSocket.beginSSL(ws_host, ws_port, ws_path);
    webSocket.onEvent(webSocketEvent);
    webSocket.setReconnectInterval(1000);
    webSocket.enableHeartbeat(15000, 3000, 2);
}

void loop() {
    if (millis() - lastWiFiCheck >= wifiCheckInterval) {
        lastWiFiCheck = millis();
        if (WiFi.status() != WL_CONNECTED) {
            WiFi.disconnect();
            WiFi.reconnect();
            return;
        }
    }
    if (WiFi.status() == WL_CONNECTED) webSocket.loop();

    if (webSocket.isConnected() && (millis() - lastCaptureTime >= (unsigned long)currentCaptureInterval)) {
        lastCaptureTime = millis();
        camera_fb_t * fb = esp_camera_fb_get();
        if (!fb) return;
        webSocket.sendBIN(fb->buf, fb->len);
        esp_camera_fb_return(fb);
    }
}
