#include <Wire.h>
#include <LiquidCrystal_I2C.h> //Version 1.1.2
#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h> //Version 1.4.6
#include "keypad.h"
#include "hmi.h"
#include "MNI.h"

//Type(s)
typedef struct
{
  float volume1;
  float volume2;
  float volume3;  
}sensor_t;

//RTOS Handle(s)
TaskHandle_t nodeTaskHandle;

void setup() 
{
  setCpuFrequencyMhz(80);
  Serial.begin(115200);
  xTaskCreatePinnedToCore(ApplicationTask,"",30000,NULL,1,NULL,1);
  xTaskCreatePinnedToCore(NodeTask,"",25000,NULL,1,&nodeTaskHandle,1);
  xTaskCreatePinnedToCore(UtilityTask,"",25000,NULL,1,NULL,1);
}

void loop() 
{
}

void ApplicationTask(void* pvParameters)
{
  uint8_t rowPins[NUMBER_OF_ROWS] = {4,13,14,25};  
  uint8_t columnPins[NUMBER_OF_COLUMNS] = {26,27,32,33};  
  static LiquidCrystal_I2C lcd(0x27,20,4);
  static Keypad keypad(rowPins,columnPins); 
  static HMI hmi(&lcd,&keypad);

  //Startup message
  lcd.init();
  lcd.backlight();
  lcd.print(" SMART WATER METER");
  lcd.setCursor(5,1);
  lcd.print("BY OLTOSIN");
  vTaskDelay(pdMS_TO_TICKS(1500));
  lcd.setCursor(0,2);
  lcd.print("STATUS: ");
  lcd.setCursor(0,3);
  lcd.print("INITIALIZING...");
  vTaskDelay(pdMS_TO_TICKS(1500));
  lcd.clear();
  vTaskResume(nodeTaskHandle);
    
  while(1)
  {
    hmi.Start(); 
  }
}

/**
 * @brief Handles communication between the master and node.
 * NB: Master + Node = Meter
*/
void NodeTask(void* pvParameters)
{
  vTaskSuspend(NULL);
  static MNI mni(&Serial2);
  static sensor_t sensorData;
  //Initial request for sensor data from the node
  mni.EncodeData(MNI::QUERY,MNI::TxDataId::DATA_QUERY);
  mni.TransmitData();  
  uint32_t prevTime = millis();
      
  while(1)
  {
    //Request for sensor data from the node (periodically)
    if((millis() - prevTime) >= 2500)
    {
      mni.EncodeData(MNI::QUERY,MNI::TxDataId::DATA_QUERY);
      mni.TransmitData();
      prevTime = millis();
    }
    //Decode data received from node
    if(mni.ReceivedData())
    {
      if(mni.DecodeData(MNI::RxDataId::DATA_ACK) == MNI::ACK)
      {
        /*TO-DO: Add code to decode data from node and send to other tasks
         * that need the data (via Queues).
        */
      }
    }
  }
}

/**
 * @brief Handles communication between the meter and utility
 * system.
*/
void UtilityTask(void* pvParameters)
{
  while(1)
  {
    
  }
}

