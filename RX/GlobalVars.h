#ifndef GLOBALVARS_H
#define GLOBALVARS_H

#include <ESP32Servo.h>
#include <Wire.h>
#include <MPU6050_light.h>
#include <VL53L0X.h>
#include "DataTypes.h" // Cần file này để hiểu ControlData/SensorData

// Khởi tạo các đối tượng phần cứng
Servo steeringServo;
MPU6050 mpu(Wire);
VL53L0X sensor;

// Biến lưu trữ dữ liệu truyền thông
ControlData rxData;
SensorData txData;

// Trạng thái hệ thống
State currentState = WAITING;

// Biến logic
unsigned long lastRecvTime = 0;
bool isConnected = false;

#endif