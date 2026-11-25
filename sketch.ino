#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "driver/gpio.h"
#include "esp_sleep.h"

// --- CONFIGURAÇÕES DE REDE E MQTT ---
const char* ssid = "Wokwi-GUEST";
const char* password = "";
const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;

const char* topic_telemetria = "univasf/laisa/igor/telemetria";
const char* topic_config     = "univasf/laisa/igor/config";

// --- PINOS E TEMPO DE HIBERNAÇÃO ---
const int PINO_SENSOR = 34;
const int PINO_RELE   = 26;
const uint64_t TEMPO_SLEEP_SEGUNDOS = 10; // 10s para teste (em campo: 15 a 30 min)

// --- MEMÓRIA RTC (Preserva os dados durante o Deep Sleep) ---
RTC_DATA_ATTR float umidadeLiga    = 35.0;
RTC_DATA_ATTR float umidadeDesliga = 60.0;
RTC_DATA_ATTR bool estadoBomba     = false;
RTC_DATA_ATTR int cicloBoot        = 0;

WiFiClient espClient;
PubSubClient client(espClient);

void callbackMQTT(char* topic, byte* payload, unsigned int length) {
  JsonDocument doc;
  DeserializationError erro = deserializeJson(doc, payload, length);
  if (!erro) {
    if (doc["liga"].is<float>())    umidadeLiga = doc["liga"];
    if (doc["desliga"].is<float>()) umidadeDesliga = doc["desliga"];
    Serial.printf("Limiares atualizados -> Liga: %.1f%% | Desliga: %.1f%%\n", umidadeLiga, umidadeDesliga);
  }
}

void conectarWiFi() {
  Serial.print("Conectando ao Wi-Fi");
  WiFi.begin(ssid, password, 6);
  int tentativas = 0;
  while (WiFi.status() != WL_CONNECTED && tentativas < 20) {
    delay(250);
    Serial.print(".");
    tentativas++;
  }
  Serial.println(WiFi.status() == WL_CONNECTED ? " Conectado!" : " Falha!");
}

void conectarMQTT() {
  int tentativas = 0;
  while (!client.connected() && tentativas < 3) {
    Serial.print("Conectando ao Broker MQTT...");
    String clientId = "ESP32-LAISA-" + String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      Serial.println(" Conectado!");
      client.subscribe(topic_config);
    } else {
      Serial.print(" Falha, rc=");
      Serial.println(client.state());
      delay(1000);
      tentativas++;
    }
  }
}

void setup() {
  Serial.begin(115200);
  cicloBoot++;
  Serial.printf("\n--- Acordando (Ciclo #%d) ---\n", cicloBoot);

  // Restaura o estado do relé e libera a trava do Deep Sleep
  pinMode(PINO_RELE, OUTPUT);
  digitalWrite(PINO_RELE, estadoBomba ? HIGH : LOW);
  gpio_hold_dis((gpio_num_t)PINO_RELE);
  gpio_deep_sleep_hold_dis();

  // 1. Conecta à rede e verifica se existe configuração nova retida no MQTT
  conectarWiFi();
  if (WiFi.status() == WL_CONNECTED) {
    client.setServer(mqtt_server, mqtt_port);
    client.setCallback(callbackMQTT);
    conectarMQTT();
    for (int i = 0; i < 10; i++) {
      client.loop();
      delay(100);
    }
  }

  // 2. Leitura do sensor (ADC 12 bits -> 0 a 100%)
  int leituraBruta = analogRead(PINO_SENSOR);
  float umidadeAtual = (leituraBruta / 4095.0) * 100.0;

  // 3. Controle por Histerese (usando estado salvo na memória RTC)
  if (!estadoBomba && umidadeAtual < umidadeLiga) {
    estadoBomba = true;
    digitalWrite(PINO_RELE, HIGH);
  } else if (estadoBomba && umidadeAtual > umidadeDesliga) {
    estadoBomba = false;
    digitalWrite(PINO_RELE, LOW);
  }

  // 4. Publica telemetria via MQTT
  JsonDocument doc;
  doc["ciclo"]          = cicloBoot;
  doc["umidade_solo"]   = round(umidadeAtual * 10) / 10.0;
  doc["bomba_ligada"]   = estadoBomba;
  doc["limiar_liga"]    = umidadeLiga;
  doc["limiar_desliga"] = umidadeDesliga;
  doc["deep_sleep_s"]   = TEMPO_SLEEP_SEGUNDOS;

  char buffer[240];
  serializeJson(doc, buffer);
  Serial.println(buffer);

  if (client.connected()) {
    client.publish(topic_telemetria, buffer);
    client.loop();
    delay(300); // Garante o envio antes de desligar o Wi-Fi
    client.disconnect();
  }
  WiFi.disconnect(true);

  // 5. Trava o estado atual do relé e entra em Deep Sleep
  gpio_hold_en((gpio_num_t)PINO_RELE);
  gpio_deep_sleep_hold_en();

  Serial.printf("Entrando em Deep Sleep por %llu segundos...\n", TEMPO_SLEEP_SEGUNDOS);
  Serial.flush();
  esp_sleep_enable_timer_wakeup(TEMPO_SLEEP_SEGUNDOS * 1000000ULL);
  esp_deep_sleep_start();
}

void loop() {
  // Não utilizado: ao acordar do Deep Sleep, o ESP32 reinicia pelo setup()
}
