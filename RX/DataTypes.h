#ifndef DATATYPES_H
#define DATATYPES_H

#include <Arduino.h>

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

// Trạng thái hoạt động của xe
enum State { WAITING, READY };

#endif