#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "DHT.h"

// Cấu hình màn hình OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Cấu hình cảm biến DHT22 nối với chân GPIO 4
#define DHTPIN 4
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// Cấu hình đèn LED cảnh báo nối với chân GPIO 23
#define LED_PIN 23

// Ngưỡng cảnh báo nhiệt độ (trên 35°C sẽ sáng đèn)
const float TEMP_THRESHOLD = 35.0;

void setup()
{
  Serial.begin(115200);
  delay(1000); // Chờ cổng Serial ổn định

  // Khởi tạo chân LED
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // Khởi tạo cảm biến DHT22
  dht.begin();

  // Khởi tạo màn hình OLED (Địa chỉ I2C 0x3C)
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
  {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 10);
  display.println("System Initializing...");
  display.display();
  delay(1000);

  // Kết nối Wi-Fi giả lập trên Wokwi
  WiFi.begin("Wokwi-GUEST", "", 6);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");
}

void loop()
{
  // Đợi 2 giây giữa các lần đọc (DHT22 cần tối thiểu 2s để trả về dữ liệu mới)
  delay(2000);

  // Đọc độ ẩm và nhiệt độ
  float h = dht.readHumidity();
  float t = dht.readTemperature(); // Đơn vị độ C

  // Kiểm tra xem cảm biến có đọc được dữ liệu không
  if (isnan(h) || isnan(t))
  {
    Serial.println(F("Warning: Failed to read from DHT sensor, retrying..."));
    return;
  }

  // In thông số ra màn hình Serial Monitor
  Serial.print(F("Humidity: "));
  Serial.print(h);
  Serial.print(F("%  |  Temperature: "));
  Serial.print(t);
  Serial.println(F("°C"));

  // Hiển thị giao diện trực quan lên màn hình OLED
  display.clearDisplay();

  // Tiêu đề
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("SMART ROOM MONITOR");
  display.drawFastHLine(0, 10, 128, SSD1306_WHITE);

  // Thông số Nhiệt độ lớn
  display.setCursor(0, 18);
  display.print("Temp: ");
  display.setTextSize(2);
  display.setCursor(36, 16);
  display.print(t, 1);
  display.cp437(true);
  display.write(167); // Ký hiệu độ C (°C)
  display.print("C");

  // Thông số Độ ẩm
  display.setTextSize(1);
  display.setCursor(0, 42);
  display.print("Humidity: ");
  display.print(h, 1);
  display.print(" %");

  // Kiểm tra ngưỡng cảnh báo nhiệt độ
  if (t > TEMP_THRESHOLD)
  {
    digitalWrite(LED_PIN, HIGH); // Bật LED đỏ
    display.setCursor(0, 54);
    display.print("STATUS: HIGH TEMP!");
  }
  else
  {
    digitalWrite(LED_PIN, LOW); // Tắt LED
    display.setCursor(0, 54);
    display.print("STATUS: NORMAL");
  }

  display.display();
}