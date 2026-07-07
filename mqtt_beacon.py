import json
import threading
import time

import certifi
import numpy as np
import paho.mqtt.client as mqtt

from trilateracao import trilateracao3d

# ============================================================
# CONFIGURAÇÕES
# ============================================================

HOST = "mqtt.janks.dev.br"
PORTA = 8883

NOME_USUARIO = "aula"
SENHA = "zowmad-tavQez"

NUMERO_ESPS = 4
MINIMO_ESPS = 3
TIMEOUT = 30  # 20s de medição no ESP + 10s de margem para o envio/round-trip do MQTT

BEACON_IDS = [
    "51:00:23:11:04:6d",
    "51:00:23:11:04:38",
]

TOPICO_LOCALIZACAO = "A1/esp32/localizacao"

TOPICOS_DISTANCIA = [
    f"A1/esp32/{i}/distancia"
    for i in range(1, NUMERO_ESPS + 1)
]

INDICES = {
    topico: i
    for i, topico in enumerate(TOPICOS_DISTANCIA)
}

# ============================================================
# ESTADO
# ============================================================

lock = threading.Lock()

BEACON_ATUAL = None

ready = [False] * NUMERO_ESPS
distancias = [0.0] * NUMERO_ESPS

posicoes_esp32 = np.array([
    [0, 0, 0],
    [10, 0, 0],
    [0, 10, 0],
    [0, 0, 10]
])

# ============================================================


def numero_respostas():
    with lock:
        return sum(ready)


def resetar_estado():
    with lock:
        ready[:] = [False] * NUMERO_ESPS
        distancias[:] = [0.0] * NUMERO_ESPS


def enviar_para_banco_dados(client, posicao, beacon_id):
    payload = json.dumps({
        "posicao_x": float(posicao[0]),
        "posicao_y": float(posicao[1]),
        "posicao_z": float(posicao[2]),
        "beacon_id": beacon_id,
    })

    client.publish(TOPICO_LOCALIZACAO, payload=payload, qos=2)

    print("Localização publicada:", payload)


def imprime_informacoes():

    with lock:

        print("=" * 60)
        print("Beacon:", BEACON_ATUAL)

        for i in range(NUMERO_ESPS):

            print(
                f"ESP{i+1}: "
                f"Ready={ready[i]} "
                f"Dist={distancias[i]}"
            )

        print(
            f"Recebidos: {sum(ready)}/{NUMERO_ESPS}"
        )

        print("=" * 60)


# ============================================================


def on_connect(client, userdata, flags, reason_code, properties):

    print("MQTT conectado.")

    for topico in TOPICOS_DISTANCIA:
        client.subscribe(topico)


def on_message(client, userdata, msg):

    global BEACON_ATUAL

    if msg.topic not in INDICES:
        return

    try:

        dados = json.loads(msg.payload.decode())

        beacon_id = dados["beacon_id"]
        distancia = float(dados["distancia"])

    except (json.JSONDecodeError, KeyError, ValueError):

        print("Mensagem inválida recebida.")

        return

    if beacon_id != BEACON_ATUAL:
        return

    indice = INDICES[msg.topic]

    with lock:

        ready[indice] = True
        distancias[indice] = distancia

    print(f"ESP {indice+1} respondeu ({distancia:.2f} m)")


# ============================================================


def notificar_esps(client, beacon_id):

    for esp in range(1, NUMERO_ESPS + 1):

        client.publish(
            f"A1/esp32/{esp}/{beacon_id}",
            payload=".",
            qos=1
        )

        print(f"Solicitado ESP {esp}")


# ============================================================


def main():

    global BEACON_ATUAL

    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)

    client.tls_set(certifi.where())

    client.username_pw_set(
        NOME_USUARIO,
        SENHA
    )

    client.on_connect = on_connect
    client.on_message = on_message

    client.connect(
        HOST,
        PORTA
    )

    client.loop_start()

    while True:

        for beacon in BEACON_IDS:

            print()
            print("=" * 60)
            print("Localizando", beacon)
            print("=" * 60)

            BEACON_ATUAL = beacon

            resetar_estado()

            notificar_esps(client, beacon)

            inicio = time.time()

            while True:

                if numero_respostas() >= MINIMO_ESPS:
                    break

                if time.time() - inicio > TIMEOUT:
                    break

                time.sleep(0.01)

            imprime_informacoes()

            if numero_respostas() < MINIMO_ESPS:

                print("Poucas respostas. Ignorando.")

                continue

            with lock:

                posicoes = []
                dist = []

                for i in range(NUMERO_ESPS):

                    if ready[i]:

                        posicoes.append(posicoes_esp32[i])
                        dist.append(distancias[i])

            posicao = trilateracao3d(
                np.array(posicoes),
                np.array(dist)
            )

            enviar_para_banco_dados(
                client,
                posicao,
                beacon
            )

            print("Posição:", posicao)

            time.sleep(2)

        time.sleep(1)


if __name__ == "__main__":
    main()