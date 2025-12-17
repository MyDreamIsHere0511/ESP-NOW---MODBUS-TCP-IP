#ifndef COMMS_H
#define COMMS_H

#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include "Config.h"
#include "GlobalVars.h"
#include "MotorControl.h" // Cần setMotor

// --- Callback nhận dữ liệu ---
void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
    memcpy(&rxData, data, sizeof(rxData));
    lastRecvTime = millis();
    isConnected = true;
    digitalWrite(LED_PIN, HIGH);

    // Chuyển trạng thái
    if (currentState == WAITING && rxData.command == 1) { 
        currentState = READY; 
        beep(2, 50); 
    }
    else if (currentState == READY && rxData.command == 2) { 
        setMotor(0);
        for (int i = 0; i < 3; i++) { 
            digitalWrite(LED_PIN, 0); delay(100); 
            digitalWrite(LED_PIN, 1); delay(100); 
        }
        currentState = WAITING; 
        beep(2, 100); 
    }

    // Điều khiển
    if (currentState == READY) {
        if (rxData.emergencyStop) {
            setMotor(0); 
        } else {
            setMotor(rxData.speed);
            int angle = map(rxData.steer, -100, 100, 45, 135);
            steeringServo.write(constrain(angle, 45, 135));
        }
    } else {
        setMotor(0); 
        steeringServo.write(90); 
    }
}

// --- Khởi tạo ESP-NOW ---
void initComms() {
    WiFi.mode(WIFI_STA); 
    WiFi.disconnect();
    
    esp_wifi_set_promiscuous(true); 
    esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE); 
    esp_wifi_set_promiscuous(false);

    if (esp_now_init() != ESP_OK) {
        ESP.restart();
    }
    esp_now_register_recv_cb(OnDataRecv);

    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, remoteMAC, 6);
    peerInfo.channel = 1; 
    peerInfo.encrypt = false;
    esp_now_add_peer(&peerInfo);
}

// --- Kiểm tra kết nối (Failsafe) ---
void checkFailsafe() {
    if (millis() - lastRecvTime > 500) {
        if (isConnected) { 
            isConnected = false; 
            currentState = WAITING;
            setMotor(0); 
            steeringServo.write(90); 
            digitalWrite(LED_PIN, LOW);
        }
    }
}

#endif