# projeto-iot-gustavos — Monitor de umidade do solo

Projeto individual do curso **CTIoT**: acompanha a umidade da terra de um vaso e indica quando ela está **secando** e precisa de rega.

**Identificação:** aluno `gustavos` · dispositivo `esp32-gustavos` · projeto `solo` · usuários do broker `esp-gustavos`, `leitor-gustavos` e `admin-gustavos`.

> **⏳** = ainda depende de etapas futuras.

## Arquitetura

```
Sensor ─(tensão)→ ESP32 ─(MQTT/Wi-Fi, JSON)→ Mosquitto ─→ Coletor Python ─→ MySQL
                                                                              │
              Grafana ←─(HTTP/JSON, plugin Infinity)── API FastAPI ←──────────┘
```

Broker (porta 1883) e Grafana (3000) rodam em Docker; coletor, MySQL (3306) e API (8000) rodam direto na VM. Cada camada tem uma responsabilidade só, então dá para testar cada uma isoladamente. Contratos de tópicos, banco, API e dashboard seguem o escopo geral do CTIoT.

## Status dos requisitos

| # | Requisito | Status |
|---|---|---|
| R1 | Leitura analógica com média de várias amostras | ✅ média de 20 amostras |
| R2 | Calibração seco/molhado → `solo_pct` relativo | ✅
| R3 | Publica a cada 60 s; reconecta sozinho; status com Last Will | ✅ |
| R4 | Broker em container, sem anônimo, senhas com hash e ACL | ⏳ |
| R5 | Coletor valida e grava as grandezas | ⏳ |
| R6 | API com os 4 endpoints padrão + `/secagem` | ⏳ |
| R7 | Grafana com medidor e faixas, gráfico de 24 h e "secando" | ⏳ |
| R8 | Plano de testes, incluindo um ciclo real de rega e secagem | ⏳ |

## Materiais e ligação

- ESP32 DevKit (`esp32dev`), alimentado pela USB
- 2 sensores de umidade do solo **resistivos** (garfo + módulo AO/DO), chamados de A e B
- Jumpers, protoboard e vaso com terra (⏳ tipo de solo e profundidade)

| Sensor | VCC | GND | AO | DO |
|---|---|---|---|---|
| A | GPIO 23 | GND | GPIO 34 | não usado |
| B | GPIO 23 | GND | GPIO 35 | não usado |

ADC com 12 bits (0 a 4095) e atenuação de 11 dB. ⏳ Esquema e foto em `docs/`.

**Por que assim:**

1. **3,3 V, não 5 V.** A saída AO nunca passa da alimentação. Com 5 V ela poderia passar dos ~3,3 V que o pino do ESP32 aceita e danificá-lo.
2. **Só a AO.** A DO só diz "seco/molhado" (pelo potenciômetro). O percentual precisa de um valor contínuo.
3. **GPIO 34 e 35 (ADC1).** O ADC2 não funciona com o Wi-Fi ligado.
4. **Sensor alimentado pelo GPIO 23 só durante a leitura.** Com corrente o tempo todo, as hastes sofrem eletrólise e corroem em semanas. Ligando o sensor só por alguns milissegundos a cada 60 s, a haste fica sem corrente mais de 99 % do tempo.
5. **O que não pode molhar:** o módulo e as conexões no garfo. Só as hastes vão na terra, e a placa fica acima do vaso.

⏳ Justificar resistivo vs. capacitivo (durabilidade e corrosão).

## Calibração

A leitura depende do solo, da compactação e da profundidade, então `solo_pct` é um **percentual relativo** entre dois pontos medidos no próprio solo, e não umidade volumétrica. Neste sensor o bruto **cai** quando a umidade sobe, por isso a subtração é `seco − bruto`:

```
solo_pct = (seco − bruto) / (seco − molhado) × 100     (limitado a 0–100)
```

Exemplo, sensor B: bruto 4095 → 0 %; 2965 → (4095 − 2965) / 2715 ≈ 42 %; 1380 → 100 %.

**Procedimento do 100 %:** regar até escorrer pelo fundo e esperar 10 min. **0 %:** ⏳ medir com terra seca.

| Sensor | Seco (0 %) | Molhado (100 %) | Data |
|---|---|---|---|
| A | 4095 *(provisório)* | 1412 | 16/11/2026 |
| B | 4095 *(provisório)* | 1380 | 16/11/2026 |

