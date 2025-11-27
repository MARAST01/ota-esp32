#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <HTTPClient.h>
#include <Update.h>
#include <ArduinoJson.h>

#include "secrets.h"   

// ============================
// Variables globales
// ============================

String currentVersion = "v0.0.1";   // La versión del firmware que tienes ahora

WiFiClientSecure mqttSecureClient;
PubSubClient mqttClient(mqttSecureClient);

// ============================
// ===== FUNCIÓN OTA HTTPS ====
// ============================

bool performOTA(const String& url) {
  Serial.println("\n====================================");
  Serial.println("        INICIANDO OTA DESDE S3      ");
  Serial.println("====================================");
  Serial.println("URL:");
  Serial.println(url);
  Serial.println("====================================");

  if (!url.startsWith("https://")) {
    Serial.println("ERROR: La URL OTA no es HTTPS.");
    return false;
  }

  WiFiClientSecure httpsClient;
  httpsClient.setInsecure();  // Permitir certificados S3 sin verificar

  HTTPClient https;

  if (!https.begin(httpsClient, url)) {
    Serial.println(" ERROR: No se pudo iniciar conexión HTTPS.");
    return false;
  }

  int httpCode = https.GET();
  Serial.printf("Código HTTP recibido: %d\n", httpCode);

  if (httpCode != HTTP_CODE_OK) {
    Serial.println("ERROR: No se pudo descargar firmware desde S3.");
    https.end();
    return false;
  }

  int contentLength = https.getSize();
  if (contentLength <= 0) {
    Serial.println(" ERROR: Tamaño de firmware inválido.");
    https.end();
    return false;
  }

  WiFiClient * stream = https.getStreamPtr();
  Serial.printf("Tamaño del firmware: %d bytes\n", contentLength);

  if (!Update.begin(contentLength)) {
    Serial.println(" ERROR al preparar OTA:");
    Update.printError(Serial);
    https.end();
    return false;
  }

  Serial.println("⬇ Escribiendo firmware...");

  size_t written = Update.writeStream(*stream);

  if (written != contentLength) {
    Serial.printf(" ERROR: escritos %d bytes de %d\n", written, contentLength);
    https.end();
    return false;
  }

  if (!Update.end()) {
    Serial.println(" ERROR finalizando OTA:");
    Update.printError(Serial);
    https.end();
    return false;
  }

  if (!Update.isFinished()) {
    Serial.println(" ERROR: OTA no completada.");
    https.end();
    return false;
  }

  Serial.println("====================================");
  Serial.println("       OTA COMPLETADA EXITOSAMENTE");
  Serial.println("       Reiniciando ESP32...");
  Serial.println("====================================");

  delay(1500);
  ESP.restart();
  return true;
}


// ============================
// ===== CALLBACK MQTT ========
// ============================

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.println("\n=== MQTT Mensaje recibido ===");
  Serial.print("Topic: ");
  Serial.println(topic);

  String msg;
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];

  Serial.println("Payload:");
  Serial.println(msg);

  DynamicJsonDocument doc(2048);
  auto error = deserializeJson(doc, msg);

  if (error) {
    Serial.print(" Error parseando JSON: ");
    Serial.println(error.f_str());
    return;
  }

  if (!doc.containsKey("version") || !doc.containsKey("url")) {
    Serial.println(" JSON inválido. Falta 'version' o 'url'");
    return;
  }

  String newVersion = doc["version"].as<String>();
  String url        = doc["url"].as<String>();

  Serial.printf("Versión actual: %s\n", currentVersion.c_str());
  Serial.printf("Nueva versión: %s\n", newVersion.c_str());

  if (newVersion == currentVersion) {
    Serial.println(" Ya tengo la versión más reciente.");
    return;
  }

  Serial.println(" NUEVA VERSIÓN DETECTADA — INICIANDO OTA");
  performOTA(url);
}



// ============================
// ===== RECONNECT MQTT =======
// ============================

void reconnectMQTT() {
  while (!mqttClient.connected()) {
    Serial.print("Conectando a MQTT... ");

    if (strlen(MQTT_USER) > 0)
      mqttClient.connect("esp32-s3", MQTT_USER, MQTT_PASS);
    else
      mqttClient.connect("esp32-s3");

    if (mqttClient.connected()) {
      Serial.println(" Conectado a MQTT.");
      mqttClient.subscribe(MQTT_TOPIC_IN);
      Serial.print(" Suscrito al topic: ");
      Serial.println(MQTT_TOPIC_IN);
    } else {
      Serial.print("Error MQTT: ");
      Serial.println(mqttClient.state());
      delay(3000);
    }
  }
}


// ============================
// ========= SETUP ============
// ============================

void setup() {
  Serial.begin(115200);
  delay(300);

  Serial.println("\n=== ESP32 OTA by MQTT + HTTPS (S3) ===");

  // Conexión WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Conectando a WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }

  Serial.println("\n✔ WiFi conectado");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  // Configurar MQTT
  mqttSecureClient.setInsecure();
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
}


// ============================
// ========== LOOP ============
// ============================

void loop() {
  if (!mqttClient.connected()) reconnectMQTT();
  mqttClient.loop();
}
