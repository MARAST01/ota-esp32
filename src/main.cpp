#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <HTTPClient.h>
#include <Update.h>
#include <ArduinoJson.h>

// ==== CONFIGURACIÓN DE RED ====
const char* WIFI_SSID = "S20fe";
const char* WIFI_PASS = "12345678";

// ==== CONFIGURACIÓN MQTT ====
const char* MQTT_SERVER = "esp32univalle.duckdns.org";
const int   MQTT_PORT   = 8883;      // 8883 con TLS, 1883 sin TLS
const char* MQTT_USER   = "alvaro";
const char* MQTT_PASS   = "supersecreto";
const char* MQTT_TOPIC  = "test/topic";

// ==== CLIENTES ====
// Cliente seguro SOLO para MQTT
WiFiClientSecure mqttSecureClient;
PubSubClient mqttClient(mqttSecureClient);

// Cliente HTTP independiente para OTA
WiFiClientSecure httpsClient;

// ==== VARIABLES ====
String currentVersion = "v0.0.1";


// ============================
// ==== FUNCIÓN OTA HTTPS ====
// ============================
bool performOTA(String url) {
  Serial.println("Iniciando descarga OTA desde: " + url);

  HTTPClient https;

  httpsClient.setInsecure();     // Permitir OTA sin certificado
  https.begin(httpsClient, url);

  int httpCode = https.GET();
  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("Error HTTP: %d\n", httpCode);
    https.end();
    return false;
  }

  int contentLength = https.getSize();
  WiFiClient * stream = https.getStreamPtr();

  if (!Update.begin(contentLength)) {
    Update.printError(Serial);
    return false;
  }

  Serial.println("⬇ Escribiendo firmware...");
  size_t written = Update.writeStream(*stream);

  if (written != contentLength) {
    Serial.printf("ERROR: Escribió %d de %d bytes\n", written, contentLength);
  }

  if (Update.end()) {
    if (Update.isFinished()) {
      Serial.println("OTA completada. Reiniciando...");
      delay(1000);
      ESP.restart();
    }
  } else {
    Update.printError(Serial);
  }

  https.end();
  return true;
}


// ============================
// ========== CALLBACK =========
// ============================
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.println("Mensaje MQTT recibido");

  String msg;
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];

  Serial.println("Payload: " + msg);

  DynamicJsonDocument doc(512);
  if (deserializeJson(doc, msg)) {
    Serial.println("Error parseando JSON");
    return;
  }

  String newVersion = doc["version"];
  String url        = doc["url"];

  if (newVersion == currentVersion) {
    Serial.println("Ya tengo la versión más reciente.");
    return;
  }

  Serial.printf("Nueva versión %s disponible (actual %s)\n",
                newVersion.c_str(), currentVersion.c_str());

  performOTA(url);
}


// ============================
// ====== CONEXIÓN MQTT =======
// ============================
void reconnectMQTT() {
  while (!mqttClient.connected()) {
    Serial.print("Conectando a MQTT... ");

    if (MQTT_USER && strlen(MQTT_USER) > 0)
      mqttClient.connect("esp32-s3", MQTT_USER, MQTT_PASS);
    else
      mqttClient.connect("esp32-s3");

    if (mqttClient.connected()) {
      Serial.println("Conectado");
      mqttClient.subscribe(MQTT_TOPIC);
      Serial.println("Suscrito a: " + String(MQTT_TOPIC));
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
  delay(500);

  Serial.println("\n=== ESP32-S3 OTA por MQTT ===");

  // ---- WIFI ----
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Conectando a WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println("\nWiFi conectado!");
  Serial.println(WiFi.localIP());

  // ---- MQTT ----
  mqttSecureClient.setInsecure();   // Permitir MQTT sin certificado
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
