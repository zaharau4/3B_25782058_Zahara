#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DHT.h>

// Kredensial Wi-Fi & MQTT
const char* ssid = "hisnulmuslim";
const char* password = "*muslim#";

const char* mqttServer = "broker.hivemq.com";
const int mqttPort = 1883;
const char* clientId = "ESP8266_LabIoT_25782058_Tugas";

// Definisi Topik MQTT 
const char* topicTelemetry = "kampus/labiot/25782058/telemetri";
const char* topicCommand   = "kampus/labiot/25782058/kendali";
const char* topicAlert     = "kampus/labiot/25782058/alert";
const char* topicStatus    = "kampus/labiot/25782058/status";

// Konfigurasi Pin Perangkat Keras
const byte dhtPin = 2; // D4
const byte ledPin = 5; // D1
#define DHTTYPE DHT11

DHT dht(dhtPin, DHTTYPE);
WiFiClient espClient;
PubSubClient client(espClient);

unsigned long lastPublishTime = 0;
const unsigned long publishInterval = 4000; // Kirim data tiap 4 detik
bool alertTriggered = false;

// Fungsi Callback untuk Menerima Perintah Kendali LED dari Luar
void callback(char* topic, byte* payload, unsigned int length) {
  char buffer[length + 1];
  memcpy(buffer, payload, length);
  buffer[length] = '\0';

  Serial.printf("Pesan diterima pada topik [%s]: %s\n", topic, buffer);

  StaticJsonDocument<256> doc;
  if (!deserializeJson(doc, buffer)) {
    // Kendali manual hanya aktif jika kondisi suhu normal (tidak sedang alert)
    if (doc.containsKey("led") && !alertTriggered) {
      int val = doc["led"];
      digitalWrite(ledPin, val == 1 ? HIGH : LOW);
      Serial.print("Status LED diubah secara manual menjadi: ");
      Serial.println(val == 1 ? "NYALA" : "PADAM");
    }
  }
}

// Fungsi Rekoneksi Otomatis & Konfigurasi LWT (Last Will and Testament)
void reconnect() {
  while (!client.connected()) {
    Serial.print("Menghubungkan ke MQTT Broker...");
    
    // Konfigurasi pesan LWT (jika perangkat terputus mendadak)
    StaticJsonDocument<128> lwtDoc;
    lwtDoc["status"] = "OFFLINE";
    lwtDoc["keterangan"] = "Perangkat Terputus Tak Terduga";
    char lwtBuffer[128];
    serializeJson(lwtDoc, lwtBuffer);

    if (client.connect(clientId, NULL, NULL, topicStatus, 1, true, lwtBuffer)) {
      Serial.println(" BERHASIL terhubung!");
      
      // Subscribe kembali ke topik kendali
      client.subscribe(topicCommand);
      Serial.println("Berhasil subscribe ke topik kendali.");

      // Publikasikan status ONLINE saat terhubung
      StaticJsonDocument<128> onlineDoc;
      onlineDoc["status"] = "ONLINE";
      onlineDoc["keterangan"] = "Perangkat Beroperasi Normal";
      char onlineBuffer[128];
      serializeJson(onlineDoc, onlineBuffer);
      client.publish(topicStatus, onlineBuffer, true);
      
    } else {
      Serial.print(" GAGAL, rc=");
      Serial.print(client.state());
      Serial.println(" Coba lagi dalam 4 detik...");
      delay(4000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);
  dht.begin();

  // Koneksi Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("\nMenghubungkan ke Wi-Fi");
  
  while (WiFi.status() != WL_CONNECTED) { 
    delay(500); 
    Serial.print("."); 
  }
  
  Serial.println("\nWi-Fi Berhasil Terkoneksi!");
  Serial.print("Alamat IP Lokal: ");
  Serial.println(WiFi.localIP());

  // Pengaturan MQTT Server
  client.setServer(mqttServer, mqttPort);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // Non-blocking timer pengiriman data telemetri berkala
  unsigned long now = millis();
  if (now - lastPublishTime >= publishInterval) {
    lastPublishTime = now;

    float t = dht.readTemperature();
    float h = dht.readHumidity();

    if (!isnan(t) && !isnan(h)) {
      String kondisi = (t > 31.0) ? "PANAS" : "NORMAL";

      // Rule Engine Lokal (Closed-Loop Safety System)
      if (t > 31.0) {
        digitalWrite(ledPin, HIGH); // Otomatis nyalakan LED jika suhu > 31.0°C
        if (!alertTriggered) {
          alertTriggered = true;
          StaticJsonDocument<128> alertDoc;
          alertDoc["status"] = "BAHAYA";
          alertDoc["parameter"] = "OVERHEAT";
          alertDoc["nilai_suhu"] = serialized(String(t, 2));

          char alertBuffer[128];
          serializeJson(alertDoc, alertBuffer);
          client.publish(topicAlert, alertBuffer);
          Serial.println("PERINGATAN: Suhu melebihi 31.0°C! Alert dipublikasikan.");
        }
      } else {
        alertTriggered = false;
      }

      // Serialisasi Data Telemetri ke Format JSON
      StaticJsonDocument<256> doc;
      doc["node_id"] = clientId;
      doc["suhu"] = serialized(String(t, 2));
      doc["kelembapan"] = serialized(String(h, 2));
      doc["status_led"] = digitalRead(ledPin);
      doc["kondisi_suhu"] = kondisi;
      doc["uptime_detik"] = millis() / 1000;
      doc["ip_lokal"] = WiFi.localIP().toString();

      char buffer[256];
      serializeJson(doc, buffer);
      client.publish(topicTelemetry, buffer);
      
      // Cetak pemantauan ke Serial Monitor
      Serial.print("Kirim Telemetri -> ");
      Serial.println(buffer);
    } else {
      Serial.println("Gagal membaca data dari sensor DHT!");
    }
  }
}