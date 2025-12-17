/*
 * PROJECT: XE ĐIỀU KHIỂN RC ESP32 (MODULAR VERSION)
 * CẤU TRÚC: Đã tách file .h
 */

#include <esp_task_wdt.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

// --- IMPORT CÁC MODULE CON ---
// Lưu ý: Thứ tự import rất quan trọng!
#include "Config.h"       // 1. Cấu hình
#include "DataTypes.h"    // 2. Kiểu dữ liệu
#include "GlobalVars.h"   // 3. Biến toàn cục
#include "MotorControl.h" // 4. Động cơ & Servo
#include "Sensors.h"      // 5. Cảm biến
#include "Comms.h"        // 6. Giao tiếp

void setup() {
    // 1. System Config
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); 
    Serial.begin(115200);

    // 2. Khởi tạo các module con
    initActuators(); // Động cơ, Servo, LED
    initSensors();   // MPU, VL53L0X
    initComms();     // WiFi, ESP-NOW

    // 3. Khởi tạo Watchdog (Core v3.0 fix)
    esp_task_wdt_config_t twdt_config = {
        .timeout_ms = WDT_TIMEOUT * 1000,
        .idle_core_mask = 0,
        .trigger_panic = true
    };
    esp_task_wdt_init(&twdt_config); 
    esp_task_wdt_add(NULL);
}

void loop() {
    // 1. Reset Watchdog
    esp_task_wdt_reset(); 

    // 2. Xử lý cảm biến & Gửi dữ liệu
    handleSensorData();

    // 3. Kiểm tra an toàn
    checkFailsafe();
    
    // Lưu ý: Logic nhận và điều khiển (setMotor) 
    // đã nằm trong hàm callback OnDataRecv (file Comms.h)
    // nên không cần gọi ở đây.
}