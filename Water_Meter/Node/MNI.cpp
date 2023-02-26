#include <Arduino.h>
#include <SoftwareSerial.h>
#include "MNI.h"

MNI::MNI(SoftwareSerial* serial,uint32_t baudRate)
{
  //Initialize private variables
  port = serial;
  port->begin(baudRate);   
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
