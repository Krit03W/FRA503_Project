import cv2
import numpy as np
import requests
import paho.mqtt.client as mqtt
import time

# ESP32-CAM URL
camera_url = "http://172.20.10.5/capture"

# MQTT Configuration
mqtt_broker = "broker.hivemq.com"
mqtt_port = 1883
mqtt_topic = "esp_cam_air/6552"

# Initialize MQTT Client
def on_connect(client, userdata, flags, rc):
    """Callback for successful connection to MQTT broker."""
    if rc == 0:
        print("Connected to MQTT broker")
    else:
        print(f"Failed to connect to MQTT broker. Return code: {rc}")

def on_disconnect(client, userdata, rc):
    """Callback for disconnection from MQTT broker."""
    print("Disconnected from MQTT broker. Reconnecting...")
    client.reconnect()

mqtt_client = mqtt.Client("ESP32_CAM_Client")
mqtt_client.on_connect = on_connect
mqtt_client.on_disconnect = on_disconnect

try:
    mqtt_client.connect(mqtt_broker, mqtt_port, 60)
    mqtt_client.loop_start()  # Start MQTT loop for continuous communication
except Exception as e:
    print(f"MQTT Connection Error: {e}")
    exit()

def detect_green_light(image):
    """
    Detects green light in the image.
    """
    # Convert the image to HSV
    hsv = cv2.cvtColor(image, cv2.COLOR_BGR2HSV)

    # Define the green color range in HSV
    lower_green = np.array([40, 70, 70])
    upper_green = np.array([80, 255, 255])

    # Create a mask for green areas
    mask = cv2.inRange(hsv, lower_green, upper_green)
    green_detected = cv2.countNonZero(mask) > 50  # Threshold for detection

    # Uncomment for debugging
    # cv2.imshow("Green Mask", mask)

    return green_detected

while True:
    try:
        # Fetch the image from ESP32-CAM
        response = requests.get(camera_url, timeout=10)
        response.raise_for_status()

        # Decode the image
        img_array = np.asarray(bytearray(response.content), dtype=np.uint8)
        img = cv2.imdecode(img_array, cv2.IMREAD_COLOR)

        if img is not None:
            # Detect green light in the image
            if detect_green_light(img):
                status = "ON"
            else:
                status = "OFF"

            # Publish to MQTT topic
            mqtt_client.publish(mqtt_topic, status)
            print(f"Published '{status}' to {mqtt_topic}")

            # Display the live stream
            cv2.imshow("ESP32-CAM Live Stream", img)

            if cv2.waitKey(1) & 0xFF == ord('q'):
                break
        else:
            print("Failed to decode the image.")

    except requests.exceptions.RequestException as e:
        print(f"Error fetching image from ESP32-CAM: {e}")

    # Wait for 5 seconds before the next capture
    time.sleep(5)

# Cleanup
cv2.destroyAllWindows()
mqtt_client.loop_stop()  # Stop MQTT loop
mqtt_client.disconnect()
