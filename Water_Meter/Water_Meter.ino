#include <Wire.h>
#include <LiquidCrystal_I2C.h> //Version 1.1.2
#include "keypad.h"
#include "hmi.h"

void setup() 
{
  // put your setup code here, to run once:
  setCpuFrequencyMhz(80);
  Serial.begin(115200);
  xTaskCreatePinnedToCore(ApplicationTask,"",30000,NULL,1,NULL,0);
  xTaskCreatePinnedToCore(FlowSensorTask,"",25000,NULL,1,NULL,1);
}

void loop() 
{
  // put your main code here, to run repeatedly:
}

void ApplicationTask(void* pvParameters)
{
  uint8_t rowPins[NUMBER_OF_ROWS] = {18,19,23,25};  
  uint8_t columnPins[NUMBER_OF_COLUMNS] = {26,27,32,33};  
  static LiquidCrystal_I2C lcd(0x27,20,4);
  static Keypad keypad(rowPins,columnPins); 
  static HMI hmi(&lcd,&keypad);
    
  while(1)
  {
    
  }
}

void FlowSensorTask(void* pvParameters)
{
  while(1)
  {
    
  }
}

