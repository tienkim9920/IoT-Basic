#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

#define BUZZER_PIN 18

// Wi-Fi ảo của Wokwi (phải dùng Wokwi-GUEST để ra được internet tới broker IP public)
const char* ssid = "Wokwi-GUEST";
const char* password = "";

// Cấu hình MQTT
const char* mqtt_server = "61.14.233.172";
const int mqtt_port = 1883;
const char* topic_payment = "shop/speaker/payment";

WiFiClient espClient;
PubSubClient client(espClient);

// Tạo tiếng chuông Ting Ting bằng Buzzer
void playTingTing() {
  tone(BUZZER_PIN, 2093, 120); // Note C7
  delay(140);
  tone(BUZZER_PIN, 2793, 250); // Note F7
  delay(280);
  noTone(BUZZER_PIN);
}

// Cập nhật giao diện màn hình OLED khi nhận tiền
void showPaymentOLED(long amount, const char* invoice) {
  display.clearDisplay();
  
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(16, 0);
  display.println("THANH TOAN SEPAY");
  display.drawFastHLine(0, 10, 128, SSD1306_WHITE);

  display.setCursor(0, 16);
  display.print("HD: ");
  display.println(invoice);

  display.setCursor(0, 30);
  display.print("So tien:");

  display.setTextSize(2);
  display.setCursor(0, 44);
  display.print("+");
  display.print(amount);

  display.display();
}

// Giả lập đọc chuỗi voice audio qua Serial
void simulateVoiceTracks(long amount) {
  Serial.println("==================================================");
  Serial.println("[AUDIO EMULATOR] Danh sach track DFPlayer can phat:");
  Serial.println(" -> Play: 0001.mp3 (Ting Ting)");
  Serial.println(" -> Play: 0002.mp3 (Thanh toan thanh cong)");

  if (amount == 20000) {
    Serial.println(" -> Play: 0005.mp3 (Hai)");
    Serial.println(" -> Play: 0014.mp3 (Muoi)");
    Serial.println(" -> Play: 0016.mp3 (Nghin)");
  } else if (amount == 50000) {
    Serial.println(" -> Play: 0008.mp3 (Nam)");
    Serial.println(" -> Play: 0014.mp3 (Muoi)");
    Serial.println(" -> Play: 0016.mp3 (Nghin)");
  } else {
    Serial.printf(" -> Play: [Doc so tien %ld]\n", amount);
  }
  Serial.println(" -> Play: 0018.mp3 (Dong)");
  Serial.println("==================================================");
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("\n[MQTT] Nhan ban tin tu topic: ");
  Serial.println(topic);

  StaticJsonDocument<384> doc;
  DeserializationError error = deserializeJson(doc, payload, length);

  if (error) {
    Serial.print("Loi parse JSON: ");
    Serial.println(error.f_str());
    return;
  }

  const char* event = doc["event"];
  long amount = doc["amount"];
  const char* invoice = doc["invoiceNumber"];

  Serial.printf("Event: %s | Invoice: %s | Amount: %ld VND\n", event, invoice, amount);

  if (String(event) == "PAYMENT_RECEIVED" && amount > 0) {
    // 1. Kích hoạt còi kêu Ting Ting
    playTingTing();
    // 2. Cập nhật màn hình OLED
    showPaymentOLED(amount, invoice ? invoice : "");
    // 3. In log mô phỏng voice DFPlayer
    simulateVoiceTracks(amount);
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Dang ket noi toi MQTT Broker 61.14.233.172...");
    String clientId = "Wokwi_Speaker_" + String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      Serial.println(" Ket noi thanh cong!");
      client.subscribe(topic_payment);
    } else {
      Serial.print(" That bai, rc=");
      Serial.print(client.state());
      Serial.println(". Thu lai sau 3 giay...");
      delay(3000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(BUZZER_PIN, OUTPUT);

  // Khởi tạo màn hình OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("Khoi tao SSD1306 that bai");
  } else {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(10, 24);
    display.println("WAITING PAYMENT...");
    display.display();
  }

  // Kết nối Wi-Fi Wokwi
  WiFi.begin(ssid, password, 6);
  Serial.print("Connecting WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.printf("\nWiFi Connected! IP: %s\n", WiFi.localIP().toString().c_str());

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
}