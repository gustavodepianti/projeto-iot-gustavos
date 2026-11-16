#include <Arduino.h>

// ---------- Pinos ----------
const int PINO_SENSOR_A = 34;     // AO do sensor A (ADC1)
const int PINO_SENSOR_B = 35;     // AO do sensor B (ADC1)
const int PINO_ALIMENTACAO = 23;  // VCC dos dois sensores

// ---------- Parâmetros da leitura ----------
const int NUM_AMOSTRAS = 20;
const unsigned long TEMPO_ESTABILIZAR_MS = 20;
const unsigned long INTERVALO_LEITURA_MS = 2000;  // 2 s para testar

unsigned long ultimaLeitura = 0;

// Faz a média de várias leituras de UM pino
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

// Liga os sensores, lê os dois e desliga
void lerSensores(int &brutoA, int &mvA, int &brutoB, int &mvB) {
  digitalWrite(PINO_ALIMENTACAO, HIGH);
  delay(TEMPO_ESTABILIZAR_MS);

  mediaDoPino(PINO_SENSOR_A, brutoA, mvA);
  mediaDoPino(PINO_SENSOR_B, brutoB, mvB);

  digitalWrite(PINO_ALIMENTACAO, LOW);
}

void setup() {
  Serial.begin(115200);

  pinMode(PINO_ALIMENTACAO, OUTPUT);
  digitalWrite(PINO_ALIMENTACAO, LOW);

  analogReadResolution(12);
  analogSetPinAttenuation(PINO_SENSOR_A, ADC_11db);
  analogSetPinAttenuation(PINO_SENSOR_B, ADC_11db);
}

void loop() {
  unsigned long agora = millis();

  if (agora - ultimaLeitura >= INTERVALO_LEITURA_MS) {
    ultimaLeitura = agora;

    int brutoA, mvA, brutoB, mvB;
    lerSensores(brutoA, mvA, brutoB, mvB);

    Serial.printf("A: %4d (%.2f V) | B: %4d (%.2f V)\n",
                  brutoA, mvA / 1000.0, brutoB, mvB / 1000.0);
  }
}