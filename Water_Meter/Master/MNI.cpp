#include <Arduino.h>
#include "MNI.h"

MNI::MNI(HardwareSerial* serial,
         uint32_t baudRate,
         int8_t serialRx,
         int8_t serialTx)
{
  //Initialize private variables
  port = serial;
  port->begin(baudRate,SERIAL_8N1,serialRx,serialTx);
  rxDataCounter = 0;
  for(uint8_t i = 0; i < BufferSize::TX; i++)
  {
    txBuffer[i] = 0;
  }
  for(uint8_t i = 0; i < BufferSize::RX; i++)
  {
    rxBuffer[i] = 0;
  }    
}

void MNI::EncodeData(uint32_t dataToEncode,TxDataId id)
{
  uint8_t dataID = (uint8_t)id;
  //split 32-bit data into 4 bytes and store in the 'txBuffer'
  txBuffer[dataID] = (dataToEncode & 0xFF000000) >> 24; 
  txBuffer[dataID + 1] = (dataToEncode & 0x00FF0000) >> 16;
  txBuffer[dataID + 2] = (dataToEncode & 0x0000FF00) >> 8;
  txBuffer[dataID + 3] = (dataToEncode & 0x000000FF); 
}

void MNI::TransmitData(void)
{
  Serial.print("Tx buffer: ");
  for(uint8_t i = 0; i < BufferSize::TX; i++)
  {
    port->write(txBuffer[i]);
    //Debug 
    Serial.print(txBuffer[i]);
    Serial.print(' ');
  }
  Serial.print("\n");  
}

bool MNI::ReceivedData(void)
{
  bool rxDone = false;
  if(port->available())
  {
    if(rxDataCounter < BufferSize::RX)
    {
      rxBuffer[rxDataCounter] = port->read();
      rxDataCounter++; 
    }
  }
  if(rxDataCounter == BufferSize::RX)
  {
    rxDataCounter = 0;
    rxDone = true;
  }
  return rxDone;  
}

uint32_t MNI::DecodeData(RxDataId id)
{
  uint8_t dataID = (uint8_t)id;  
  //merge 4 bytes stored in 'rxBuffer' into 32-bit data
  return (rxBuffer[dataID] << 24) | (rxBuffer[dataID + 1] << 16) |
         (rxBuffer[dataID + 2] << 8) | (rxBuffer[dataID + 3]); 
}
