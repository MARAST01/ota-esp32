#pragma once

// ===== WiFi =====
#define WIFI_SSID "Milena"
#define WIFI_PASS "8deportivocali9"

// ===== MQTT =====
#define MQTT_SERVER "espunivalle.duckdns.org"
#define MQTT_PORT 8883
#define MQTT_USER "alvaro"
#define MQTT_PASS "supersecreto"

// Topics (el que escucha el ESP para recibir la orden de OTA)
#define MQTT_TOPIC_IN  "colombia/valle/tulua/1/alvaro/ota"
#define MQTT_TOPIC_OUT "colombia/valle/tulua/1/alvaro/out"

// Opcional: versión compilada (si quieres incrustarla en el bin)
#define FIRMWARE_VERSION "v0.0.1"
