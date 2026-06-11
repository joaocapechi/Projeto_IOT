import paho.mqtt.client as mqtt
import ssl
import certifi
import json
import time
import numpy as np
from trilateracao import trilateracao3d

#CONSTANTES:
HOST = "mqtt.janks.dev.br"
PORTA = 8883
NUMERO_ESPS = 4
BEACON_IDS = ["fda50693-a4e2-4fb1-afcf-c6eb07647825", "BBB-CCC-DDD-EEE-ASASASASA"]

NOME_USUARIO = "username"
SENHA = "SENHA"


BEACON_ATUAL = BEACON_IDS[0]
ready = [False] * NUMERO_ESPS
distancias = [0] * NUMERO_ESPS

posicoes_esp32 = np.array([
    [0, 0, 0],
    [10, 0, 0],
    [0, 10, 0],
    [0, 0, 10]
])

#recebo
INDICES = {
    "A1/esp32/1/distancia": 0,
    "A1/esp32/2/distancia": 1,
    "A1/esp32/3/distancia": 2,
    "A1/esp32/4/distancia": 3,
    }

# ----------------------------------------------------------------------------------#

def on_connect(client, userdata, flags, reason_code, properties):
    print("Conectado!")

    client.subscribe("A1/esp32/1/distancia")
    client.subscribe("A1/esp32/2/distancia")
    client.subscribe("A1/esp32/3/distancia")
    client.subscribe("A1/esp32/4/distancia")

def on_message(client, userdata, msg):

    topico = msg.topic
    mensagem = msg.payload.decode()
    #print("Tópico:", topico)
    #print("Mensagem:", mensagem)

    dados = json.loads(mensagem)

    beacon_id = dados["beacon_id"]
    distancia = dados["distancia"]

    if topico not in INDICES:
        return

    if beacon_id != BEACON_ATUAL:
        return

    indice = INDICES[topico]

    ready[indice] = True
    distancias[indice] = float(distancia)

def notifica_todos_esps_pendentes(mqtt_client, beacon_id):
    for i in range(NUMERO_ESPS):
        if not ready[i]:
            mqtt_client.publish(f"A1/esp32/{i+1}/{beacon_id}", payload=".", qos=2)   

def main():
    global BEACON_ATUAL

    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    client.tls_set(certifi.where())

    client.username_pw_set(
    username=NOME_USUARIO,
    password=SENHA
    )

    client.on_connect = on_connect
    client.on_message = on_message

    client.connect(
        host=HOST,
        port=PORTA
    )

    client.loop_start()

    while True:
        for beacon_id in BEACON_IDS:

            BEACON_ATUAL = beacon_id

            ready[:] = [False] * NUMERO_ESPS

            timeout = False

            inicio = time.time()
            notifica_todos_esps_pendentes(mqtt_client=client, beacon_id=beacon_id)

            while not all(ready):
                if time.time() - inicio > 10:
                    print("Timeout")
                    break

                #notifica_todos_esps_pendentes(mqtt_client=client, beacon_id=beacon_id)
                #time.sleep(0.5)

            if timeout:
                continue

            posicao = trilateracao3d(posicoes_esp32, distancias)

            print(beacon_id, posicao)

            time.sleep(5)

if __name__ == "__main__":
    main()