> O seco provisório é o fundo de escala do ADC, porque não havia terra seca. ⚠️ O molhado de A está anotado como 1412, mas no firmware está 1410: conferir.

## Faixas e secagem

| Faixa | `solo_pct` |
|---|---|
| Seco | ⏳ |
| Ideal | ⏳ |
| Encharcado | ⏳ |

**"Secando":** ⏳ critério a definir. Deve usar médias por hora, e não leituras isoladas (o `/secagem` compara a última hora com a anterior).

## MQTT

Leituras em `inatel/gustavos/dados` a cada 60 s (QoS 0, não retida). Status em `inatel/gustavos/status`: `online` ao conectar e `offline` pelo Last Will (retida).

```json
{
  "dispositivo": "esp32-gustavos",
  "projeto": "solo",
  "leituras": {
    "solo_bruto": 2430, "solo_pct": 62.0,
    "solo_bruto_b": 2343, "solo_pct_b": 64.5
  }
}
```

O sensor A publica as grandezas padrão (`solo_bruto`, `solo_pct`); o B publica as extras (`_b`). Faixas válidas: bruto de 0 a 4095 e pct de 0 a 100. O horário é registrado pelo servidor.

## Como executar

**Firmware:** abrir `firmware/` no VS Code com PlatformIO, copiar `include/secrets.example.h` para `include/secrets.h` e preencher Wi-Fi e broker, gravar, e abrir o monitor serial a 115200 baud.

**Broker, coletor, banco, API e Grafana:** ⏳ a documentar em cada etapa.

Credenciais reais (`secrets.h`, `.env`, `broker/passwd`) ficam fora do Git, com arquivos de exemplo no lugar.

## Decisões técnicas

- **Laço sem `delay()` longo** (usa `millis()`): Wi-Fi, MQTT e leitura convivem sem travar.
- **Reconexão própria:** Wi-Fi a cada 10 s e MQTT a cada 5 s, cada parte provada separadamente.
- **Média de 20 amostras:** reduz o ruído do ADC.
- **PubSubClient + ArduinoJson:** bibliotecas mais usadas e documentadas no ESP32.
- **Dois sensores no mesmo payload:** permite comparar A e B sem quebrar o padrão, já que A usa os nomes padrão.
- Demais decisões (Docker, ACL, Grafana via API, horário no servidor) seguem o escopo do curso.

## Testes

**Bancada 1 — média:** o sensor B variou só entre 2959 e 2969 (≈ 0,25 % da escala). Comprova o R1.

**Bancada 2 — molhado vs. bem molhado:** ~2965 contra ~1380. A leitura muda muito com a quantidade de água, por isso o 100 % segue sempre o mesmo procedimento.

**Plano de testes** (detalhes e evidências em `docs/plano-de-testes.md` ⏳):

| # | Caso | Esperado | Resultado |
|---|---|---|---|
| 1 | Regar o vaso | Percentual sobe; faixa muda | ⏳ |
| 2 | Secar por 2+ dias | Gráfico cai; "secando" ativo | ⏳ |
| 3 | Sensor fora da terra | ~0 %; explicar como diferenciar de solo seco | ⏳ |
| 4 | Profundidades diferentes | Documentar a diferença | ⏳ |
| 5 | Wi-Fi cai por 1 min | ESP reconecta sozinho | ⏳ |
| 6 | `docker compose restart` no broker | ESP e coletor reconectam | ⏳ |
| 7 | Sem usuário / publicar com `leitor-gustavos` | Recusado | ⏳ |
| 8 | Publicar `"solo_pct": 150` | Coletor descarta com motivo | ⏳ |
| 9 | Reiniciar a VM | Broker sobe sozinho; o resto volta | ⏳ |
| 10 | Desligar o ESP | "Sem dados" no Grafana em até 3 min | ⏳ |

## Problemas encontrados

| Data | Problema | Solução |
|---|---|---|
| 16/11/2026 | Sem terra seca para calibrar o 0 % | Seco provisório = 4095; confirmar depois |

## Marcos

- [ ] **M1:** ESP publica a cada 60 s, calibrado, com Last Will
- [ ] **M2:** broker protegido em container; coletor aceita e descarta corretamente
- [ ] **M3:** leituras gravadas e consulta de secagem com dados reais
- [ ] **M4:** quatro endpoints e `/secagem` pela rede
- [ ] **M5:** Grafana com medidor, faixas, gráfico e "secando"
- [ ] **M6:** ciclo de secagem documentado, README completo e apresentação
