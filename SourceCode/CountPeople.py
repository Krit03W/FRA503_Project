import tkinter as tk
from tkinter import Label
import cv2
from ultralytics import YOLO
from PIL import Image, ImageTk
import paho.mqtt.client as mqtt
import base64  # For encoding image to base64
import os
import time
import pandas as pd


class PeopleCounterApp:
    def __init__(self, root):
        self.root = root
        self.root.title("People Counter Application")
        self.root.geometry("800x600")

        # Load YOLOv8 model
        self.model = YOLO("yolov8n.pt")

        # Initialize video capture
        self.cap = cv2.VideoCapture(1)  # Camera index: 1 is the second camera
        if not self.cap.isOpened():
            raise Exception("Error: Unable to access the camera")

        # Create labels for display
        self.count_label = Label(self.root, text="People count: 0", font=("Arial", 16))
        self.count_label.pack()
        self.video_label = Label(self.root)
        self.video_label.pack()

        # List for tracking counts and calculating averages
        self.person_counts = []
        self.average_label = Label(self.root, text="Average People count: 0", font=("Arial", 16))
        self.average_label.pack()

        # MQTT Settings
        try:
            self.mqtt_client = mqtt.Client()
            self.mqtt_broker = "broker.hivemq.com"  # HiveMQ public broker
            self.mqtt_port = 1883  # Default non-SSL port for HiveMQ
            self.mqtt_topic_data = "camera_detect_topic/6538"
            self.mqtt_topic_image = "camera_detect_topic/image1"  # Topic for images
            self.temp_sensor_topic = "temp_sensor_topic/6552"  # Temperature sensor topic
            self.humidity_sensor_topic = "humidity_sensor_topic/6552"  # Humidity sensor topic

            # Set MQTT callbacks
            self.mqtt_client.on_message = self.on_message
            self.mqtt_client.on_connect = self.on_connect
            self.mqtt_client.on_disconnect = self.on_disconnect

            self.mqtt_client.connect(self.mqtt_broker, self.mqtt_port, 60)
            self.mqtt_client.loop_start()  # Start MQTT loop for continuous communication
        except Exception as e:
            print(f"MQTT Connection Error: {e}")

        # Directory to save images
        self.image_dir = os.path.join(os.getcwd(), "image")
        os.makedirs(self.image_dir, exist_ok=True)
        self.last_save_time = time.time()  # Track last image save time

        # Excel file setup
        self.excel_file = os.path.join(os.getcwd(), "average_count.xlsx")
        if not os.path.exists(self.excel_file):
            # Create a new Excel file with headers if it doesn't exist
            df = pd.DataFrame(columns=["Timestamp", "Average Count", "Temp Sensor", "Humidity Sensor"])
            df.to_excel(self.excel_file, index=False, engine="openpyxl")

        # Variables to store the latest temperature and humidity values
        self.latest_temperature = None
        self.latest_humidity = None

        # Start updating frames, counts, and saving to Excel
        self.update_frame()
        self.update_count()
        self.save_to_excel()

    def count_nearby_people(self, person_boxes):
        """Simply count each bounding box as a person."""
        return len(person_boxes)

    def update_frame(self):
        """Update the video frame and display it in the UI."""
        ret, frame = self.cap.read()
        if ret:
            # Run YOLO inference
            results = self.model(frame)
            person_boxes = []

            # Process detections
            for result in results[0].boxes:
                class_id = int(result.cls)
                x1, y1, x2, y2 = map(int, result.xyxy[0])

                if class_id == 0:  # Person
                    person_boxes.append((x1, y1, x2, y2))
                    cv2.rectangle(frame, (x1, y1), (x2, y2), (0, 255, 0), 2)
                    cv2.putText(frame, "Person", (x1, y1 - 10), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 0), 2)

            # Update count labels
            person_count = self.count_nearby_people(person_boxes)
            self.count_label.config(text=f"People count: {person_count}")

            # Save the frame to the 'image' folder every 5 seconds
            current_time = time.time()
            if current_time - self.last_save_time >= 5:
                image_path = os.path.join(self.image_dir, f"frame_{int(current_time)}.jpg")
                cv2.imwrite(image_path, frame)
                print(f"Image saved to: {image_path}")
                self.last_save_time = current_time

            # Publish the current frame as an image
            _, buffer = cv2.imencode('.jpg', frame)
            image_base64 = base64.b64encode(buffer).decode('utf-8')
            self.mqtt_client.publish(self.mqtt_topic_image, image_base64)

            # Update the frame in the UI
            frame = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
            img = Image.fromarray(frame)
            imgtk = ImageTk.PhotoImage(image=img)
            self.video_label.imgtk = imgtk
            self.video_label.configure(image=imgtk)
        else:
            print("Warning: Unable to read frame from camera")

        # Schedule the next frame update
        self.root.after(500, self.update_frame)

    def update_count(self):
        """Calculate and update people counts."""
        ret, frame = self.cap.read()
        if ret:
            # Run YOLO inference
            results = self.model(frame)
            person_boxes = [result.xyxy[0] for result in results[0].boxes if int(result.cls) == 0]

            # Count people
            person_count = self.count_nearby_people(person_boxes)
            self.person_counts.append(person_count)

            # Calculate average people count
            average_count = sum(self.person_counts[-25:]) / len(self.person_counts[-25:])  # Limit to last 25 frames
            self.average_label.config(text=f"Average People count (25s): {average_count:.2f}")

            # Publish data to MQTT
            self.mqtt_client.publish(self.mqtt_topic_data, average_count)
        else:
            print("Warning: Unable to update count (No frame)")

        # Schedule the next count update
        self.root.after(1000, self.update_count)

    def save_to_excel(self):
        """Save the average count, temperature, and humidity to the Excel file every 5 seconds."""
        if self.person_counts:
            # Calculate the current average count
            average_count = sum(self.person_counts[-25:]) / len(self.person_counts[-25:])
            timestamp = time.strftime("%Y-%m-%d %H:%M:%S")
            temperature = self.latest_temperature if self.latest_temperature is not None else "N/A"
            humidity = self.latest_humidity if self.latest_humidity is not None else "N/A"

            # Append the data to the Excel file
            new_data = pd.DataFrame({
                "Timestamp": [timestamp],
                "Average Count": [average_count],
                "Temp Sensor": [temperature],
                "Humidity Sensor": [humidity]
            })
            existing_data = pd.read_excel(self.excel_file, engine="openpyxl")
            updated_data = pd.concat([existing_data, new_data], ignore_index=True)
            updated_data.to_excel(self.excel_file, index=False, engine="openpyxl")
            print(f"Data saved to Excel: {timestamp}, Average Count: {average_count}, Temp Sensor: {temperature}, Humidity Sensor: {humidity}")

        # Schedule the next Excel save
        self.root.after(5000, self.save_to_excel)

    def on_connect(self, client, userdata, flags, rc):
        """Callback for successful connection to MQTT broker."""
        if rc == 0:
            print("Connected to MQTT broker")
            client.subscribe([(self.temp_sensor_topic, 0), (self.humidity_sensor_topic, 0)])
        else:
            print(f"Failed to connect to MQTT broker. Return code: {rc}")

    def on_disconnect(self, client, userdata, rc):
        """Callback for disconnection from MQTT broker."""
        print("Disconnected from MQTT broker. Reconnecting...")
        client.reconnect()

    def on_message(self, client, userdata, msg):
        """Callback for processing received MQTT messages."""
        if msg.topic == self.temp_sensor_topic:
            try:
                self.latest_temperature = float(msg.payload.decode())
                print(f"Received Temperature: {self.latest_temperature}")
            except ValueError:
                print("Error: Invalid temperature value received.")
        elif msg.topic == self.humidity_sensor_topic:
            try:
                self.latest_humidity = float(msg.payload.decode())
                print(f"Received Humidity: {self.latest_humidity}")
            except ValueError:
                print("Error: Invalid humidity value received.")

    def __del__(self):
        """Release resources."""
        if self.cap.isOpened():
            self.cap.release()
        self.mqtt_client.loop_stop()  # Stop MQTT loop
        self.mqtt_client.disconnect()


# Create the main Tkinter window
root = tk.Tk()
app = PeopleCounterApp(root)
root.mainloop()