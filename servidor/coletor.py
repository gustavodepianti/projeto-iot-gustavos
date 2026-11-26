"""Coletor: assina os dados do ESP32 no broker, valida e registra cada mensagem."""

import json
import logging
import os
from pathlib import Path

import paho.mqtt.client as mqtt
from dotenv import load_dotenv

PASTA = Path(__file__).parent
load_dotenv(PASTA / ".env")

# ---------- Configuração (vem do .env) ----------
MQTT_HOST = os.getenv("MQTT_HOST", "localhost")
MQTT_PORTA = int(os.getenv("MQTT_PORTA", "1883"))
MQTT_USUARIO = os.getenv("MQTT_USUARIO")
MQTT_SENHA = os.getenv("MQTT_SENHA")
ID_ALUNO = os.getenv("ID_ALUNO", "gustavos")

TOPICO_DADOS = f"inatel/{ID_ALUNO}/dados"
TOPICO_STATUS = f"inatel/{ID_ALUNO}/status"

# Grandezas aceitas e a faixa válida de cada uma
FAIXAS = {
    "solo_bruto": (0, 4095),
    "solo_pct": (0, 100),
    "solo_bruto_b": (0, 4095),   # sensor B (extra)
    "solo_pct_b": (0, 100),      # sensor B (extra)
}
OBRIGATORIAS = ("solo_bruto", "solo_pct")

# ---------- Log: na tela e no arquivo coletor.log ----------
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s %(levelname)-7s %(message)s",
    handlers=[
        logging.StreamHandler(),
        logging.FileHandler(PASTA / "coletor.log", encoding="utf-8"),
    ],
)
log = logging.getLogger("coletor")


class MensagemInvalida(Exception):
    """Erro de validação. O texto do erro é o motivo do descarte."""


def validar(payload: bytes) -> dict:
    """Devolve os dados se a mensagem for válida; senão, levanta MensagemInvalida."""
    try:
        dados = json.loads(payload)
    except ValueError:
        raise MensagemInvalida("JSON invalido")

    if not isinstance(dados, dict):
        raise MensagemInvalida("JSON nao e um objeto")

    for campo in ("dispositivo", "projeto", "leituras"):
        if campo not in dados:
            raise MensagemInvalida(f"campo faltando: {campo}")

    if not isinstance(dados["dispositivo"], str) or not dados["dispositivo"]:
        raise MensagemInvalida("campo 'dispositivo' invalido")

    if dados["projeto"] != "solo":
        raise MensagemInvalida(f"projeto inesperado: {dados['projeto']!r}")

    leituras = dados["leituras"]
    if not isinstance(leituras, dict):
        raise MensagemInvalida("campo 'leituras' nao e um objeto")

    for grandeza in OBRIGATORIAS:
        if grandeza not in leituras:
            raise MensagemInvalida(f"campo faltando: leituras.{grandeza}")

    for grandeza, valor in leituras.items():
        if grandeza not in FAIXAS:
            raise MensagemInvalida(f"grandeza desconhecida: {grandeza}")
        if isinstance(valor, bool) or not isinstance(valor, (int, float)):
            raise MensagemInvalida(f"valor nao numerico: {grandeza}={valor!r}")
        minimo, maximo = FAIXAS[grandeza]
        if not (minimo <= valor <= maximo):
            raise MensagemInvalida(
                f"fora da faixa: {grandeza}={valor} (aceita {minimo} a {maximo})")

    return dados


# ---------- Callbacks do MQTT ----------

def ao_conectar(client, userdata, flags, reason_code, properties):
    if reason_code.is_failure:
        log.error("Conexao recusada pelo broker: %s", reason_code)
        return
    log.info("Conectado ao broker %s:%s como %s", MQTT_HOST, MQTT_PORTA, MQTT_USUARIO)
    # A inscrição fica AQUI para ser refeita a cada reconexão
    client.subscribe([(TOPICO_DADOS, 0), (TOPICO_STATUS, 0)])
    log.info("Inscrito em %s e %s", TOPICO_DADOS, TOPICO_STATUS)


def ao_desconectar(client, userdata, flags, reason_code, properties):
    log.warning("Desconectado do broker (%s); tentando reconectar...", reason_code)


def ao_receber(client, userdata, msg):
    if msg.topic == TOPICO_STATUS:
        log.info("STATUS do ESP32: %s", msg.payload.decode(errors="replace"))
        return

    try:
        dados = validar(msg.payload)
    except MensagemInvalida as motivo:
        log.warning("DESCARTADA (%s): %s", motivo, msg.payload[:200])
        return

    leituras = ", ".join(f"{g}={v}" for g, v in dados["leituras"].items())
    log.info("ACEITA de %s: %s", dados["dispositivo"], leituras)
    # Semana 3: aqui o coletor vai gravar as leituras no MySQL


def main():
    if not MQTT_USUARIO or not MQTT_SENHA:
        raise SystemExit("Defina MQTT_USUARIO e MQTT_SENHA no arquivo .env")

    client = mqtt.Client(
        callback_api_version=mqtt.CallbackAPIVersion.VERSION2,
        client_id=f"coletor-{ID_ALUNO}",
    )
    client.username_pw_set(MQTT_USUARIO, MQTT_SENHA)
    client.on_connect = ao_conectar
    client.on_disconnect = ao_desconectar
    client.on_message = ao_receber
    client.reconnect_delay_set(min_delay=1, max_delay=30)

    log.info("Coletor iniciado; conectando a %s:%s...", MQTT_HOST, MQTT_PORTA)
    client.connect_async(MQTT_HOST, MQTT_PORTA, keepalive=60)
    try:
        client.loop_forever(retry_first_connection=True)
    except KeyboardInterrupt:
        log.info("Coletor encerrado pelo usuario")
        client.disconnect()


if __name__ == "__main__":
    main()