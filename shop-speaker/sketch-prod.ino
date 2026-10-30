#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <HTTPClient.h>
#include <ESP_I2S.h>

// --- 1. Cấu hình Wi-Fi ---
const char *SSID = "Ninedev";
const char *PASSWORD = "123456789x@X";

// --- 2. Cấu hình TTS Server (Máy tính chạy tts_server.py) ---
const char *SERVER_IP = "192.168.1.25"; // Đổi nếu IP máy bạn thay đổi
const int SERVER_PORT = 8000;

// --- 3. Cấu hình MQTT Broker ---
const char* mqtt_server = "61.14.233.172";
const int mqtt_port = 1883;
const char* topic_payment = "shop/speaker/payment";

// --- 4. Bộ chân I2S cho MAX98357A ---
#define I2S_BCLK 27
#define I2S_LRC  26
#define I2S_DOUT 25

#define SAMPLE_RATE 16000

WiFiClient espClient;
PubSubClient client(espClient);
I2SClass i2s;

// Âm thanh Ting thông báo trước khi đọc giọng nói
void playChime() {
  const int samples = SAMPLE_RATE * 150 / 1000;
  int16_t buffer[512];
  int generated = 0;
  while (generated < samples) {
    int count = min(256, samples - generated);
    for (int i = 0; i < count; i++) {
      float t = (float)(generated + i) / SAMPLE_RATE;
      int16_t sample = (int16_t)(6000 * sin(2.0 * PI * 1318.51 * t)); // Nốt Mi E6
      buffer[i * 2]     = sample;
      buffer[i * 2 + 1] = sample;
    }
    i2s.write((uint8_t*)buffer, count * 2 * sizeof(int16_t));
    generated += count;
  }
}

// Bóc tách JSON thủ công (không phụ thuộc thư viện ngoài)
String extractJsonValue(const String &json, const String &key) {
  String searchKey = "\"" + key + "\":";
  int startIdx = json.indexOf(searchKey);
  if (startIdx == -1) return "";

  startIdx += searchKey.length();
  while (startIdx < json.length() && (json[startIdx] == ' ' || json[startIdx] == '\"')) {
    startIdx++;
  }

  int endIdx = startIdx;
  while (endIdx < json.length() && json[endIdx] != ',' && json[endIdx] != '}' && json[endIdx] != '\"') {
    endIdx++;
  }

  return json.substring(startIdx, endIdx);
}

// Gọi API tải stream PCM và phát thẳng ra loa
void streamVoiceFromServer(const String &url) {
  HTTPClient http;
  Serial.printf("[Audio] Request: %s\n", url.c_str());
  http.begin(url);

  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    WiFiClient *stream = http.getStreamPtr();
    uint8_t buffer[1024];

    Serial.println("[Audio] Loa dang doc...");
    while (http.connected() && (stream->available() > 0 || stream->peek() != -1)) {
      int bytesRead = stream->readBytes((char*)buffer, sizeof(buffer));
      if (bytesRead > 0) {
        i2s.write(buffer, bytesRead);
      }
    }
    Serial.println("[Audio] Hoan tat phat am!");
  } else {
    Serial.printf("[Loi] HTTP Code: %d\n", httpCode);
  }
  http.end();
}

// Hàm callback nhận dữ liệu khi có message từ MQTT
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  Serial.println("\n-------------------------------------------");
  Serial.printf("[MQTT] Nhan tin tu topic [%s]:\n", topic);
  Serial.println(message);

  String event = extractJsonValue(message, "event");
  String amount = extractJsonValue(message, "amount");
  String invoice = extractJsonValue(message, "invoiceNumber");

  if (event == "PAYMENT_RECEIVED" && amount.length() > 0) {
    Serial.printf(">>> Xac nhan don [%s]: +%s VND\n", invoice.c_str(), amount.c_str());

    // 1. Kêu 1 tiếng Ting nhẹ báo hiệu
    playChime();
    delay(100);

    // 2. Kéo giọng đọc thanh toán về phát ra loa
    String requestUrl = String("http://") + SERVER_IP + ":" + SERVER_PORT + "/speak?amount=" + amount;
    streamVoiceFromServer(requestUrl);
  }
}

// Kết nối lại MQTT nếu bị rớt mạng
void reconnectMQTT() {
  while (!client.connected()) {
    Serial.print("[MQTT] Dang ket noi toi broker...");
    String clientId = "ESP32_Speaker_" + String(random(0xffff), HEX);
    
    if (client.connect(clientId.c_str())) {
      Serial.println(" Thanh cong!");
      client.subscribe(topic_payment);
      Serial.printf("[MQTT] Da subscribe topic: %s\n", topic_payment);
      
      // Kêu tiếng bíp ngắn báo broker đã sẵn sàng
      playChime();
    } else {
      Serial.printf(" That bai, rc=%d. Thu lai sau 3 giay...\n", client.state());
      delay(3000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== KHOI DONG LOA THONG BAO MQTT ===");

  // 1. Khoi tao I2S
  i2s.setPins(I2S_BCLK, I2S_LRC, I2S_DOUT);
  if (!i2s.begin(I2S_MODE_STD, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO, I2S_STD_SLOT_BOTH)) {
    Serial.println("[LOI] Khoi tao I2S that bai!");
    while (1) { delay(1000); }
  }

  // 2. Ket noi Wi-Fi
  Serial.printf("Ket noi Wi-Fi: %s", SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n[Wi-Fi] OK! IP ESP32: ");
  Serial.println(WiFi.localIP());

  // 3. Cau hinh MQTT
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(mqttCallback);
  client.setBufferSize(1024); // Đảm bảo buffer đủ lớn để nhận trọn vẹn JSON payload
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.reconnect();
    delay(1000);
    return;
  }

  if (!client.connected()) {
    reconnectMQTT();
  }
  client.loop(); // Lắng nghe message liên tục trong background
}
