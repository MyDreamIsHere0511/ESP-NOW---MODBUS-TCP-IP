#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "modbustcp.h"

// ===== ĐỊA CHỈ MAC XE =====
uint8_t receiverMAC[] = {0x88, 0x57, 0x21, 0xAC, 0xA7, 0xA0}; // Cập nhật đúng MAC xe của bạn nếu cần

// ===== CẤU HÌNH =====
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
uint16_t mbRegisters[10]; 
ModbusTCPServer modbus(mbRegisters, 10);

// PINS
#define LED_RED   14 
#define LED_GREEN 27 
#define VRXL 36
#define VRYL 39
#define SWL  33
#define VRXR 35
#define VRYR 34
#define SWR  32

// DATA
typedef struct { int8_t steer; int8_t speed; bool emergencyStop; uint8_t command; uint32_t timestamp; } ControlData;
typedef struct { float pitch; float roll; float yaw; uint16_t distance; } SensorData;

ControlData dataToSend;
SensorData dataReceived;

enum State { WAITING, READY };
State currentState = WAITING;

// VARS LOGIC
unsigned long btnPressTime = 0;
bool isHoldingButtons = false;
bool actionPerformed = false;
bool isSignalGood = false; 
unsigned long lastRecvTime = 0; 

// VARS YÊU CẦU MỚI (SPEED LIMIT)
int speedLimits[] = {120, 180, 255}; // Mảng giá trị tốc độ
int speedLimitIdx = 0;               // 0: 120, 1: 180, 2: 255
bool lastBtnRState = HIGH;           // Để bắt sườn nút nhấn phải

// CALIBRATION
int zeroLX = 1800, zeroRY = 1800;
#define DEAD_ZONE 20

// CALLBACK
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {}

void OnDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *incomingData, int len) {
  if (len == sizeof(SensorData)) {
    memcpy(&dataReceived, incomingData, sizeof(SensorData));
    lastRecvTime = millis();
    isSignalGood = true;

    // Modbus Update
    mbRegisters[3] = (int16_t)(dataReceived.pitch * 10);
    mbRegisters[4] = (int16_t)(dataReceived.roll * 10);
    mbRegisters[5] = (int16_t)(dataReceived.yaw * 10);
    mbRegisters[6] = dataReceived.distance / 10; 
  }
}

void setup() {
  Serial.begin(115200);
  
  Wire.begin(); 
  Wire.setClock(400000); 
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  
  pinMode(LED_RED, OUTPUT); pinMode(LED_GREEN, OUTPUT);
  pinMode(VRXL, INPUT); pinMode(VRYL, INPUT); pinMode(SWL, INPUT_PULLUP);
  pinMode(VRXR, INPUT); pinMode(VRYR, INPUT); pinMode(SWR, INPUT_PULLUP);
  
  digitalWrite(LED_RED, HIGH); digitalWrite(LED_GREEN, LOW);

  // Auto Calibration
  long sumX = 0, sumY = 0;
  for (int i = 0; i < 20; i++) { sumX += analogRead(VRXL); sumY += analogRead(VRYR); delay(5); }
  zeroLX = sumX / 20; zeroRY = sumY / 20;

  // WiFi & ESP-NOW
  WiFi.mode(WIFI_AP_STA);
  esp_wifi_set_ps(WIFI_PS_NONE);
  WiFi.softAP("RC_CONTROLLER", "12345678", 1, 0);
  
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);

  if (esp_now_init() != ESP_OK) ESP.restart();
  
  esp_now_register_recv_cb(OnDataRecv);
  esp_now_register_send_cb(OnDataSent);
  
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, receiverMAC, 6);
  peerInfo.channel = 1; peerInfo.encrypt = false;
  esp_now_add_peer(&peerInfo);

  modbus.begin();
}

void updateDisplay() {
  display.clearDisplay();
  display.setTextSize(1); display.setTextColor(SSD1306_WHITE); display.setCursor(0, 0);

  if (isSignalGood) {
    display.print("KET NOI:  [TOT]");
    
    // Góc nghiêng
    display.setCursor(0, 12); 
    display.printf("P: %-4.0f R:%-4.0f Y: %-4.0f", dataReceived.pitch, dataReceived.roll, dataReceived.yaw);

    // Dòng 3: Hiển thị trạng thái + SPEED LIMIT (Yêu cầu mới)
    display.setCursor(0, 24);
    if (currentState == READY) {
      // Hiển thị tốc độ Joystick và Giới hạn hiện tại (L:120/180/255)
      display.printf("S:%-3d L:%-3d", dataToSend.speed, speedLimits[speedLimitIdx]);
    } else {
      if (isHoldingButtons) {
        long ms = millis() - btnPressTime;
        display.printf("GIU NUT: %ld ms", (ms > 3000) ? 3000 : ms);
      } else {
        display.print("CHE DO: CHO (KHOA)");
      }
    }

    // Khoảng cách
    display.setCursor(0, 40); display.setTextSize(2); 
    display.printf("%3d cm", dataReceived.distance / 10);
    
  } else {
    display.print("MAT KET NOI !!!");
    display.setCursor(0, 20); display.print("Dang tim lai xe...");
  }
  display.display();
}

