#ifndef DATATYPES_H
#define DATATYPES_H

#include <Arduino.h>

// Dữ liệu gửi đi (Control Data)
typedef struct { 
    int8_t steer; 
    int8_t speed; 
    bool emergencyStop; 
    uint8_t command; 
    uint32_t timestamp; 
} ControlData;

// Dữ liệu nhận về (Sensor Data)
typedef struct { 
    float pitch; 
    float roll; 
    float yaw; 
    uint16_t distance; 
} SensorData;

// Trạng thái hoạt động
enum State { WAITING, READY };

#endif