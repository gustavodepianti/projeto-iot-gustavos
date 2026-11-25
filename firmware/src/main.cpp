#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "secrets.h"

// ---------- Pinos ----------
const int PINO_SENSOR_A = 34;
const int PINO_SENSOR_B = 35;
const int PINO_ALIMENTACAO = 23;

// ---------- Parâmetros da leitura ----------
const int NUM_AMOSTRAS = 20;
const unsigned long TEMPO_ESTABILIZAR_MS = 20;
const unsigned long INTERVALO_LEITURA_MS = 60000;  // 60 s, como pede o projeto

// ---------- Calibração (valores brutos) ----------
// Medida em 16/11/2026. Seco provisório: confirmar com terra seca.
const int CAL_A_SECO = 4095;
const int CAL_A_MOLHADO = 1410;   // <-- mantenha o valor que você mediu
const int CAL_B_SECO = 4095;
const int CAL_B_MOLHADO = 1380;

// ---------- Wi-Fi ----------
const unsigned long INTERVALO_WIFI_MS = 10000;

// ---------- MQTT ----------
const char* CLIENT_ID = "esp32-gustavos";
const char* TOPICO_DADOS = "inatel/gustavos/dados";
const char* TOPICO_STATUS = "inatel/gustavos/status";
const unsigned long INTERVALO_MQTT_MS = 5000;

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

unsigned long ultimaLeitura = 0;
unsigned long ultimaTentativaWiFi = 0;
unsigned long ultimaTentativaMqtt = 0;
bool estavaConectado = false;

// ================= Sensores =================

void mediaDoPino(int pino, int &bruto, int &milivolts) {
  long somaBruto = 0;
  long somaMv = 0;
  for (int i = 0; i < NUM_AMOSTRAS; i++) {
    somaBruto += analogRead(pino);
    somaMv += analogReadMilliVolts(pino);
  }
  bruto = somaBruto / NUM_AMOSTRAS;
  milivolts = somaMv / NUM_AMOSTRAS;
}

void lerSensores(int &brutoA, int &mvA, int &brutoB, int &mvB) {
  digitalWrite(PINO_ALIMENTACAO, HIGH);
  delay(TEMPO_ESTABILIZAR_MS);

  mediaDoPino(PINO_SENSOR_A, brutoA, mvA);
  mediaDoPino(PINO_SENSOR_B, brutoB, mvB);

  digitalWrite(PINO_ALIMENTACAO, LOW);
}

float calcularPct(int bruto, int seco, int molhado) {
  float pct = (float)(seco - bruto) / (seco - molhado) * 100.0f;
  return constrain(pct, 0.0f, 100.0f);
}

// Arredonda para 1 casa decimal (ex.: 62.37 -> 62.4)
float umaCasa(float valor) {
  return roundf(valor * 10.0f) / 10.0f;
}

// ================= Wi-Fi =================

void verificarWiFi() {
  bool conectado = (WiFi.status() == WL_CONNECTED);

  if (conectado && !estavaConectado) {
    Serial.printf("Wi-Fi: conectado! IP: %s | sinal: %d dBm\n",
                  WiFi.localIP().toString().c_str(), WiFi.RSSI());
  }
  if (!conectado && estavaConectado) {
    Serial.println("Wi-Fi: conexao perdida");
  }
  estavaConectado = conectado;

  if (!conectado && millis() - ultimaTentativaWiFi >= INTERVALO_WIFI_MS) {
    ultimaTentativaWiFi = millis();
    Serial.println("Wi-Fi: tentando conectar...");
    WiFi.disconnect();
    WiFi.begin(WIFI_SSID, WIFI_SENHA);
  }
}

// ================= MQTT =================

void verificarMQTT() {
  if (WiFi.status() != WL_CONNECTED) return;

  if (mqtt.connected()) {
    mqtt.loop();
    return;
  }

  if (millis() - ultimaTentativaMqtt < INTERVALO_MQTT_MS) return;
  ultimaTentativaMqtt = millis();

  Serial.println("MQTT: conectando...");
  bool ok = mqtt.connect(CLIENT_ID, MQTT_USUARIO, MQTT_SENHA,
                         TOPICO_STATUS, 0, true, "offline");

  if (ok) {
    Serial.println("MQTT: conectado!");
    mqtt.publish(TOPICO_STATUS, "online", true);
  } else {
    Serial.printf("MQTT: falhou, codigo %d\n", mqtt.state());
  }
}

// Monta o JSON e publica no tópico de dados
void publicarLeituras(int brutoA, float pctA, int brutoB, float pctB) {
  if (!mqtt.connected()) {
    Serial.println("MQTT: desconectado, leitura nao publicada");
    return;
  }

  JsonDocument doc;
  doc["dispositivo"] = CLIENT_ID;
  doc["projeto"] = "solo";

  JsonObject leituras = doc["leituras"].to<JsonObject>();
  leituras["solo_bruto"] = brutoA;          // sensor A: grandezas padrão
  leituras["solo_pct"] = umaCasa(pctA);
  leituras["solo_bruto_b"] = brutoB;        // sensor B: grandezas extras
  leituras["solo_pct_b"] = umaCasa(pctB);

  char payload[256];
  serializeJson(doc, payload, sizeof(payload));

  bool ok = mqtt.publish(TOPICO_DADOS, payload);  // não retido
  Serial.printf("MQTT: %s -> %s\n", ok ? "publicado" : "FALHOU", payload);
}

// ================= Programa =================

void setup() {
  Serial.begin(115200);

  pinMode(PINO_ALIMENTACAO, OUTPUT);
  digitalWrite(PINO_ALIMENTACAO, LOW);

  analogReadResolution(12);
  analogSetPinAttenuation(PINO_SENSOR_A, ADC_11db);
  analogSetPinAttenuation(PINO_SENSOR_B, ADC_11db);

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(false);
  WiFi.begin(WIFI_SSID, WIFI_SENHA);
  ultimaTentativaWiFi = millis();
  Serial.println("Wi-Fi: conectando...");

  mqtt.setServer(MQTT_HOST, MQTT_PORTA);
  mqtt.setBufferSize(512);  // folga para o JSON
}

void loop() {
  verificarWiFi();
  verificarMQTT();

  unsigned long agora = millis();
  if (agora - ultimaLeitura >= INTERVALO_LEITURA_MS) {
    ultimaLeitura = agora;

    int brutoA, mvA, brutoB, mvB;
    lerSensores(brutoA, mvA, brutoB, mvB);

    float pctA = calcularPct(brutoA, CAL_A_SECO, CAL_A_MOLHADO);
    float pctB = calcularPct(brutoB, CAL_B_SECO, CAL_B_MOLHADO);

    Serial.printf("A: %4d (%.2f V) %5.1f %% | B: %4d (%.2f V) %5.1f %%\n",
                  brutoA, mvA / 1000.0, pctA,
                  brutoB, mvB / 1000.0, pctB);

    publicarLeituras(brutoA, pctA, brutoB, pctB);
  }
}