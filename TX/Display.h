#ifndef DISPLAY_H
#define DISPLAY_H

#include "GlobalVars.h"

void initDisplay() {
    display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
    display.clearDisplay();
    display.display();
}

void updateDisplay() {
    display.clearDisplay();
    display.setTextSize(1); 
    display.setTextColor(SSD1306_WHITE); 
    display.setCursor(0, 0);

    if (isSignalGood) {
        display.print("KET NOI:  [TOT]");
        
        // Góc nghiêng
        display.setCursor(0, 12); 
        display.printf("P: %-4.0f R:%-4.0f Y: %-4.0f", 
                       dataReceived.pitch, dataReceived.roll, dataReceived.yaw);

        // Trạng thái + Speed Limit
        display.setCursor(0, 24);
        if (currentState == READY) {
            display.printf("S:%-3d L:%-3d", dataToSend.speed, speedLimits[speedLimitIdx]);
        } else {
            if (isHoldingButtons) {
                long ms = millis() - btnPressTime;
                display.printf("GIU NUT: %ld ms", (ms > 3000) ? 3000 : ms);
            } else {
                display.print("CHE DO: CHO (KHOA)");
            }
        }

        // Khoảng cách
        display.setCursor(0, 40); 
        display.setTextSize(2); 
        display.printf("%3d cm", dataReceived.distance / 10);
        
    } else {
        display.print("MAT KET NOI !!!");
        display.setCursor(0, 20); 
        display.print("Dang tim lai xe...");
        display.setCursor(0, 35); 
        display.print("Kiem tra pin xe");
    }
    
    display.display();
}

#endif