#include <Arduino.h>

// ---------- Pinos ----------
const int PINO_SENSOR_A = 34;     // AO do sensor A (ADC1)
const int PINO_SENSOR_B = 35;     // AO do sensor B (ADC1)
const int PINO_ALIMENTACAO = 23;  // VCC dos dois sensores

// ---------- Parâmetros da leitura ----------
const int NUM_AMOSTRAS = 20;
const unsigned long TEMPO_ESTABILIZAR_MS = 20;
const unsigned long INTERVALO_LEITURA_MS = 2000;  // 2 s para testar

// ---------- Calibração (valores brutos) ----------
// Medida em 16/11/2026. Seco provisório: confirmar com terra seca.
const int CAL_A_SECO = 4095;
const int CAL_A_MOLHADO = 1412;
const int CAL_B_SECO = 4095;
const int CAL_B_MOLHADO = 1380;

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

// Converte o valor bruto em percentual relativo (0 % = seco, 100 % = molhado)
float calcularPct(int bruto, int seco, int molhado) {
  float pct = (float)(seco - bruto) / (seco - molhado) * 100.0f;
  return constrain(pct, 0.0f, 100.0f);  // limita entre 0 e 100
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

    float pctA = calcularPct(brutoA, CAL_A_SECO, CAL_A_MOLHADO);
    float pctB = calcularPct(brutoB, CAL_B_SECO, CAL_B_MOLHADO);

    Serial.printf("A: %4d (%.2f V) %5.1f %% | B: %4d (%.2f V) %5.1f %%\n",
                  brutoA, mvA / 1000.0, pctA,
                  brutoB, mvB / 1000.0, pctB);
  }
}