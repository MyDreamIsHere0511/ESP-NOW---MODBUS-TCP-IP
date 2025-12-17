#ifndef MODBUSTCP_H
#define MODBUSTCP_H

#include <WiFi.h>

class ModbusTCPServer {
private:
    WiFiServer server;
    WiFiClient client;
    uint16_t* registers;
    int regCount;

public:
    // Khởi tạo Server Port 502
    ModbusTCPServer(uint16_t* regs, int count) : server(502), registers(regs), regCount(count) {}

    void begin() {
        server.begin();
    }

    void handle() {
        if (!client) {
            client = server.available(); // Chờ QModbusMaster kết nối
            return;
        }

        if (client.connected()) {
            if (client.available()) {
                // Buffer nhận dữ liệu
                uint8_t buf[256];
                int len = client.read(buf, 256);

                // Kiểm tra độ dài tối thiểu của gói tin Modbus TCP
                if (len > 7) {
                    uint8_t funcCode = buf[7];
                    
                    // Chỉ xử lý Function 03: Read Holding Registers
                    if (funcCode == 0x03) {
                        uint16_t startAddr = (buf[8] << 8) | buf[9];
                        uint16_t numRegs = (buf[10] << 8) | buf[11];

                        // Kiểm tra địa chỉ hợp lệ
                        if (startAddr + numRegs <= regCount) {
                            uint8_t response[256];
                            int idx = 0;

                            // 1. Transaction ID (Copy từ Request)
                            response[idx++] = buf[0]; response[idx++] = buf[1];
                            // 2. Protocol ID (00 00)
                            response[idx++] = buf[2]; response[idx++] = buf[3];
                            
                            // 3. Length (Sẽ điền sau) - tạm để trống
                            int lenIdx = idx;
                            idx += 2; 

                            // 4. Unit ID
                            response[idx++] = buf[6];
                            // 5. Function Code
                            response[idx++] = 0x03;
                            // 6. Byte Count
                            int byteCount = numRegs * 2;
                            response[idx++] = byteCount;

                            // 7. Data Registers
                            for (int i = 0; i < numRegs; i++) {
                                response[idx++] = (registers[startAddr + i] >> 8) & 0xFF; // High Byte
                                response[idx++] = registers[startAddr + i] & 0xFF;        // Low Byte
                            }

                            // Điền lại độ dài gói tin (Length = UnitID + Func + ByteCount + Data)
                            int packetLen = idx - 6; 
                            response[lenIdx] = (packetLen >> 8) & 0xFF;
                            response[lenIdx+1] = packetLen & 0xFF;

                            // Gửi phản hồi
                            client.write(response, idx);
                        }
                    }
                }
            }
        }
    }
};

#endif