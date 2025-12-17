/*
 * PROJECT: XE ĐIỀU KHIỂN RC ESP32 (ESP-NOW + MODBUS MONITOR)
 * PHIÊN BẢN: WDT ENABLED (WATCHDOG TIMER 1s) - FIX CORE V3.0
 * LOGIC: Ổn định, đã test.
 */

#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <ESP32Servo.h>
#include <Wire.h>
#include <MPU6050_light.h>
#include <VL53L0X.h>
#include <esp_task_wdt.h> // Thư viện Watchdog Timer

// Thư viện can thiệp thanh ghi hệ thống (để tắt Brownout)
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

// ================================================================
//                        CẤU HÌNH HỆ THỐNG
// ================================================================

// Cấu hình Watchdog: 1 giây (Nếu treo quá 1s sẽ tự reset)
#define WDT_TIMEOUT 1 

// Địa chỉ MAC của Tay Cầm Điều Khiển
uint8_t remoteMAC[] = {0xF8, 0xB3, 0xB7, 0x20, 0x94, 0x78};

// Định nghĩa chân kết nối (Pin Definitions)
#define LED_PIN     18 
#define BUZZER      19

// Driver Động Cơ (L298/TB6612)
#define PWMA        32
#define AI1         26
#define AI2         27
#define PWMB        25
#define BI1         14
#define BI2         13
#define STBY        33

// Kênh PWM (ESP32 LEDC Channel)
#define MOTOR_CHA   6
#define MOTOR_CHB   7

// ================================================================
//                        KHỞI TẠO ĐỐI TƯỢNG
// ================================================================
Servo steeringServo;
MPU6050 mpu(Wire);
VL53L0X sensor;

// ================================================================
//                        CẤU TRÚC DỮ LIỆU
// ================================================================

// Dữ liệu nhận từ Tay cầm (Control Data)
typedef struct { 
    int8_t steer;           // Góc lái (-100 đến 100)
    int8_t speed;           // Tốc độ (-100 đến 100)
    bool emergencyStop;     // Cờ dừng khẩn cấp
    uint8_t command;        // Lệnh đặc biệt (1: Ready, 2: Waiting)
    uint32_t timestamp;     // Thời gian gửi
} ControlData;

// Dữ liệu gửi về Tay cầm (Sensor Data)
typedef struct { 
    float pitch;            // Góc nghiêng trước sau
    float roll;             // Góc nghiêng trái phải
    float yaw;              // Góc xoay hướng
    uint16_t distance;      // Khoảng cách đo được (mm)
} SensorData;

// Biến lưu trữ dữ liệu
ControlData rxData;
SensorData txData;

// Trạng thái hoạt động của xe
enum State { WAITING, READY };
State currentState = WAITING;

// Biến logic hệ thống
unsigned long lastRecvTime = 0;
bool isConnected = false;

// ================================================================
//                        HÀM PHỤ TRỢ (HELPER FUNCTIONS)
// ================================================================

// --- Bộ lọc trung bình trượt cho cảm biến khoảng cách ---
// Giúp số đo ổn định, bớt nhảy loạn xạ
int fBuf[5]; 
int fIdx = 0;

int getFiltered(int val) {
    // Lọc nhiễu gai (giá trị quá lớn hoặc lỗi 65535)
    if (val > 2000 || val == 65535) {
        val = 2000;
    }
    
    // Lưu vào bộ đệm vòng tròn
    fBuf[fIdx] = val; 
    fIdx = (fIdx + 1) % 5;
    
    // Tính trung bình cộng 5 giá trị gần nhất
    long s = 0; 
    for (int i = 0; i < 5; i++) {
        s += fBuf[i];
    }
    return s / 5;
}

// --- Hàm phát tiếng bíp (Còi chíp) ---
void beep(int times, int duration) { 
    for (int i = 0; i < times; i++) {
        digitalWrite(BUZZER, 1); 
        delay(duration); 
        digitalWrite(BUZZER, 0); 
        delay(duration); 
    } 
}

// ================================================================
//                        HÀM ĐIỀU KHIỂN ĐỘNG CƠ
// ================================================================

void setMotor(int8_t speed) {
    // Vùng chết (Deadzone): Tốc độ quá nhỏ thì coi như dừng
    if (abs(speed) < 5) {
        speed = 0;
    }

    // Xử lý chiều quay
    if (speed > 0) { 
        // Tiến
        digitalWrite(AI1, 1); digitalWrite(AI2, 0); 
        digitalWrite(BI1, 1); digitalWrite(BI2, 0); 
    } 
    else if (speed < 0) { 
        // Lùi
        digitalWrite(AI1, 0); digitalWrite(AI2, 1); 
        digitalWrite(BI1, 0); digitalWrite(BI2, 1); 
    } 
    else { 
        // Dừng / Phanh
        digitalWrite(AI1, 0); digitalWrite(AI2, 0); 
        digitalWrite(BI1, 0); digitalWrite(BI2, 0); 
    }

    // Điều khiển tốc độ bằng PWM
    // Map giá trị 0-100 (từ Joystick) sang 0-255 (PWM 8-bit)
    int pwm = map(abs(speed), 0, 100, 0, 255);
    ledcWrite(PWMA, pwm); 
    ledcWrite(PWMB, pwm);
}

// ================================================================
//                        CALLBACK ESP-NOW
// ================================================================

