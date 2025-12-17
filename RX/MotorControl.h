#ifndef MOTORCONTROL_H
#define MOTORCONTROL_H

#include <Arduino.h>
#include "Config.h"
#include "GlobalVars.h" // Cần Servo

// --- Hàm phát tiếng bíp (Còi chíp) ---
void beep(int times, int duration) { 
    for (int i = 0; i < times; i++) {
        digitalWrite(BUZZER, 1); 
        delay(duration); 
        digitalWrite(BUZZER, 0); 
        delay(duration); 
    } 
}

// --- Hàm điều khiển động cơ ---
void setMotor(int8_t speed) {
    // Vùng chết (Deadzone)
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
    int pwm = map(abs(speed), 0, 100, 0, 255);
    ledcWrite(PWMA, pwm); 
    ledcWrite(PWMB, pwm);
}

// --- Hàm khởi tạo Motor & Servo (Gọi trong Setup) ---
void initActuators() {
    pinMode(LED_PIN, OUTPUT); 
    pinMode(BUZZER, OUTPUT);
    pinMode(AI1, OUTPUT); pinMode(AI2, OUTPUT); 
    pinMode(BI1, OUTPUT); pinMode(BI2, OUTPUT); 
    pinMode(STBY, OUTPUT);
    
    digitalWrite(STBY, HIGH); 
    digitalWrite(LED_PIN, LOW);

    ledcAttachChannel(PWMA, 1000, 8, MOTOR_CHA); 
    ledcAttachChannel(PWMB, 1000, 8, MOTOR_CHB);
  
    steeringServo.attach(17);

    // Test khởi động
    for (int i = 0; i < 3; i++) { 
        digitalWrite(LED_PIN, 1); delay(150); 
        digitalWrite(LED_PIN, 0); delay(150); 
    }
    steeringServo.write(135); delay(200); 
    steeringServo.write(45);  delay(200); 
    steeringServo.write(90);  delay(200);
}

#endif