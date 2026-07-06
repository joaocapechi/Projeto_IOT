import certifi
import cv2
from dotenv import load_dotenv
import numpy as np
import os
import paho.mqtt.client as mqtt
import ssl 
from ultralytics import YOLO

load_dotenv()

# Global state to hold the latest image
current_image = None
model = YOLO("yolov8n.pt")

# MQTT login
USERNAME = os.getenv("username")
PASSWORD = os.getenv("password")

def on_connect(client, userdata, flags, rc, properties=None):
    print("Connected to MQTT Broker successfully!")
    # Subscribe to your topics
    client.subscribe("A1/esp32/camera")
    client.subscribe("A1/esp32/camera/qtd")


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


def on_message(client, userdata, msg):
    global current_image
    
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
                            client.publish("A1/camera/alerta", payload="Alerta", qos=2)
                    
        
        # --- SAVE THE IMAGE HERE ---
        # This saves the image in the same folder as your Python script
        cv2.imwrite("detected_person.jpg", current_image)
   

def main():
    # Initialize client using Paho MQTT v2.x syntax
    client = mqtt.Client()
    client.tls_set(certifi.where())
    client.username_pw_set(username=USERNAME, password=PASSWORD)
    client.on_connect = on_connect
    client.on_message = on_message

    # Connect to your broker (replace with your broker's IP/domain)
    client.connect("mqtt.janks.dev.br", 8883, 100)

    # Start the network loop to listen for messages indefinitely
    client.loop_forever()


if __name__ == "__main__":
    main()