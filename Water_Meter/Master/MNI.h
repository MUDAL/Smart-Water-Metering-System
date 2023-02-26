#pragma once

//MNI: Master-Node-Interface
//Handles serial communication between Master (ESP32) and the Node(Nano)

class MNI
{
  private:
    HardwareSerial* port;
    
  public:
    MNI(HardwareSerial* serial,
        uint32_t baudRate = 9600,
        int8_t serialRx = -1,
        int8_t serialTx = -1);
    bool IsReceiverReady(uint8_t expectedNumOfBytes);
    void TransmitData(void* dataBuffer,uint8_t dataSize);
    void ReceiveData(void* dataBuffer,uint8_t dataSize);      
};