void loop() {
  modbus.handle();
  unsigned long now = millis();

  // Watchdog
  if (now - lastRecvTime > 800) {
    isSignalGood = false;
    if (currentState == READY) {
      currentState = WAITING;
      digitalWrite(LED_RED, HIGH); digitalWrite(LED_GREEN, LOW);
    }
  }

  // Control Loop (50Hz)
  static unsigned long lastCtrl = 0;
  if (now - lastCtrl > 20) {
    lastCtrl = now;

    // Đọc nút bấm
    bool btnL = !digitalRead(SWL); 
    bool btnR = !digitalRead(SWR);
    dataToSend.command = 0;

    // --- LOGIC 1: Hai nút cùng lúc (Giữ nguyên logic cũ) ---
    if (btnL && btnR) { 
      if (!isHoldingButtons) { 
        isHoldingButtons = true; btnPressTime = now; actionPerformed = false; 
      }
      if (isSignalGood && now - btnPressTime > 3000 && !actionPerformed) {
        actionPerformed = true;
        if (currentState == WAITING) {
          currentState = READY; dataToSend.command = 1; 
          digitalWrite(LED_RED, LOW); digitalWrite(LED_GREEN, HIGH);
        } else {
          currentState = WAITING; dataToSend.command = 2; 
          digitalWrite(LED_RED, HIGH); digitalWrite(LED_GREEN, LOW);
        }
      }
    } 
    // --- LOGIC MỚI: Xử lý nút đơn lẻ khi đang READY ---
    else { 
      isHoldingButtons = false; actionPerformed = false; 
      
      if (currentState == READY) {
        // YÊU CẦU 3: Nút Phải chuyển Speed Limit (Bắt sườn xung xuống - thả tay ra mới tính hoặc vừa nhấn)
        // Ở đây dùng logic bắt sườn nhấn (Rising edge logic vì input pullup -> logic đảo)
        if (btnR && lastBtnRState == LOW) { // Nếu đang nhấn mà trước đó không nhấn (lỗi logic biến, sửa lại bên dưới)
           // Logic đúng: btnR=true (đang nhấn), lastLoop=false -> Nhấn lần đầu
        }
        
        // Cải tiến logic nút nhấn đơn giản:
        if (btnR && !lastBtnRState) { // Mới nhấn
             speedLimitIdx++;
             if (speedLimitIdx > 2) speedLimitIdx = 0;
             // Beep nhẹ hoặc in Serial để biết
             Serial.printf("Speed Limit: %d\n", speedLimits[speedLimitIdx]);
        }
        lastBtnRState = btnR; // Lưu trạng thái
      }
    }

    // --- TÍNH TOÁN JOYSTICK ---
    int rawLX = analogRead(VRXL); 
    int rawRY = analogRead(VRYR);
    int8_t steer = constrain(map(rawLX - zeroLX, -zeroLX, 4095 - zeroLX, -100, 100), -100, 100);
    int8_t speedPct = constrain(map(rawRY - zeroRY, -zeroRY, 4095 - zeroRY, -100, 100), -100, 100);
    
    if (abs(steer) < 15) steer = 0; 
    if (abs(speedPct) < 15) speedPct = 0;

    // --- ÁP DỤNG LOGIC MỚI VÀO DỮ LIỆU GỬI ---
    if (currentState == READY && isSignalGood) {
      // YÊU CẦU 2: Nút Trái là PHANH GẤP (Dừng khẩn cấp)
      if (btnL && !btnR) { // Chỉ nhấn trái (để tránh xung đột với nhấn cả 2)
         dataToSend.emergencyStop = true; // Code xe RXXX đã có logic: if emergencyStop -> setMotor(0)
         dataToSend.speed = 0;
      } else {
         dataToSend.emergencyStop = false;
         
         // YÊU CẦU 3: SCALING TỐC ĐỘ
         // Xe nhận 0-100 rồi map ra 0-255. 
         // Ta muốn max PWM là speedLimits[speedLimitIdx] (120, 180, 255).
         // Công thức: SpeedGửi = (SpeedJoystick% * LimitPWM) / 255
         // Nhưng xe lại map SpeedGửi(0-100) -> PWM(0-255).
         // Nên ta cần tính ngược một chút để khi Xe map lại thì đúng Limit.
         // Tuy nhiên, để đơn giản và hiệu quả: 
         // Coi như 100% Joystick = 100% Limit hiện tại.
         
         float limitFactor = (float)speedLimits[speedLimitIdx] / 255.0; 
         dataToSend.speed = (int8_t)(speedPct * limitFactor); 
         dataToSend.steer = steer;
      }
    } else {
      dataToSend.emergencyStop = false; dataToSend.speed = 0; dataToSend.steer = 0;
    }

    dataToSend.timestamp = now;
    esp_now_send(receiverMAC, (uint8_t *)&dataToSend, sizeof(dataToSend));

    // Modbus Update
    mbRegisters[0] = currentState; 
    mbRegisters[1] = dataToSend.steer + 100;
    mbRegisters[2] = dataToSend.speed + 100;
  }

  // OLED Update (10Hz)
  static unsigned long lastDisp = 0;
  if (now - lastDisp > 100) { lastDisp = now; updateDisplay(); }
}