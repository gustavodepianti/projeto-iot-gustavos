#include <Arduino.h>
#include <WiFi.h>
#include "secrets.h"

// ---------- Pinos ----------
const int PINO_SENSOR_A = 34;
const int PINO_SENSOR_B = 35;
const int PINO_ALIMENTACAO = 23;

// ---------- Parâmetros da leitura ----------
const int NUM_AMOSTRAS = 20;
const unsigned long TEMPO_ESTABILIZAR_MS = 20;
const unsigned long INTERVALO_LEITURA_MS = 2000;  // 2 s para testar

// ---------- Calibração (valores brutos) ----------
// Medida em 16/11/2026. Seco provisório: confirmar com terra seca.
const int CAL_A_SECO = 4095;
const int CAL_A_MOLHADO = 1410;
const int CAL_B_SECO = 4095;
const int CAL_B_MOLHADO = 1380;

// ---------- Wi-Fi ----------
const unsigned long INTERVALO_WIFI_MS = 10000;  // nova tentativa a cada 10 s

unsigned long ultimaLeitura = 0;
unsigned long ultimaTentativaWiFi = 0;
bool estavaConectado = false;  // estado do Wi-Fi na volta anterior do loop

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

// ================= Wi-Fi =================

// Confere o Wi-Fi a cada volta do loop, sem nunca esperar
void verificarWiFi() {
  bool conectado = (WiFi.status() == WL_CONNECTED);

  // Avisa só quando o estado MUDA
  if (conectado && !estavaConectado) {
    Serial.printf("Wi-Fi: conectado! IP: %s | sinal: %d dBm\n",
                  WiFi.localIP().toString().c_str(), WiFi.RSSI());
  }
  if (!conectado && estavaConectado) {
    Serial.println("Wi-Fi: conexao perdida");
  }
  estavaConectado = conectado;

  // Desconectado e já passou o intervalo: tenta de novo
  if (!conectado && millis() - ultimaTentativaWiFi >= INTERVALO_WIFI_MS) {
    ultimaTentativaWiFi = millis();
    Serial.println("Wi-Fi: tentando conectar...");
    WiFi.disconnect();
    WiFi.begin(WIFI_SSID, WIFI_SENHA);
  }
}

// ================= Programa =================

void setup() {
  Serial.begin(115200);

  pinMode(PINO_ALIMENTACAO, OUTPUT);
  digitalWrite(PINO_ALIMENTACAO, LOW);

  analogReadResolution(12);
  analogSetPinAttenuation(PINO_SENSOR_A, ADC_11db);
  analogSetPinAttenuation(PINO_SENSOR_B, ADC_11db);

  WiFi.mode(WIFI_STA);           // modo "cliente": conecta num roteador
  WiFi.setAutoReconnect(false);  // a reconexão é feita pelo nosso código
  WiFi.begin(WIFI_SSID, WIFI_SENHA);
  ultimaTentativaWiFi = millis();
  Serial.println("Wi-Fi: conectando...");
}

void loop() {
  verificarWiFi();

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
  }
}