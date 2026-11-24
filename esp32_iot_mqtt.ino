#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>

// ====== GANTI SESUAI KEBUTUHAN ======
const char* WIFI_SSID = "Wokwi-GUEST";   // ganti dengan WiFi asli jika pakai ESP32 fisik
const char* WIFI_PASS = "";              // password WiFi (kosong untuk Wokwi)
const char* NIM       = "2410010140";      // <- GANTI dengan NIM kamu

const char* MQTT_SERVER = "broker.hivemq.com";
const int   MQTT_PORT   = 1883;
// ====================================

#define DHT_PIN    4
#define DHT_TYPE   DHT22
#define RELAY_PIN  26
#define LED_PIN    2

#define PWM_FREQ   5000
#define PWM_RES    8       // 8 bit -> 0-255
#define PWM_CH     0       // dipakai di core 2.x

DHT dht(DHT_PIN, DHT_TYPE);
WiFiClient espClient;
PubSubClient client(espClient);

String topicSuhu, topicRelay, topicPwm;
unsigned long lastSend = 0;
const unsigned long SEND_INTERVAL = 3000;  // 3 detik

void pwmWrite(int duty) {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(LED_PIN, duty);
#else
  ledcWrite(PWM_CH, duty);
#endif
}

void setupPwm() {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(LED_PIN, PWM_FREQ, PWM_RES);
#else
  ledcSetup(PWM_CH, PWM_FREQ, PWM_RES);
  ledcAttachPin(LED_PIN, PWM_CH);
#endif
  pwmWrite(0);
}

void setupWifi() {
  Serial.print("Menghubungkan WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi terhubung, IP: " + WiFi.localIP().toString());
}

void callback(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];
  msg.trim();
  String t = String(topic);
  Serial.println("Pesan [" + t + "]: " + msg);

  if (t == topicRelay) {
    if (msg.equalsIgnoreCase("ON")) {
      digitalWrite(RELAY_PIN, HIGH);
      Serial.println("Relay ON");
    } else if (msg.equalsIgnoreCase("OFF")) {
      digitalWrite(RELAY_PIN, LOW);
      Serial.println("Relay OFF");
    }
  } else if (t == topicPwm) {
    int duty = constrain(msg.toInt(), 0, 255);
    pwmWrite(duty);
    Serial.println("PWM = " + String(duty));
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Menghubungkan MQTT...");
    String clientId = "esp32-" + String(NIM) + "-" + String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      Serial.println("terhubung");
      client.subscribe(topicRelay.c_str());
      client.subscribe(topicPwm.c_str());
    } else {
      Serial.print("gagal, rc=");
      Serial.println(client.state());
      delay(2000);
    }
  }
}

void setup() {
  Serial.begin(115200);

  topicSuhu  = "iot/" + String(NIM) + "/sensor/suhu";
  topicRelay = "iot/" + String(NIM) + "/aktuator/relay";
  topicPwm   = "iot/" + String(NIM) + "/aktuator/pwm";

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  setupPwm();
  dht.begin();

  setupWifi();
  client.setServer(MQTT_SERVER, MQTT_PORT);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) reconnect();
  client.loop();

  unsigned long now = millis();
  if (now - lastSend >= SEND_INTERVAL) {
    lastSend = now;
    float suhu = dht.readTemperature();
    if (isnan(suhu)) {
      Serial.println("Gagal membaca DHT22!");
      return;
    }
    char buf[10];
    dtostrf(suhu, 4, 1, buf);
    client.publish(topicSuhu.c_str(), buf);
    Serial.println("Suhu terkirim: " + String(buf) + " C");
  }
}