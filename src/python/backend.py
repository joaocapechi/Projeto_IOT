import certifi
import cv2
from dotenv import load_dotenv
import json
import numpy as np
import os
import paho.mqtt.client as mqtt
import ssl 
import threading
import time
from trilateracao import trilateracao3d
from ultralytics import YOLO


load_dotenv()

'''
============================================================
                       Image Processing
============================================================
'''
current_image = None
model = YOLO("yolov8n.pt")

def get_dominant_color(image_roi):
    """Finds the dominant color and consistently returns (color_name, bgr_tuple)."""
    small_roi = cv2.resize(image_roi, (1, 1), interpolation=cv2.INTER_AREA)
    b, g, r = small_roi[0, 0]
    
    # Safely convert numpy values to standard Python integers
    b, g, r = int(b), int(g), int(r)
    
    # Classify basic colors and map them to their distinct BGR values for drawing
    if r > 180 and g > 180 and b > 180: 
        return "White", (255, 255, 255)
    if r < 50 and g < 50 and b < 50: 
        return "Black", (0, 0, 0)
    if r > g and r > b: 
        return "Red", (0, 0, 255)      # OpenCV uses BGR
    if g > r and g > b: 
        return "Green", (0, 255, 0)
    if b > r and b > g: 
        return "Blue", (255, 0, 0)
    
    # Fallback if it's an unclassified color mix
    return f"RGB({r},{g},{b})", (b, g, r)


'''
============================================================
                        CONFIGURAÇÕES
============================================================
'''
NUMERO_ESPS = 4
MINIMO_ESPS = 3
TIMEOUT = 30  # 20s de medição no ESP + 10s de margem para o envio/round-trip do MQTT

BEACON_IDS = [
    "51:00:23:11:04:6d",
    "51:00:23:11:04:38",
]

COMPRIMENTO_SALA = 15.5
LARGURA_SALA = 3.3

TOPICO_LOCALIZACAO = "A1/esp32/localizacao"
TOPICO_ALERTA = "A1/esp32/alerta"

TOPICOS_DISTANCIA = [
    f"A1/esp32/{i}/distancia"
    for i in range(1, NUMERO_ESPS + 1)
]

INDICES = {
    topico: i
    for i, topico in enumerate(TOPICOS_DISTANCIA)
}


'''
============================================================
                            Estado
============================================================
'''
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


def numero_respostas():
    with lock:
        return sum(ready)


def resetar_estado():
    with lock:
        ready[:] = [False] * NUMERO_ESPS
        distancias[:] = [0.0] * NUMERO_ESPS


def notificar_esps(client, beacon_id):
    for esp in range(1, NUMERO_ESPS + 1):
        client.publish(
            f"A1/esp32/{esp}/{beacon_id}",
            payload=".",
            qos=1
        )

        print(f"Solicitado ESP {esp}")


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


'''
============================================================
                            MQTT
============================================================
'''
HOST = "mqtt.janks.dev.br"
PORTA = 8883
USERNAME = "aula"
PASSWORD = "zowmad-tavQez"


def on_connect(client, userdata, flags, rc, properties=None):
    print("Connected to MQTT Broker successfully!")
    # Subscribe to your topics
    client.subscribe("A1/esp32/camera")
    client.subscribe("A1/esp32/camera/qtd")
    for topico in TOPICOS_DISTANCIA:
        client.subscribe(topico)


def enviar_para_banco_dados(client, posicao, beacon_id):
    payload = json.dumps({
        "posicao_x": float(posicao[0]),
        "posicao_y": float(posicao[1]),
        "posicao_z": float(posicao[2]),
        "beacon_id": beacon_id,
    })

    client.publish(TOPICO_LOCALIZACAO, payload=payload, qos=2)

    print("Localização publicada:", payload)


def on_message(client, userdata, msg):
    global current_image
    global BEACON_ATUAL
    
    # 1. Handle Incoming Photo
    if msg.topic == "A1/esp32/camera":
        # Convert raw byte payload into a numpy array
        np_buffer = np.frombuffer(msg.payload, dtype=np.uint8)
        # Decode the numpy array into an OpenCV BGR image
        current_image = cv2.imdecode(np_buffer, cv2.IMREAD_COLOR)

        if current_image is not None:
            print("Image received and buffered")
    # 2. Handle Image Processing Bounding Box
    elif msg.topic == "A1/esp32/camera/qtd":
        if current_image is None:
            print("No image available")
        
        qtd = int(msg.payload.decode())

        if qtd > 0:
            results = model(current_image, classes=0, verbose=False)

            for result in results:
                boxes = result.boxes
                for box in boxes:
                    # Get bounding box coordinates
                    x1, y1, x2, y2 = map(int, box.xyxy[0])
                    
                    # --- Shirt Localization ---
                    # A shirt typically resides in the upper-middle region of a person's bounding box
                    person_height = y2 - y1
                    shirt_y1 = y1 + int(person_height * 0.2) # Skip the head/face
                    shirt_y2 = y2 # y1 + person_height
                    
                    # Crop the shirt ROI (Region of Interest)
                    shirt_roi = current_image[shirt_y1:shirt_y2, x1:x2]
                    
                    if shirt_roi.size > 0:
                        # 1. Properly unpack the string name and the BGR tuple
                        color_name, box_color = get_dominant_color(shirt_roi)
                        print(f"Detected person with {color_name} shirt.")
                        
                        # 2. Draw the bounding box using the BGR tuple
                        cv2.rectangle(
                            current_image, 
                            (x1, y1), 
                            (x2, y2), 
                            box_color,
                            2
                        )
                        
                        if color_name in ["Black", "Blue"]:
                            # client.publish("A1/esp32/camera/alerta", payload="Alerta", qos=2)
                            print("*" * 25)
                            print("Alerta".center(25,"*"))
                            print("*" * 25)
                    
        
        # --- SAVE THE IMAGE HERE ---
        # This saves the image in the same folder as your Python script
        cv2.imwrite("detected_person.jpg", current_image)
    # 3. Handle beacons
    elif msg.topic in INDICES:
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


'''
============================================================
                            Main
============================================================
'''
def main():
    global BEACON_ATUAL

    client = mqtt.Client()
    client.tls_set(certifi.where())
    client.username_pw_set(username=USERNAME, password=PASSWORD)
    client.on_connect = on_connect
    client.on_message = on_message

    client.connect(HOST, PORTA, 100)

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
                dist = [0, 0, 0, 0]

                for i in range(NUMERO_ESPS):
                    if ready[i]:
                        dist[i] = distancias[i]

            posicao = trilateracao3d(
                np.array(posicoes_esp32),
                np.array(dist)
            )

            enviar_para_banco_dados(
                client,
                posicao,
                beacon
            )
            
            # verificando se a posicao esta dentro oui fora da sala:
            if ((posicao[0] > LARGURA_SALA or posicao[0] < 0) or (posicao[1] > COMPRIMENTO_SALA or posicao[1] < 0)):
                print("Alerta enviado para o telegram do dono")
                client.publish(topic=TOPICO_ALERTA, payload="SAIU DA SALA", qos=2)

            print("Posição:", posicao)

            time.sleep(2)

        time.sleep(1)


if __name__ == "__main__":
    main()
    
