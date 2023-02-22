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
typedef struct
{
  float volume1;
  float volume2;
  float volume3;  
}sensor_t; //mL

typedef struct
{
  char phoneNum[SIZE_PHONE];
  uint32_t units;
}recharge_t;

typedef struct
{
  recharge_t recharge;
  sensor_t sensorData;
}meter_utility_t;

TaskHandle_t nodeTaskHandle;
QueueHandle_t nodeToGetUnitsQueue;
QueueHandle_t nodeToUtilityQueue;
QueueHandle_t requestToUtilityQueue;
Preferences preferences; //for accessing ESP32 flash memory

void setup() 
{
  setCpuFrequencyMhz(80);
  Serial.begin(115200);
  preferences.begin("S-Meter",false); 
  nodeToGetUnitsQueue = xQueueCreate(1,sizeof(sensor_t));
  nodeToUtilityQueue = xQueueCreate(1,sizeof(sensor_t));
  requestToUtilityQueue = xQueueCreate(1,sizeof(recharge_t));
  if(nodeToGetUnitsQueue != NULL)
  {
    Serial.println("Node-GetUnits Queue successfully created");
  }
  if(nodeToUtilityQueue != NULL)
  {
    Serial.println("Node-Utility Queue successfully created");
  }
  if(requestToUtilityQueue != NULL)
  {
    Serial.println("Request-Utility Queue successfully created");
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
  hmi.RegisterCallback(StoreUserParam);
  hmi.RegisterCallback(HandleRecharge);
  
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
        //Place sensor data in the appropriate Queue(s)
        if(xQueueSend(nodeToGetUnitsQueue,&sensorData,0) == pdPASS)
        {
          Serial.println("--Data successfully sent to 'GetUnits' callback\n");
        }
        if(xQueueSend(nodeToUtilityQueue,&sensorData,0) == pdPASS)
        {
          Serial.println("--Data successfully sent to Utility task\n");
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
  const uint8_t chipEn = 15;
  const uint8_t chipSel = 5; 
  const byte addr[][6] = {"00001","00002"};
  static RF24 nrf24(chipEn,chipSel);
  static meter_utility_t meterToUtil;
  
  nrf24.begin();
  nrf24.openWritingPipe(addr[1]);
  nrf24.openReadingPipe(1,addr[0]);
  nrf24.setPALevel(RF24_PA_MAX);
  uint32_t prevTime = millis();
  
  while(1)
  {
    if(xQueueReceive(nodeToUtilityQueue,&meterToUtil.sensorData,0) == pdPASS)
    {
      Serial.println("Node-Util RX PASS\n");
    }
    if(xQueueReceive(requestToUtilityQueue,&meterToUtil.recharge,0) == pdPASS)
    {
      Serial.println("Request-Util RX PASS\n");
    }
    if((millis() - prevTime) >= 3000)
    {
      nrf24.stopListening();
      nrf24.write(&meterToUtil,sizeof(meterToUtil)); 
      nrf24.startListening();
      //Clear phone number and units required
      memset(meterToUtil.recharge.phoneNum,'\0',SIZE_PHONE);
      meterToUtil.recharge.units = 0;
      prevTime = millis();
    }
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
UserIndex ValidateLogin(char* id,uint8_t idSize,char* pin,uint8_t pinSize)
{     
  const uint8_t numOfUsers = 3;
  const UserIndex indexArray[numOfUsers] = {USER1,USER2,USER3};
  UserIndex userIndex = USER_UNKNOWN;
  
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
      userIndex = indexArray[i];
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
void GetPhoneNum(UserIndex userIndex,char* phoneNum,uint8_t phoneNumSize)
{
  if(userIndex == USER_UNKNOWN)
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
void GetUnits(UserIndex userIndex,float* volumePtr)
{
  if(userIndex == USER_UNKNOWN)
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

/**
 * @brief Callback function stores the new value of a parameter 
 * (ID, PIN, or Phone number) in the ESP32's flash. It is called when 
 * the user uses the HMI to save the parameter.
 * 
 * @param userIndex: To determine the user whose information is required.  
 * @param paramType: Type of the parameter to be stored.  
 * @param param: Buffer containing the new value of the parameter to be stored. 
 * @param paramSize: Size of the 'param' buffer.
 * @return true if parameter was successfully stored.
 *         false if parameter was not stored (occurs if new parameter is same as the old one).
*/
bool StoreUserParam(UserIndex userIndex,UserParam paramType,
                    char* param,uint8_t paramSize)
{  
  if(userIndex == USER_UNKNOWN)
  {
    Serial.println("Storage Error: Invalid user");
    return false; //invalid index
  }
  bool isParamStored = false; 
  char paramFlash[paramSize]; //to hold previous value of 'param' stored in flash.
  char flashLoc[2] = {0};
  
  switch(paramType)
  {
    case ID:
      flashLoc[0] = '0' + userIndex;
      break;
    case PIN:
      flashLoc[0] = '3' + userIndex;
      break;
    case PHONE:
      flashLoc[0] = '6' + userIndex;
      break;
  }
  preferences.getBytes(flashLoc,paramFlash,paramSize); 
  if(strcmp(param,paramFlash))
  {
    preferences.putBytes(flashLoc,param,paramSize);
    Serial.println("Stored parameter in Flash");
    isParamStored = true;
  }
  return isParamStored;
}

/**
 * @brief Callback function that is called when a user requests  
 * for more units (via the HMI) in order to recharge. It sends the  
 * user's phone number and 'units required for recharge' to the 
 * 'Utility task'
 * 
 * @param userIndex: To determine the user whose information is required.
 * @param unitsRequired: Number of units required by the user. 
 * for recharge.  
 * @return true if recharge request has been sent to the utility task. 
 *         false if otherwise.  
 *                                                                        
*/
bool HandleRecharge(UserIndex userIndex,uint32_t unitsRequired)
{
  if(userIndex == USER_UNKNOWN)
  {
    Serial.println("Request Error: Invalid user");
    return false; //invalid index
  }   
  bool requestSent = false; 
  char flashLoc[2] = {0};
  flashLoc[0] = '6' + userIndex; //to get phone number's location in flash memory
  recharge_t recharge = {};
  
  preferences.getBytes(flashLoc,recharge.phoneNum,SIZE_PHONE);
  recharge.units = unitsRequired;
  if(xQueueSend(requestToUtilityQueue,&recharge,0) == pdPASS)
  {
    requestSent = true;
    Serial.println("Request-Util TX PASS\n");
  }
  return requestSent;  
}

