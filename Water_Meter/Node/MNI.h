#pragma once

//MNI: Master-Node-Interface
//Handles serial communication between Master (ESP32) and the Node(Nano)
//Features: Bi-directional communication, 32-bit encoded/decoded data.

class MNI
{
  private:
    enum BufferSize {TX = 16, RX = 4};
    SoftwareSerial* port;
    uint8_t rxDataCounter;
    uint8_t txBuffer[BufferSize::TX];
    uint8_t rxBuffer[BufferSize::RX];
    
  public:
    enum {QUERY = 0xAA, ACK = 0xBB};
    enum TxDataId
    {
      DATA_ACK = 0,
      USER1_VOLUME = 4,
      USER2_VOLUME = 8,
      USER3_VOLUME = 12
    };
    enum RxDataId {DATA_QUERY = 0};
    
    MNI(SoftwareSerial* serial,uint32_t baudRate = 9600);
    void EncodeData(uint32_t dataToEncode,TxDataId id);
    void TransmitData(void);
    bool ReceivedData(void);
    uint32_t DecodeData(RxDataId id);    
};
