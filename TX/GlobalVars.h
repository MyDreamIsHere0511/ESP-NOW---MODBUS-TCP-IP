#ifndef GLOBALVARS_H
#define GLOBALVARS_H

#include <Adafruit_SSD1306.h>
#include "modbustcp.h"
#include "DataTypes.h"
#include "Config.h"

// --- Đối tượng phần cứng ---
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
uint16_t mbRegisters[10]; 
ModbusTCPServer modbus(mbRegisters, 10);

// --- Dữ liệu truyền thông ---
ControlData dataToSend;
SensorData dataReceived;

// --- Trạng thái ---
State currentState = WAITING;

// --- Biến Logic ---
unsigned long btnPressTime = 0;
bool isHoldingButtons = false;
bool actionPerformed = false;
bool isSignalGood = false; 
unsigned long lastRecvTime = 0; 

// --- Calibration ---
int zeroLX = 1800, zeroRY = 1800;

// --- Speed Limit ---
int speedLimits[] = {120, 180, 255}; 
int speedLimitIdx = 0;               
bool lastBtnRState = HIGH;           

#endif