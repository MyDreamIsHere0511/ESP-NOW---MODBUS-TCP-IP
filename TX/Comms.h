#ifndef COMMS_H
#define COMMS_H

#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include "GlobalVars.h"

// --- Callback khi gửi xong (Không làm gì nhưng cần khai báo) ---
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {}

// --- Callback khi nhận dữ liệu từ xe ---
void OnDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *incomingData, int len) {
    if (len == sizeof(SensorData)) {
        memcpy(&dataReceived, incomingData, sizeof(SensorData));
        lastRecvTime = millis();
        isSignalGood = true;

        // Cập nhật thanh ghi Modbus ngay khi có dữ liệu mới
        // Reg 3,4,5: Góc nghiêng (x10)
        mbRegisters[3] = (int16_t)(dataReceived.pitch * 10);
        mbRegisters[4] = (int16_t)(dataReceived.roll * 10);
        mbRegisters[5] = (int16_t)(dataReceived.yaw * 10);
        // Reg 6: Khoảng cách (mm -> cm)
        mbRegisters[6] = dataReceived.distance / 10; 
    }
}

// --- Khởi tạo WiFi & ESP-NOW ---
void initComms() {
    // WiFi AP + STA
    WiFi.mode(WIFI_AP_STA);
    esp_wifi_set_ps(WIFI_PS_NONE);
    WiFi.softAP("RC_CONTROLLER", "12345678", 1, 0);
    
    // Fix Channel 1
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_promiscuous(false);

    if (esp_now_init() != ESP_OK) {
        ESP.restart();
    }
    
    esp_now_register_recv_cb(OnDataRecv);
    esp_now_register_send_cb(OnDataSent);
    
    // Đăng ký Peer (Xe)
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, receiverMAC, 6);
    peerInfo.channel = 1; 
    peerInfo.encrypt = false;
    esp_now_add_peer(&peerInfo);

    // Khởi động Modbus Server
    modbus.begin();
}

// --- Xử lý gửi tin & Cập nhật Modbus còn lại ---
void handleComms() {
    unsigned long now = millis();

    // 1. Kiểm tra Watchdog (Mất kết nối)
    if (now - lastRecvTime > 800) {
        isSignalGood = false;
        if (currentState == READY) {
            currentState = WAITING;
            digitalWrite(LED_RED, HIGH); 
            digitalWrite(LED_GREEN, LOW);
        }
    }

    // 2. Gửi dữ liệu đi (Tần số gửi 50Hz)
    static unsigned long lastSend = 0;
    if (now - lastSend > 20) {
        lastSend = now;
        
        // Gán Timestamp
        dataToSend.timestamp = now;
        esp_now_send(receiverMAC, (uint8_t *)&dataToSend, sizeof(dataToSend));

        // Cập nhật các thanh ghi Modbus phần điều khiển
        // Reg 0: Trạng thái
        mbRegisters[0] = currentState; 
        // Reg 1, 2: Lái & Tốc độ (Offset 100)
        mbRegisters[1] = dataToSend.steer + 100;  
        mbRegisters[2] = dataToSend.speed + 100;
    }
    
    // 3. Xử lý yêu cầu Modbus TCP từ máy tính
    modbus.handle();
}

#endif