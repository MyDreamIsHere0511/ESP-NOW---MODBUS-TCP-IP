/*
 * PROJECT: TAY CẦM ĐIỀU KHIỂN RC (MODULAR VERSION)
 * KẾT HỢP: ESP-NOW + WIFI AP + MODBUS TCP
 * PHIÊN BẢN: Đã tách file .h
 */

#include <Wire.h>

// --- IMPORT MODULES ---
// Thứ tự import quan trọng!
#include "Config.h"       // 1. Cấu hình
#include "DataTypes.h"    // 2. Struct
#include "modbustcp.h"    // 3. Thư viện Modbus
#include "GlobalVars.h"   // 4. Biến chung
#include "Inputs.h"       // 5. Joystick & Nút
#include "Display.h"      // 6. Màn hình
#include "Comms.h"        // 7. Mạng

void setup() {
    Serial.begin(115200);
    Wire.begin();
    Wire.setClock(400000); 

    // Khởi tạo lần lượt
    initInputs();   // LED, Joystick
    initDisplay();  // OLED
    initComms();    // WiFi, ESP-NOW, Modbus

    Serial.println("✅ TAY CẦM SẴN SÀNG!");
}

void loop() {
    // 1. Xử lý đầu vào (Đọc Joystick, Nút, Tính toán Speed Limit)
    handleInputs();

    // 2. Xử lý truyền thông (Gửi lệnh đi, Check timeout, Modbus Server)
    handleComms();

    // 3. Cập nhật hiển thị (10Hz)
    static unsigned long lastDisp = 0;
    if (millis() - lastDisp > 100) { 
        lastDisp = millis(); 
        updateDisplay(); 
    }
}