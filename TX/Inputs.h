#ifndef INPUTS_H
#define INPUTS_H

#include "Config.h"
#include "GlobalVars.h"

// --- Hàm khởi tạo Input & Calibration ---
void initInputs() {
    pinMode(LED_RED, OUTPUT); 
    pinMode(LED_GREEN, OUTPUT);
    pinMode(VRXL, INPUT); pinMode(VRYL, INPUT); pinMode(SWL, INPUT_PULLUP);
    pinMode(VRXR, INPUT); pinMode(VRYR, INPUT); pinMode(SWR, INPUT_PULLUP);
    
    digitalWrite(LED_RED, HIGH); 
    digitalWrite(LED_GREEN, LOW);

    // Auto Calibration
    long sumX = 0, sumY = 0;
    for (int i = 0; i < 20; i++) { 
        sumX += analogRead(VRXL); 
        sumY += analogRead(VRYR); 
        delay(5); 
    }
    zeroLX = sumX / 20; 
    zeroRY = sumY / 20;
}

// --- Hàm xử lý logic đầu vào (Gọi trong Loop) ---
void handleInputs() {
    unsigned long now = millis();

    // 1. Đọc nút bấm
    bool btnL = !digitalRead(SWL); 
    bool btnR = !digitalRead(SWR);
    dataToSend.command = 0;

    // 2. Logic: Hai nút cùng lúc (Chuyển chế độ WAITING <-> READY)
    if (btnL && btnR) { 
        if (!isHoldingButtons) { 
            isHoldingButtons = true; 
            btnPressTime = now; 
            actionPerformed = false; 
        }
        
        if (isSignalGood && now - btnPressTime > 3000 && !actionPerformed) {
            actionPerformed = true;
            if (currentState == WAITING) {
                currentState = READY; 
                dataToSend.command = 1; 
                digitalWrite(LED_RED, LOW); digitalWrite(LED_GREEN, HIGH);
            } else {
                currentState = WAITING; 
                dataToSend.command = 2; 
                digitalWrite(LED_RED, HIGH); digitalWrite(LED_GREEN, LOW);
            }
        }
    } 
    // 3. Logic: Nút đơn lẻ
    else { 
        isHoldingButtons = false; 
        actionPerformed = false; 
        
        if (currentState == READY) {
            // Nút Phải: Chuyển Speed Limit (Bắt sườn xung xuống)
            if (btnR && lastBtnRState == LOW) { 
                // Logic debounce đơn giản bằng logic vòng lặp
            }
            if (btnR && !lastBtnRState) { // Mới nhấn
                speedLimitIdx++;
                if (speedLimitIdx > 2) speedLimitIdx = 0;
            }
            lastBtnRState = btnR; 
        }
    }

    // 4. Tính toán Joystick
    int rawLX = analogRead(VRXL); 
    int rawRY = analogRead(VRYR);
    int8_t steer = constrain(map(rawLX - zeroLX, -zeroLX, 4095 - zeroLX, -100, 100), -100, 100);
    int8_t speedPct = constrain(map(rawRY - zeroRY, -zeroRY, 4095 - zeroRY, -100, 100), -100, 100);
    
    if (abs(steer) < 15) steer = 0; 
    if (abs(speedPct) < 15) speedPct = 0;

    // 5. Tổng hợp dữ liệu gửi đi
    if (currentState == READY && isSignalGood) {
        // Nút Trái: Phanh gấp
        if (btnL && !btnR) { 
            dataToSend.emergencyStop = true; 
            dataToSend.speed = 0;
        } else {
            dataToSend.emergencyStop = false;
            
            // Scaling Tốc độ theo Limit
            float limitFactor = (float)speedLimits[speedLimitIdx] / 255.0; 
            dataToSend.speed = (int8_t)(speedPct * limitFactor); 
            dataToSend.steer = steer;
        }
    } else {
        // Chế độ chờ: Không làm gì
        dataToSend.emergencyStop = false; 
        dataToSend.speed = 0; 
        dataToSend.steer = 0;
    }
}

#endif