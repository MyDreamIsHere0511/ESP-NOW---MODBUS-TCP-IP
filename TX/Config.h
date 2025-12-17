#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ===== CẤU HÌNH MÀN HÌNH =====
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// ===== CẤU HÌNH CHÂN (PINS) =====
#define LED_RED     14 
#define LED_GREEN   27 

// Joystick Trái (Lái)
#define VRXL        36
#define VRYL        39
#define SWL         33

// Joystick Phải (Tốc độ)
#define VRXR        35
#define VRYR        34
#define SWR         32

// ===== CẤU HÌNH LOGIC =====
#define DEAD_ZONE 20

// Địa chỉ MAC của Xe (Receiver)
const uint8_t receiverMAC[] = {0x88, 0x57, 0x21, 0xAC, 0xA7, 0xA0}; 

#endif