#include <Wire.h>
#include <LiquidCrystal_I2C.h> //Version 1.1.2
#include "keypad.h"
#include "hmi.h"
#include "MNI.h"

void setup() 
{
  // put your setup code here, to run once:
  setCpuFrequencyMhz(80);
  Serial.begin(115200);
  xTaskCreatePinnedToCore(ApplicationTask,"",30000,NULL,1,NULL,1);
  xTaskCreatePinnedToCore(NodeTask,"",25000,NULL,1,NULL,1);
  xTaskCreatePinnedToCore(UtilityTask,"",25000,NULL,1,NULL,1);
}

void loop() 
{
  // put your main code here, to run repeatedly:
}

void ApplicationTask(void* pvParameters)
{
  uint8_t rowPins[NUMBER_OF_ROWS] = {4,13,14,25};  
  uint8_t columnPins[NUMBER_OF_COLUMNS] = {26,27,32,33};  
  static LiquidCrystal_I2C lcd(0x27,20,4);
  static Keypad keypad(rowPins,columnPins); 
  static HMI hmi(&lcd,&keypad);
    
  while(1)
  {
    
  }
}

/**
 * @brief Handles communication between the master and node.
 * NB: Master + Node = Meter
*/
void NodeTask(void* pvParameters)
{
  while(1)
  {
    
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

