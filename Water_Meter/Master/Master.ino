#include <Preferences.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h> //Version 1.1.2
#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h> //Version 1.4.6
#include "keypad.h"
#include "hmi.h"
#include "MNI.h"

/**
 * @brief Description of the storage of user-specific data in flash memory.
 * Each user has a default value for ID, PIN, and phone number. These values   
 * are to be changed by the user. The default values are hardcoded into the  
 * ESP32's flash memory such that:
 * 1. All IDs are stored in memory locations with labels "0","1", and "2".
 * 2. All PINs are stored in memory locations with labels "3","4", and "5".
 * 3. All phone numbers are stored in memory locations with labels "6","7", and "8".
*/

//Type(s)
enum UserIndex
{
  USER1 = 0,
  USER2,
  USER3
};
typedef struct
{
  float volume1;
  float volume2;
  float volume3;  
}sensor_t; 

//RTOS Handle(s)
TaskHandle_t nodeTaskHandle;
QueueHandle_t nodeToGetUnitsQueue;
//Object(s)
Preferences preferences; //for accessing ESP32 flash memory

void setup() 
{
  setCpuFrequencyMhz(80);
  Serial.begin(115200);
  preferences.begin("S-Meter",false); 
  nodeToGetUnitsQueue = xQueueCreate(1,sizeof(sensor_t));
  if(nodeToGetUnitsQueue != NULL)
  {
    Serial.println("Node-GetUnits Queue successfully created");
  }
  else
  {
    Serial.println("Node-GetUnits Queue failed");
  }
  xTaskCreatePinnedToCore(ApplicationTask,"",30000,NULL,2,NULL,1);
  xTaskCreatePinnedToCore(NodeTask,"",25000,NULL,1,&nodeTaskHandle,1);
  xTaskCreatePinnedToCore(UtilityTask,"",25000,NULL,1,NULL,1);
  //preferences.putBytes("0","123A",11);
  //preferences.putBytes("3","AABB",11);
  //preferences.putBytes("6","08155176712",12);
}

void loop() 
{
}

/**
 * @brief Handles main application logic
*/
void ApplicationTask(void* pvParameters)
{
  uint8_t rowPins[NUMBER_OF_ROWS] = {4,13,14,25};  
  uint8_t columnPins[NUMBER_OF_COLUMNS] = {26,27,32,33};  
  static LiquidCrystal_I2C lcd(0x27,20,4);
  static Keypad keypad(rowPins,columnPins); 
  static HMI hmi(&lcd,&keypad);
  hmi.RegisterCallback(ValidateLogin);
  hmi.RegisterCallback(GetPhoneNum);
  hmi.RegisterCallback(GetUnits);
  //Startup message
  lcd.init();
  lcd.backlight();
  lcd.print(" SMART WATER METER");
  lcd.setCursor(0,1);
  for(uint8_t i = 0; i < 20; i++)
  {
    lcd.print('-');
  }
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
    vTaskDelay(pdMS_TO_TICKS(50));
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
        Serial.println("--Received serial data from node\n");
        sensorData.volume1 = mni.DecodeData(MNI::RxDataId::USER1_VOLUME);
        sensorData.volume2 = mni.DecodeData(MNI::RxDataId::USER2_VOLUME);
        sensorData.volume3 = mni.DecodeData(MNI::RxDataId::USER3_VOLUME);
        //Debug
        Serial.print("User1 volume: ");
        Serial.println(sensorData.volume1); 
        Serial.print("User2 volume: ");
        Serial.println(sensorData.volume2); 
        Serial.print("User3 volume: ");
        Serial.println(sensorData.volume3);
        //Place sensor data in the Node-GetUnits Queue
        if(xQueueSend(nodeToGetUnitsQueue,&sensorData,0) == pdPASS)
        {
          Serial.println("--Data successfully sent to 'GetUnits' callback\n");
        }
        else
        {
          Serial.println("--Failed to send data to 'GetUnits' callback\n");
        }
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

/**
 * @brief Callback function that gets called when the user attempts
 * to log in after entering his/her ID and PIN using the HMI.
 * This function validates the login details of the user by comparing  
 * the ID and PIN entered via the HMI to the ID and PIN stored in the  
 * ESP32's flash.
 * 
 * @return Index unique to each user. This index can be used by other  
 * callback functions to access details specific to a user.  
*/
int ValidateLogin(char* id,uint8_t idSize,char* pin,uint8_t pinSize)
{
  int userIndex = -1;     
  const uint8_t numOfUsers = 3;
  for(uint8_t i = 0; i < numOfUsers; i++)
  { 
    char idFlash[idSize] = {0};
    char pinFlash[pinSize] = {0};
    char flashLoc[2] = {0};
    flashLoc[0] = '0' + i;
    preferences.getBytes(flashLoc,idFlash,idSize);
    flashLoc[0] = '3' + i;
    preferences.getBytes(flashLoc,pinFlash,pinSize);
    if(!strcmp(id,idFlash) && !strcmp(pin,pinFlash))
    {
      Serial.println("SUCCESS");
      Serial.print("Details found at index: ");
      Serial.println(i);
      userIndex = i;
      break;
    }
  }
  return userIndex;
}

/**
 * @brief Callback function that gets called after the user's ID and PIN  
 * have been validated. It gets the phone number of the user from the 
 * ESP32's flash.
 * 
 * @param userIndex: To determine the user whose information is required.
 * @param phoneNum: Buffer to store the phone number retrieved from the flash.  
 * @param phoneNumSize: Size of the 'phoneNum' buffer.
 * @return None
*/
void GetPhoneNum(int userIndex,char* phoneNum,uint8_t phoneNumSize)
{
  if(userIndex != USER1 && userIndex != USER2 && userIndex != USER3)
  {
    Serial.println("Could not get phone number");
    return; //invalid index
  }
  char flashLoc[2] = {0};
  flashLoc[0] = '6' + userIndex;
  preferences.getBytes(flashLoc,phoneNum,phoneNumSize);
}

/**
 * @brief Callback function that gets called when the HMI needs to display 
 * the available units (or volume of water) for a particular user. 
 * 
 * @param userIndex: To determine the user whose information is required.  
 * @param volumePtr: Points to the memory location that holds the available units  
 * for a user based on the 'userIndex'.
 * @return None
*/
void GetUnits(int userIndex,float* volumePtr)
{
  if(userIndex != USER1 && userIndex != USER2 && userIndex != USER3)
  {
    Serial.println("Could not get available units (or volume of water)");
    return; //invalid index
  }  
  sensor_t sensorData = {};
  if(xQueueReceive(nodeToGetUnitsQueue,&sensorData,0) != pdPASS)
  {
    return;
  }
  Serial.println("Node-GetUnits RX PASS");
  switch(userIndex)
  {
    case USER1:
      *volumePtr = sensorData.volume1;
      break;
    case USER2:
      *volumePtr = sensorData.volume2;
      break;
    case USER3:
      *volumePtr = sensorData.volume3;
      break;
  }
}

