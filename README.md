# projeto-iot-gustavos

1. Alimentar o sensor com 3,3 V, e não com 5 V.
O módulo funciona de 3,3 V a 5 V. A saída AO nunca passa da tensão de alimentação. Com 5 V, ela poderia chegar a 5 V, acima dos ~3,3 V que o pino do ESP32 aceita, e isso danificaria o ESP32. Com 3,3 V, a saída fica sempre dentro do limite seguro.

2. Usar só a saída AO e deixar a DO desligada.
A DO só diz “seco” ou “molhado”, conforme o potenciômetro. O projeto precisa de um valor contínuo para calcular o percentual, e esse valor vem da AO.4. Alimentar o sensor por um pino (GPIO 23), e não direto no 3V3.

4. Alimentar o sensor por um pino (GPIO 23), e não direto no 3V3.
Esta é a resposta para “como reduzir a corrosão?”. Com corrente passando o tempo todo, as hastes sofrem eletrólise e corroem em poucas semanas. Como lemos a cada 60 s, o ESP32 pode ligar o sensor só durante a leitura, por alguns milissegundos, e desligar em seguida. Assim a haste fica sem corrente mais de 99 % do tempo. O módulo consome poucos miliamperes, e um pino do ESP32 fornece isso sem problema.

5. O que não pode molhar: a placa pequena e as conexões dos fios no garfo. Só a parte das hastes vai na terra. Deixe a placa acima do vaso, longe dos respingos da rega.