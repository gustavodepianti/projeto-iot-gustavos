# projeto-iot-gustavos

1. Alimentar o sensor com 3,3 V, e não com 5 V.
O módulo funciona de 3,3 V a 5 V. A saída AO nunca passa da tensão de alimentação. Com 5 V, ela poderia chegar a 5 V, acima dos ~3,3 V que o pino do ESP32 aceita, e isso danificaria o ESP32. Com 3,3 V, a saída fica sempre dentro do limite seguro.

2. Usar só a saída AO e deixar a DO desligada.
A DO só diz “seco” ou “molhado”, conforme o potenciômetro. O projeto precisa de um valor contínuo para calcular o percentual, e esse valor vem da AO.4. Alimentar o sensor por um pino (GPIO 23), e não direto no 3V3.

4. Alimentar o sensor por um pino (GPIO 23), e não direto no 3V3.
Esta é a resposta para “como reduzir a corrosão?”. Com corrente passando o tempo todo, as hastes sofrem eletrólise e corroem em poucas semanas. Como lemos a cada 60 s, o ESP32 pode ligar o sensor só durante a leitura, por alguns milissegundos, e desligar em seguida. Assim a haste fica sem corrente mais de 99 % do tempo. O módulo consome poucos miliamperes, e um pino do ESP32 fornece isso sem problema.

5. O que não pode molhar: a placa pequena e as conexões dos fios no garfo. Só a parte das hastes vai na terra. Deixe a placa acima do vaso, longe dos respingos da rega.

TESTE 1:
1. A média está funcionando bem. No final, o B variou só entre 2959 e 2969, uma diferença de cerca de 10 em 4095, ou 0,25 %. Sem a média, o ruído do ADC do ESP32 costuma ser bem maior. Isso comprova o requisito R1.

TESTE 2: 
Na medição anterior, a terra “molhada” deu ~2965, e agora a “bem molhada” deu ~1380. Isso mostra que a leitura muda muito conforme o quanto a terra está molhada. Por isso o 100 % precisa seguir sempre o mesmo procedimento: regar até escorrer pelo fundo e esperar 10 minutos

16/11/2026 --> Vou usar o parametro para "seco" como 4095, já que não tenho terra seca no momento =(.

O truque é a ordem da subtração. Fazendo seco − bruto em vez de bruto − seco, a fórmula “inverte” o sentido do sensor. Um exemplo com a terra “molhada” que você mediu antes (2965): (4095 − 2965) / 2715 × 100 ≈ 42 %.

No seco (bruto = 4095): o numerador fica 4095 − 4095 = 0, então o resultado é 0 %.
No molhado (bruto = 1380, no sensor B): o numerador fica 4095 − 1380 = 2715, igual ao denominador. 2715 / 2715 × 100 = 100 %.
No meio do caminho: quanto mais o bruto cai, maior fica o numerador, e mais o percentual sobe.

CALIBRAÇÃO: 
CAL_A_SECO = 4095 - 0%
CAL_A_MOLHADO = 1412 - 100%
CAL_B_SECO = 4095 - 0%
CAL_B_MOLHADO = 1380 - 100%