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
const int   MQTT_PORT   = 8883;                // 8883 para TLS o 1883 si es sin TLS
const char* MQTT_USER   = "alvaro";      // o nullptr si no hay auth
const char* MQTT_PASS   = "supersecreto";
const char* MQTT_TOPIC  = "test/topic";

// ==== CLIENTES ====
WiFiClientSecure secureClient;
PubSubClient mqttClient(secureClient);

// ==== VARIABLES GLOBALES ====
String currentVersion = "v0.0.1"; // versión actual (puedes cambiarla en tu firmware)

// ==== DESCARGA Y APLICA OTA ====
bool performOTA(String url) {
  Serial.println("Iniciando descarga OTA desde: " + url);
  HTTPClient https;

  // Si usas certificados propios, cámbialo por setCACert()
  secureClient.setInsecure();
  https.begin(secureClient, url);

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

  Serial.println("⬇Escribiendo firmware...");
  size_t written = Update.writeStream(*stream);

  if (written == contentLength) {
    Serial.println("Firmware descargado correctamente");
  } else {
    Serial.printf("Escribió solo %d de %d bytes\n", written, contentLength);
  }

  if (Update.end()) {
    if (Update.isFinished()) {
      Serial.println("OTA completada. Reiniciando...");
      delay(1000);
      ESP.restart();
    } else {
      Serial.println("OTA incompleta");
    }
  } else {
    Update.printError(Serial);
  }

  https.end();
  return true;
}

// ==== CALLBACK DE MQTT ====
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.println("Mensaje OTA recibido");

  String msg;
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];
  Serial.println("Payload: " + msg);

  DynamicJsonDocument doc(512);
  DeserializationError err = deserializeJson(doc, msg);
  if (err) {
    Serial.println("Error parseando JSON");
    return;
  }

  String newVersion = doc["version"].as<String>();
  String url = doc["url"].as<String>();

  if (newVersion == currentVersion) {
    Serial.println("ℹYa tengo la versión más reciente (" + newVersion + ")");
    return;
  }

  Serial.printf("Nueva versión %s disponible (actual %s)\n", newVersion.c_str(), currentVersion.c_str());
  performOTA(url);
}

// ==== CONEXIÓN A MQTT ====
void reconnectMQTT() {
  while (!mqttClient.connected()) {
    Serial.print("Conectando a MQTT...");
    if (MQTT_USER && strlen(MQTT_USER) > 0)
      mqttClient.connect("esp32-client", MQTT_USER, MQTT_PASS);
    else
      mqttClient.connect("esp32-client");

    if (mqttClient.connected()) {
      Serial.println("Conectado a broker");
      mqttClient.subscribe(MQTT_TOPIC);
      Serial.println("Suscrito a: " + String(MQTT_TOPIC));
    } else {
      Serial.print("Error MQTT: ");
      Serial.println(mqttClient.state());
      delay(5000);
    }
  }
}

// ==== SETUP ====
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n=== OTA MQTT ESP32S3 ===");

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConectado a WiFi");

  secureClient.setInsecure(); // Si no usas certificado TLS
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
}

// ==== LOOP ====
void loop() {
  if (!mqttClient.connected()) reconnectMQTT();
  mqttClient.loop();
}
