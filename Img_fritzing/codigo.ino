#include <WiFi.h>
#include <PubSubClient.h>
#include <math.h>
#include "time.h"


const int analogPin = 34;


const char* ssid = "Internet_UNL";
const char* password = "UNL1859WiFi";
//const char* ssid = "DAYSI";
//const char* password = "Tauro181100";
const char* mqtt_server = "181.113.129.26";
const int mqtt_port = 1883;
const char* topic = "ruido";


const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -5 * 3600;
const int daylightOffset_sec = 0;


WiFiClient espClient;
PubSubClient client(espClient);


void setupWiFi() {
  Serial.print("Conectando a Wi-Fi: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);


  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }


  Serial.println("\nConexión Wi-Fi establecida.");
  Serial.print("IP asignada: ");
  Serial.println(WiFi.localIP());
}


void reconnect() {
  while (!client.connected()) {
    Serial.print("Conectando al broker MQTT...");
    if (client.connect("ESP32Client")) {
      Serial.println("Conectado.");
    } else {
      Serial.print("Fallo en la conexión. Código: ");
      Serial.println(client.state());
      delay(5000);
    }
  }
}


float analogToDecibels(int analogValue) {
  const int referenceValue = 34;
  const float scalingFactor = 0.125;
  if (analogValue <= referenceValue) return 0;
  float decibels = (analogValue - referenceValue) * scalingFactor;
  return decibels > 1.0 ? decibels : 1.0;
}


String getTimestamp() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "NoTime";
  }
  char buffer[25];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(buffer);
}


void setup() {
  Serial.begin(115200);
  pinMode(analogPin, INPUT);
  setupWiFi();
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.println("Sincronizando hora con NTP...");
  delay(2000);
  client.setServer(mqtt_server, mqtt_port);
}


// Variables para promedio y rango cada 30s
float sumDecibels = 0;
float minDecibels = 9999;
float maxDecibels = 0;
int sampleCount = 0;
unsigned long lastPublish = 0;
const unsigned long interval = 30000;  // 30 segundos


void loop() {
  if (!client.connected()) reconnect();
  client.loop();


  int analogValue = analogRead(analogPin);
  float decibels = analogToDecibels(analogValue);
  sumDecibels += decibels;
  minDecibels = min(minDecibels, decibels);
  maxDecibels = max(maxDecibels, decibels);
  sampleCount++;


  unsigned long currentMillis = millis();
  if (currentMillis - lastPublish >= interval) {
    float avgDecibels = sumDecibels / sampleCount;
    String timestamp = getTimestamp();


    // Coordenadas fijas del sensor
    const char* lat = "-4.030417";
    const char* lon = "-79.199528";


    // Payload completo
    char payload[256];
    snprintf(payload, sizeof(payload),
             "{"
             "\"sensor\":\"KY-038\","
             "\"tipo\":\"Sonido analógico\","
             "\"dato_raw\":%d,"
             "\"ruido_dB\":%.2f,"
             "\"promedio_30s\":%.2f,"
             "\"min_30s\":%.2f,"
             "\"max_30s\":%.2f,"
             "\"timestamp\":\"%s\","
             "\"ubicacion\":{\"lat\":%s,\"lon\":%s}"
             "}",
             analogValue, decibels, avgDecibels, minDecibels, maxDecibels,
             timestamp.c_str(), lat, lon);


    client.publish(topic, payload);
    Serial.println("---- Publicado ----");
    Serial.println(payload);


    // Reiniciar acumuladores
    sumDecibels = 0;
    sampleCount = 0;
    minDecibels = 9999;
    maxDecibels = 0;
    lastPublish = currentMillis;
  }


  delay(1000);
}