void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
    // Copy dữ liệu nhận được vào struct
    memcpy(&rxData, data, sizeof(rxData));
    lastRecvTime = millis(); // Cập nhật thời gian nhận cuối cùng
  
    // Đánh dấu đã kết nối
    isConnected = true;
    digitalWrite(LED_PIN, HIGH);

    // --- XỬ LÝ CHUYỂN TRẠNG THÁI (STATE MACHINE) ---
    
    // 1. Chuyển sang READY (Sẵn sàng chạy)
    if (currentState == WAITING && rxData.command == 1) { 
        currentState = READY; 
        beep(2, 50); // Bíp 2 lần nhanh
    }
    // 2. Chuyển sang WAITING (Chế độ chờ/Khóa)
    else if (currentState == READY && rxData.command == 2) { 
        setMotor(0);
        // Nháy đèn báo hiệu khóa
        for (int i = 0; i < 3; i++) { 
            digitalWrite(LED_PIN, 0); delay(100); 
            digitalWrite(LED_PIN, 1); delay(100); 
        }
        currentState = WAITING; 
        beep(2, 100); // Bíp 2 lần chậm
    }

    // --- ĐIỀU KHIỂN THỰC THI ---
    if (currentState == READY) {
        if (rxData.emergencyStop) {
            setMotor(0); // Dừng khẩn cấp
        }
        else {
            // Điều khiển động cơ chạy
            setMotor(rxData.speed);
            
            // Điều khiển Servo lái
            // Map góc lái (-100 đến 100) sang góc Servo (45 đến 135 độ)
            int angle = map(rxData.steer, -100, 100, 45, 135);
            steeringServo.write(constrain(angle, 45, 135));
        }
    } 
    else {
        // Nếu đang WAITING thì khóa mọi thứ
        setMotor(0); 
        steeringServo.write(90); // Trả lái về giữa
    }
}

// ================================================================
//                        SETUP (KHỞI TẠO)
// ================================================================

void setup() {
    // 1. Tắt Brownout Detector
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); 

    Serial.begin(115200);
  
    // 2. Cấu hình I2C cho cảm biến
    Wire.begin(); 
    Wire.setClock(400000);   
    Wire.setTimeOut(1000);   

    // 3. Cấu hình chân GPIO
    pinMode(LED_PIN, OUTPUT); 
    pinMode(BUZZER, OUTPUT);
    pinMode(AI1, OUTPUT); pinMode(AI2, OUTPUT); 
    pinMode(BI1, OUTPUT); pinMode(BI2, OUTPUT); 
    pinMode(STBY, OUTPUT);
    
    digitalWrite(STBY, HIGH); 
    digitalWrite(LED_PIN, LOW);

    // 4. Cấu hình PWM cho động cơ
    ledcAttachChannel(PWMA, 1000, 8, MOTOR_CHA); 
    ledcAttachChannel(PWMB, 1000, 8, MOTOR_CHB);
  
    // 5. Cấu hình Servo
    steeringServo.attach(17);

    // Test khởi động
    for (int i = 0; i < 3; i++) { 
        digitalWrite(LED_PIN, 1); delay(150); 
        digitalWrite(LED_PIN, 0); delay(150); 
    }
    steeringServo.write(135); delay(200); 
    steeringServo.write(45);  delay(200); 
    steeringServo.write(90);  delay(200);

    // 6. Khởi tạo Cảm biến MPU6050
    if (mpu.begin() != 0) { 
        Serial.println("Lỗi MPU"); 
    }
    else { 
        mpu.calcOffsets(); 
    }

    // 7. Khởi tạo Cảm biến khoảng cách VL53L0X
    sensor.setTimeout(500); 
    if (!sensor.init()) { 
        Serial.println("Lỗi VL53L0X"); 
    }
    else { 
        sensor.startContinuous(); 
    }

    // 8. Cấu hình WiFi & ESP-NOW
    WiFi.mode(WIFI_STA); 
    WiFi.disconnect();
    
    esp_wifi_set_promiscuous(true); 
    esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE); 
    esp_wifi_set_promiscuous(false);

    if (esp_now_init() != ESP_OK) {
        ESP.restart();
    }
    esp_now_register_recv_cb(OnDataRecv);

    // Thêm Peer (Tay cầm)
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, remoteMAC, 6);
    peerInfo.channel = 1; 
    peerInfo.encrypt = false;
    esp_now_add_peer(&peerInfo);

    // 9. KHỞI TẠO WATCHDOG TIMER (FIX CHO CORE V3.0)
    // Cấu hình tham số WDT
    esp_task_wdt_config_t twdt_config = {
        .timeout_ms = WDT_TIMEOUT * 1000, // Đổi giây sang ms
        .idle_core_mask = 0,              // Không theo dõi idle task
        .trigger_panic = true             // Reset khi hết giờ
    };
    
    // Khởi tạo với config mới
    esp_task_wdt_init(&twdt_config); 
    esp_task_wdt_add(NULL);               // Thêm Loop hiện tại vào WDT
}

// ================================================================
//                        MAIN LOOP
// ================================================================

void loop() {
    // 1. Reset Watchdog (Cho "chó" ăn)
    esp_task_wdt_reset(); 

    // 2. Cập nhật góc nghiêng
    mpu.update();

    // 3. Kiểm tra Failsafe
    if (millis() - lastRecvTime > 500) {
        if (isConnected) { 
            isConnected = false; 
            currentState = WAITING;
            setMotor(0); 
            steeringServo.write(90); 
            digitalWrite(LED_PIN, LOW);
        }
    }

    // 4. Gửi dữ liệu cảm biến
    static unsigned long lastSensorTime = 0;
    if (millis() - lastSensorTime > 50) {
        txData.pitch = mpu.getAngleX();
        txData.roll  = mpu.getAngleY();
        txData.yaw   = mpu.getAngleZ();
        txData.distance = getFiltered(sensor.readRangeContinuousMillimeters());
      
        esp_now_send(remoteMAC, (uint8_t*)&txData, sizeof(txData));
        
        lastSensorTime = millis();
    }
}