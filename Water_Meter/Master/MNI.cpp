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
}

bool MNI::IsReceiverReady(uint8_t expectedNumOfBytes)
{
  return (port->available() == expectedNumOfBytes);
}

void MNI::TransmitData(void* dataBuffer,uint8_t dataSize)
{
  uint8_t* txBuffer = (uint8_t*)dataBuffer;
  for(uint8_t i = 0; i < dataSize; i++)
  {
    port->write(txBuffer[i]);
    Serial.print(txBuffer[i]);
    Serial.print(' ');
  } 
  Serial.print("\n");  
}

void MNI::ReceiveData(void* dataBuffer,uint8_t dataSize)
{
  uint8_t* rxBuffer = (uint8_t*)dataBuffer;
  for(uint8_t i = 0; i < dataSize; i++)
  {
    rxBuffer[i] = port->read();
  }  
}

