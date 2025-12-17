#ifndef SENSORS_H
#define SENSORS_H

#include <Arduino.h>       // Để dùng millis(), Serial...
#include <esp_now.h>       // <--- THÊM: Để dùng hàm esp_now_send
#include <WiFi.h>          // <--- THÊM: Thư viện nền tảng cho ESP-NOW

#include "Config.h"        // Để lấy địa chỉ remoteMAC
#include "GlobalVars.h"    // Để lấy biến txData, mpu, sensor

// --- Bộ lọc trung bình trượt ---
// (Giữ nguyên logic cũ của bạn)
// Lưu ý: fBuf và fIdx nên để static nếu nằm trong .h để tránh lỗi đa định nghĩa, 
// nhưng với cách include hiện tại thì tạm thời để nguyên vẫn chạy được.
static int fBuf[5]; 
static int fIdx = 0;

int getFiltered(int val) {
    if (val > 2000 || val == 65535) val = 2000;
    fBuf[fIdx] = val; 
    fIdx = (fIdx + 1) % 5;
    long s = 0; 
    for (int i = 0; i < 5; i++) s += fBuf[i];
    return s / 5;
}

// --- Hàm khởi tạo cảm biến ---
void initSensors() {
    // I2C Init
    Wire.begin(); 
    Wire.setClock(400000);   
    Wire.setTimeOut(1000); 

    // MPU6050
    if (mpu.begin() != 0) { 
        Serial.println("Lỗi MPU"); 
    } else { 
        mpu.calcOffsets(); 
    }

    // VL53L0X
    sensor.setTimeout(500); 
    if (!sensor.init()) { 
        Serial.println("Lỗi VL53L0X"); 
    } else { 
        sensor.startContinuous(); 
    }
}

// --- Hàm đọc và gửi dữ liệu cảm biến (Gọi trong Loop) ---
void handleSensorData() {
    mpu.update(); // Cần gọi liên tục

    static unsigned long lastSensorTime = 0;
    if (millis() - lastSensorTime > 50) {
        txData.pitch = mpu.getAngleX();
        txData.roll  = mpu.getAngleY();
        txData.yaw   = mpu.getAngleZ();
        txData.distance = getFiltered(sensor.readRangeContinuousMillimeters());
      
        // Lỗi cũ nằm ở đây, giờ đã có thư viện esp_now.h nên sẽ chạy OK
        esp_now_send(remoteMAC, (uint8_t*)&txData, sizeof(txData));
        
        lastSensorTime = millis();
    }
}

#endif