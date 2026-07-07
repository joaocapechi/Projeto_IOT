import paho.mqtt.client as mqtt
import ssl
import certifi
import json
import time
import numpy as np
from trilateracao import trilateracao3d
import json

#CONSTANTES:
HOST = "mqtt.janks.dev.br"
PORTA = 8883
NUMERO_ESPS = 4
BEACON_IDS = ["51:00:23:11:04:6d", "51:00:23:11:04:38"]

NOME_USUARIO = "aula"
SENHA = "zowmad-tavQez"

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

def enviar_para_banco_dados(client, posicao, beacon_id):
    posicao_x = posicao[0]
    posicao_y = posicao[1]
    posicao_z = posicao[2]

    dados = {
        "posicao_x": float(posicao_x),
        "posicao_y": float(posicao_y),
        "posicao_z": float(posicao_z),
        "beacon_id": beacon_id,
    }

    payload = json.dumps(dados)

    client.publish(f"A1/esp32/localizacao", payload=payload, qos=2)

    print(f"Mensagem enviada para o banco de dados. Dados enviados: {dados}")

def imprime_informacoes(client=None, titulo="DEBUG"):
    print("\n" + "=" * 70)
    print(f" {titulo}")
    print("=" * 70)

    print("\n--- MQTT ---")
    print(f"Host: {HOST}")
    print(f"Porta: {PORTA}")
    print(f"Usuário: {NOME_USUARIO}")
    if client:
        print(f"MQTT conectado: {client.is_connected()}")
    else:
        print("Cliente MQTT: não informado")

    print("\n--- BEACON ---")
    print(f"Beacon atual: {BEACON_ATUAL}")
    print(f"Lista de beacons: {BEACON_IDS}")

    print("\n--- ESP32 ---")
    print(f"Quantidade ESPs: {NUMERO_ESPS}")

    for i in range(NUMERO_ESPS):
        print(f"\nESP {i+1}")
        print(f"  Posição: {posicoes_esp32[i]}")
        print(f"  Ready: {ready[i]}")
        print(f"  Distância: {distancias[i]}")

    print("\n--- STATUS GERAL ---")
    print(f"Todos prontos: {all(ready)}")
    print(f"Ready vetor: {ready}")
    print(f"Distâncias: {distancias}")

    print("\n--- TÓPICOS ---")
    for topico, indice in INDICES.items():
        print(f"{topico} -> ESP {indice+1}")

    print("\n--- MATRIZ DE POSIÇÕES ---")
    print(posicoes_esp32)

    print("=" * 70 + "\n")


def on_connect(client, userdata, flags, reason_code, properties):
    print("Conectado!")

    client.subscribe("A1/esp32/1/distancia")
    client.subscribe("A1/esp32/2/distancia")
    client.subscribe("A1/esp32/3/distancia")
    client.subscribe("A1/esp32/4/distancia")

def on_message(client, userdata, msg):
    global BEACON_ATUAL

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
        #if not ready[i]:
        print(f"Notifiquei o esp {i+1}")
        mqtt_client.publish(f"A1/esp32/{i+1}/{beacon_id}", payload=".", qos=1)   

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
                    timeout = True
                    break

                #notifica_todos_esps_pendentes(mqtt_client=client, beacon_id=beacon_id)
                #time.sleep(0.5)

            imprime_informacoes(client)
            if timeout:                
                continue


            posicao = trilateracao3d(posicoes_esp32, distancias)

            enviar_para_banco_dados(client=client, posicao=posicao, beacon_id=beacon_id)

            print(beacon_id, posicao)

            time.sleep(5)
        time.sleep(1)

if __name__ == "__main__":
    main()