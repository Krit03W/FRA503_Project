#include <DHT.h>                // ไลบรารีสำหรับเซ็นเซอร์ DHT
#include <ESP32Servo.h>         // ไลบรารีสำหรับควบคุมเซอร์โวมอเตอร์
#include <WiFi.h>               // ไลบรารีสำหรับเชื่อมต่อ WiFi
#include <PubSubClient.h>       // ไลบรารีสำหรับการใช้งาน MQTT

// การตั้งค่าพินของเซ็นเซอร์ DHT
#define DHTPIN 4                // กำหนดพินที่ต่อกับ DHT
#define DHTTYPE DHT22           // ใช้เซ็นเซอร์ DHT22

DHT dht(DHTPIN, DHTTYPE);        // สร้างออบเจ็กต์สำหรับ DHT

// การตั้งค่าพินของเซอร์โว
#define SERVO_PIN 13            // พินที่ต่อเซอร์โวมอเตอร์
Servo myServo;                   // สร้างออบเจ็กต์สำหรับเซอร์โวมอเตอร์

// การตั้งค่า WiFi และ MQTT
const char* ssid = "iPhone krit";       // ชื่อเครือข่าย WiFi
const char* password = "0954312751";    // รหัสผ่าน WiFi
const char* mqtt_server = "broker.hivemq.com";  // โฮสต์ของ MQTT Broker
const int mqtt_port = 1883;                     // พอร์ตของ MQTT Broker

WiFiClient espClient;             // สร้างออบเจ็กต์สำหรับเชื่อมต่อ WiFi
PubSubClient client(espClient);   // สร้างออบเจ็กต์สำหรับใช้งาน MQTT

String currentAirState = "";      // ตัวแปรสำหรับเก็บสถานะปัจจุบันของแอร์

// ฟังก์ชันเชื่อมต่อ WiFi
void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to WiFi...");
  Serial.println(ssid);

  WiFi.begin(ssid, password);   // เริ่มเชื่อมต่อกับ WiFi

  while (WiFi.status() != WL_CONNECTED) {  // ถ้า WiFi ยังไม่เชื่อมต่อ
    delay(1000);
    Serial.print(".");  // แสดงการเชื่อมต่อ
  }

  Serial.println();
  Serial.println("WiFi connected");  // เมื่อเชื่อมต่อสำเร็จ
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());  // แสดง IP ที่เชื่อมต่อกับ WiFi
}

// ฟังก์ชันเชื่อมต่อ MQTT
void reconnect() {
  while (!client.connected()) {  // หากไม่เชื่อมต่อกับ MQTT
    Serial.print("Attempting MQTT connection...");
    String clientId = "ESP32_Client_" + String(random(0xffff), HEX);  // สร้าง client ID แบบสุ่ม
    if (client.connect(clientId.c_str())) {  // พยายามเชื่อมต่อกับ MQTT
      Serial.println("connected");  // เชื่อมต่อสำเร็จ
      // Subscribe หัวข้อ air_condition
      client.subscribe("air_condition/6552");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());  // แสดงข้อผิดพลาด
      Serial.println(" try again in 5 seconds");
      delay(5000);  // รอ 5 วินาทีก่อนลองใหม่
    }
  }
}

// ฟังก์ชัน callback เมื่อได้รับข้อความ MQTT
void callback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];  // แปลง payload เป็นข้อความ
  }

  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.println(message);

  // ตรวจสอบการเปลี่ยนแปลงของสถานะแอร์
  if (String(topic) == "air_condition/6552") {
    if (message != currentAirState) {  // ถ้าสถานะเปลี่ยน
      Serial.println("State changed!");
      currentAirState = message;  // อัปเดตสถานะปัจจุบัน

      // ควบคุมการทำงานของเซอร์โวเมื่อสถานะเปลี่ยน
      myServo.write(0);           // หมุนไปที่ 0 องศา
      delay(1000);
      myServo.write(90);          // หมุนไปที่ 90 องศา
      delay(1000);
      myServo.write(0);           // หมุนกลับที่ 0 องศา
      delay(1000);
    }
  }
}

void setup() {
  Serial.begin(115200);  // เริ่มต้นการแสดงผลใน Serial Monitor

  // เริ่มต้นการใช้งาน DHT และเซอร์โวมอเตอร์
  dht.begin();
  myServo.attach(SERVO_PIN);  // เชื่อมต่อเซอร์โวมอเตอร์กับพิน
  myServo.write(90);          // ตั้งค่าเซอร์โวเริ่มต้นที่ 90 องศา

  // เชื่อมต่อ WiFi
  setup_wifi();

  // ตั้งค่า MQTT server และ callback
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

void loop() {
  // ตรวจสอบการเชื่อมต่อ WiFi
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected! Reconnecting...");
    setup_wifi();  // เชื่อมต่อ WiFi ใหม่ถ้าหากหลุด
  }

  // ตรวจสอบการเชื่อมต่อ MQTT
  if (!client.connected()) {
    reconnect();  // เชื่อมต่อ MQTT ใหม่ถ้าหากหลุด
  }
  client.loop();  // รอการทำงานของ MQTT

  // อ่านค่าจากเซ็นเซอร์ DHT22
  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();

  // ถ้าค่าที่อ่านได้ไม่เป็น NaN (ไม่ผิดปกติ)
  if (!isnan(humidity) && !isnan(temperature)) {
    // แสดงผลข้อมูลที่อ่านได้ใน Serial Monitor
    Serial.print("Humidity: ");
    Serial.print(humidity);
    Serial.print("%  Temperature: ");
    Serial.print(temperature);
    Serial.println("°C");

    // ส่งข้อมูลอุณหภูมิไปยัง MQTT
    char tempString[8];
    dtostrf(temperature, 6, 2, tempString);
    client.publish("temp_sensor_topic/6552", tempString);  // หัวข้อ "temp_sensor_topic/6552"
    Serial.println("Temperature sent to MQTT");

    // ส่งข้อมูลความชื้นไปยัง MQTT
    char humidityString[8];
    dtostrf(humidity, 6, 2, humidityString);
    client.publish("humidity_sensor_topic/6552", humidityString);  // หัวข้อ "humidity_sensor_topic/6552"
    Serial.println("Humidity sent to MQTT");
  } else {
    Serial.println("Failed to read from DHT sensor!");  // หากไม่สามารถอ่านค่าจาก DHT ได้
  }

  delay(2000);  // หน่วงเวลา 2 วินาทีก่อนการทำงานในรอบถัดไป
}
