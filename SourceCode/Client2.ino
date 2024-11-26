#include <SPI.h>
#include <MFRC522.h>
#include <WiFi.h>
#include <PubSubClient.h>

// กำหนดพินที่เชื่อมต่อกับอุปกรณ์
#define SS_PIN 5
#define RST_PIN 2
#define RELAY_PIN 12 // พินควบคุม Relay

// สร้างออบเจ็กต์สำหรับ RFID
MFRC522 rfid(SS_PIN, RST_PIN);

// กำหนดข้อมูล WiFi และ MQTT
const char* ssid = "Inwkritza007_2.4G";
const char* password = "0934603281";
const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;

WiFiClient espClient;
PubSubClient client(espClient);

// ฟังก์ชันเชื่อมต่อ WiFi
void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to WiFi...");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

// ฟังก์ชันเชื่อมต่อ MQTT
void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    String clientId = "ESP32_Client_" + String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      client.subscribe("relay_control/6552");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

// ฟังก์ชันเมื่อได้รับข้อความ MQTT
void callback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.println(message);
}

void setup() {
  Serial.begin(115200);

  // เริ่มต้นการทำงานของ RFID
  SPI.begin();
  rfid.PCD_Init();

  // ตั้งค่าพินของ Relay
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW); // เริ่มต้นปิด Relay

  // เชื่อมต่อ WiFi
  setup_wifi();

  // ตั้งค่า MQTT broker และ callback
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

void loop() {
  // ตรวจสอบการเชื่อมต่อ WiFi
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected! Reconnecting...");
    setup_wifi();
  }

  // ตรวจสอบการเชื่อมต่อ MQTT
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // การอ่านค่าจาก RFID
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    Serial.print("RFID UID:");

    String rfidUID = ""; // สร้าง String เพื่อเก็บ UID

    for (byte i = 0; i < rfid.uid.size; i++) {
      Serial.print(" ");
      Serial.print(rfid.uid.uidByte[i], HEX);

      // สร้าง UID เป็น String
      if (rfid.uid.uidByte[i] < 0x10) {
        rfidUID += "0"; // เติม 0 หากตัวเลขน้อยกว่า 16
      }
      rfidUID += String(rfid.uid.uidByte[i], HEX);
    }
    Serial.println();

    // ส่งค่า UID ไปยัง MQTT
    client.publish("rfid/6552", rfidUID.c_str());
    Serial.println("RFID UID sent to MQTT");

    // ควบคุม Relay ตาม UID
    if (rfidUID == "a446a656") { // UID ที่อนุญาต
      Serial.println("Authorized user, activating relay...");
      digitalWrite(RELAY_PIN, HIGH); // เปิด Relay
      delay(5000); // เปิดใช้งาน 5 วินาที
      digitalWrite(RELAY_PIN, LOW); // ปิด Relay
    } else {
      Serial.println("Unauthorized user, access denied.");
    }

    rfid.PICC_HaltA(); // หยุดการสแกนบัตร
  }

  delay(1000); // รอ 1 วินาทีก่อนอ่านค่าใหม่
}
