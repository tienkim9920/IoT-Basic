#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#include "AudioFileSourceHTTPStream.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2S.h"

// 1. Cấu hình Wi-Fi & MQTT
const char* ssid = "Ninedev";
const char* password = "matkhaucuatoi";
const char* mqtt_server = "61.14.233.172";
const int mqtt_port = 1883;
const char* topic_payment = "shop/speaker/payment";

WiFiClient espClient;
PubSubClient client(espClient);

// Khởi tạo đối tượng âm thanh I2S
AudioGeneratorMP3 *mp3 = NULL;
AudioFileSourceHTTPStream *fileHttp = NULL;
AudioOutputI2S *out = NULL;

bool isPlaying = false;

// 2. Thuật toán chuyển số tiền sang chữ tiếng Việt
String doc3So(int n, bool dayDu) {
  String chuSo[] = {"khong", "mot", "hai", "ba", "bon", "nam", "sau", "bay", "tam", "chin"};
  int tram = n / 100;
  int chuc = (n % 100) / 10;
  int donVi = n % 10;
  String kq = "";

  if (tram > 0 || dayDu) {
    kq += chuSo[tram] + "+tram+";
  }

  if (chuc > 1) {
    kq += chuSo[chuc] + "+muoi+";
    if (donVi == 1) kq += "mot+";
    else if (donVi == 5) kq += "lam+";
    else if (donVi > 0) kq += chuSo[donVi] + "+";
  } else if (chuc == 1) {
    kq += "muoi+";
    if (donVi == 5) kq += "lam+";
    else if (donVi > 0) kq += chuSo[donVi] + "+";
  } else if (tram > 0 && donVi > 0) {
    kq += "le+" + chuSo[donVi] + "+";
  } else if (donVi > 0) {
    kq += chuSo[donVi] + "+";
  }
  return kq;
}

String docSoThanhChu(long so) {
  if (so == 0) return "khong";
  String donViLon[] = {"", "nghin+", "trieu+", "ty+"};
  String chuoiKq = "";
  int i = 0;

  while (so > 0) {
    int n = so % 1000;
    if (n > 0) {
      String str = doc3So(n, i > 0 && so >= 1000);
      chuoiKq = str + donViLon[i] + chuoiKq;
    }
    so = so / 1000;
    i++;
  }
  return chuoiKq;
}

// 3. Hàm phát âm thanh từ Google TTS stream thẳng ra I2S
void playVoiceGoogle(String textEncoded) {
  if (isPlaying) {
    if (mp3 && mp3->isRunning()) mp3->stop();
    delete fileHttp;
    delete mp3;
    isPlaying = false;
  }

  // URL lấy audio MP3 chuẩn Google tiếng Việt
  String url = "http://translate.google.com/translate_tts?ie=UTF-8&tl=vi&client=tw-ob&q=" + textEncoded;
  Serial.printf("[AUDIO] Dang lay am thanh tu: %s\n", url.c_str());

  fileHttp = new AudioFileSourceHTTPStream(url.c_str());
  mp3 = new AudioGeneratorMP3();

  if (mp3->begin(fileHttp, out)) {
    isPlaying = true;
    Serial.println("[AUDIO] Bat dau phat loa...");
  } else {
    Serial.println("[AUDIO] Loi khoi tao file audio");
  }
}

// 4. Nhận bản tin MQTT từ Backend
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

  if (String(event) == "PAYMENT_RECEIVED" && amount > 0) {
    Serial.printf("Xac nhan GD thanh cong: %ld VND\n", amount);
    
    // Tạo câu tiếng Việt: "Thanh toán thành công [số tiền] đồng"
    String textWords = "Thanh+toan+thanh+cong+" + docSoThanhChu(amount) + "dong";
    playVoiceGoogle(textWords);
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Dang ket noi MQTT Broker...");
    String clientId = "ESP32_SoundBox_Physical_" + String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      Serial.println(" Ket noi thanh cong!");
      client.subscribe(topic_payment);
    } else {
      Serial.print(" That bai, rc=");
      Serial.print(client.state());
      Serial.println(". Thu lai sau 3s...");
      delay(3000);
    }
  }
}

void setup() {
  Serial.begin(115200);

  // Cấu hình chân phần cứng cho bộ giải mã I2S MAX98357A
  out = new AudioOutputI2S();
  out->SetPinout(26, 25, 22); // BCLK = GPIO26, LRC = GPIO25, DIN = GPIO22
  out->SetGain(1.5);           // Tăng giảm âm lượng (0.1 - 4.0)

  // Kết nối Wi-Fi thực tế
  WiFi.begin(ssid, password);
  Serial.print("Dang ket noi Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.printf("\nWi-Fi da ket noi! IP: %s\n", WiFi.localIP().toString().c_str());

  // Cấu hình MQTT
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // Xử lý luồng buffer âm thanh liên tục
  if (isPlaying && mp3) {
    if (mp3->isRunning()) {
      if (!mp3->loop()) {
        mp3->stop();
        delete fileHttp;
        delete mp3;
        fileHttp = NULL;
        mp3 = NULL;
        isPlaying = false;
        Serial.println("[AUDIO] Phat am thanh hoan tat.");
      }
    }
  }
}