#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ================================================================
//                        CẤU HÌNH PHẦN CỨNG
// ================================================================

// Cấu hình Watchdog: 1 giây
#define WDT_TIMEOUT 1 

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
//                        THÔNG SỐ MẠNG
// ================================================================
// Địa chỉ MAC của Tay Cầm Điều Khiển
// Lưu ý: const uint8_t để an toàn hơn #define
const uint8_t remoteMAC[] = {0xF8, 0xB3, 0xB7, 0x20, 0x94, 0x78};

#endif