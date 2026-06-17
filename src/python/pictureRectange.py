import json
import cv2
import numpy as np
import paho.mqtt.client as mqtt
import ssl 
import certifi

# Global state to hold the latest image
current_image = None

def on_connect(client, userdata, flags, rc, properties=None):
    print("Connected to MQTT Broker successfully!")
    # Subscribe to your topics
    client.subscribe("A1/esp32/camera")
    client.subscribe("A1/esp32/camera/coord")


def on_message(client, userdata, msg):
    global current_image
    
    # 1. Handle Incoming Photo
    if msg.topic == "A1/esp32/camera":
        # Convert raw byte payload into a numpy array
        np_buffer = np.frombuffer(msg.payload, dtype=np.uint8)
        # Decode the numpy array into an OpenCV BGR image
        current_image = cv2.imdecode(np_buffer, cv2.IMREAD_COLOR)
        print("Image received and buffered.")
        
    # 2. Handle Incoming JSON Bounding Box
    elif msg.topic == "A1/esp32/camera/coord":
        if current_image is None:
            print("Received coordinates, but no image is available yet. Skipping.")
            return
        
        try:
            # Parse the JSON payload
            datas = json.loads(msg.payload.decode('utf-8'))
            annotated_image = current_image.copy()
            for data in datas:
                img_h, img_w = annotated_image.shape[:2]

                scale_x = img_w / data['ei_width']
                scale_y = img_h / data['ei_height']

                x = int(data['x'] * scale_x)
                y = int(data['y'] * scale_y)
                w = int(data['w'] * scale_x)
                h = int(data['h'] * scale_y)

                r = int(data["corCamisa_r"])
                g = int(data["corCamisa_g"])
                b = int(data["corCamisa_b"])
                
                # OpenCV requires top-left (x1, y1) and bottom-right (x2, y2) coordinates
                top_left = (x, y)
                bottom_right = (x + w, y + h)
                
                # Draw the box (Color is BGR: Green is (0, 255, 0), thickness is 2 pixels)
                # Working on a copy so we don't degrade the original buffered image
                
                cv2.rectangle(annotated_image, top_left, bottom_right, (r,g,b), 2)
            
            # Save or process your annotated image
            cv2.imwrite("output_with_box.jpg", annotated_image)
            print("Box successfully drawn! Saved to output_with_box.jpg")
            
        except (json.JSONDecodeError, KeyError, ValueError) as e:
            print(f"Error processing JSON metadata: {e}")

# Initialize client using Paho MQTT v2.x syntax
client = mqtt.Client()
client.tls_set(certifi.where())
client.username_pw_set(username="aula", password="zowmad-tavQez")
client.on_connect = on_connect
client.on_message = on_message

# Connect to your broker (replace with your broker's IP/domain)
client.connect("mqtt.janks.dev.br", 8883, 100)

# Start the network loop to listen for messages indefinitely
client.loop_forever()