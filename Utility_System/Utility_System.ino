#include <WiFi.h>
#include <Preferences.h>
#include <WiFiManager.h> //Version 2.0.12-beta (tzapu)
#include <PubSubClient.h>
#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h> //Version 1.4.6
#include "sim800l.h"

#define SIZE_TOPIC            30 //MQTT topic: Max number of characters
//Number of characters (including NULL)
#define SIZE_PHONE            12 
#define SIZE_OTP              11 
#define SIZE_REQUEST          11 //for units requested by the user

//Meter(Master) -> Utility
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
}recharge_util_t;

typedef struct
{
  recharge_util_t recharge;
  sensor_t sensorData;
}meter_util_t;

//Queues
typedef struct
{
  QueueHandle_t utilToMqtt;
  QueueHandle_t utilToApp1;
  QueueHandle_t utilToApp2;
}queue_t;

WiFiManagerParameter subTopic("5","HiveMQ Subscription topic","",SIZE_TOPIC);
Preferences preferences; //for accessing ESP32 flash memory
queue_t queue;
TaskHandle_t wifiTaskHandle;
uint32_t setupTime;

/**
 * @brief Store new data in specified location in ESP32's 
 * flash memory if the new ones are different from the old ones.  
*/
static void StoreNewFlashData(const char* flashLoc,const char* newData,
                              const char* oldData,uint8_t dataSize)
{
  if(strcmp(newData,"") && strcmp(newData,oldData))
  {
    preferences.putBytes(flashLoc,newData,dataSize);
  }
}

/**
 * @brief Generates OTP using a Random Number Generator  
 * (RNG).
*/
static void RandomizeOtp(char* otp)
{
  static bool isRngReady = false; //to setup RNG
  const char chars[] = "0123456789*ABCD";
  uint8_t len = strlen(chars);

  if(!isRngReady)
  {
    randomSeed(micros() - setupTime);
    isRngReady = true;
  }
  for(uint8_t i = 0; i < SIZE_OTP - 1; i++)
  {
    uint8_t randIndex = random(len);
    otp[i] = chars[randIndex];
  }
  otp[SIZE_OTP - 1] = '\0';
}

/**
 * @brief Send auto-generated OTP to the meter of the user that 
 * just sent a request to recharge.
*/
static void SendOtpToMeter(RF24& nrf24,char* otp)
{
  nrf24.stopListening();
  nrf24.write(otp,SIZE_OTP);
  nrf24.startListening();
}

/**
 * @brief Converts an integer to a string.
*/
static void IntegerToString(uint32_t integer,char* stringPtr)
{
  if(integer == 0)
  {  
    stringPtr[0] = '0';
    return;
  }
  uint32_t integerCopy = integer;
  uint8_t numOfDigits = 0;

  while(integerCopy > 0)
  {
    integerCopy /= 10;
    numOfDigits++;
  }
  while(integer > 0)
  {
    stringPtr[numOfDigits - 1] = '0' + (integer % 10);
    integer /= 10;
    numOfDigits--;
  }
}

/**
 * @brief Converts a float to a string.
*/
static void FloatToString(float floatPt,char* stringPtr,uint32_t multiplier)
{
  uint32_t floatAsInt = lround(floatPt * multiplier);
  char quotientBuff[20] = {0};
  char remainderBuff[20] = {0};
  IntegerToString((floatAsInt / multiplier),quotientBuff);
  IntegerToString((floatAsInt % multiplier),remainderBuff);
  strcat(stringPtr,quotientBuff);
  strcat(stringPtr,".");
  strcat(stringPtr,remainderBuff);
}

void setup() 
{
  setCpuFrequencyMhz(80);
  Serial.begin(115200);  
  preferences.begin("Utility",false);
  queue.utilToMqtt = xQueueCreate(1,sizeof(sensor_t));
  queue.utilToApp1 = xQueueCreate(1,sizeof(recharge_util_t));
  queue.utilToApp2 = xQueueCreate(1,SIZE_OTP);
  if(queue.utilToMqtt != NULL && queue.utilToApp1 != NULL && queue.utilToApp2 != NULL)
  {
    Serial.println("Queues successfully created");
  }  
  xTaskCreatePinnedToCore(WiFiManagementTask,"",7000,NULL,1,&wifiTaskHandle,1);
  xTaskCreatePinnedToCore(MqttTask,"",7000,NULL,1,NULL,1);
  xTaskCreatePinnedToCore(ApplicationTask,"",20000,NULL,1,NULL,1); 
  xTaskCreatePinnedToCore(MeterTask,"",20000,NULL,1,NULL,1);  
  setupTime = micros();
}

void loop() 
{
}

/**
 * @brief Manages WiFi configurations (STA and AP modes). Connects
 * to an existing/saved network if available, otherwise it acts as
 * an AP in order to receive new network credentials.
*/
void WiFiManagementTask(void* pvParameters)
{
  const uint16_t accessPointTimeout = 50000; //millisecs
  static WiFiManager wm;
  WiFi.mode(WIFI_STA);  
  wm.addParameter(&subTopic);
  wm.setConfigPortalBlocking(false);
  wm.setSaveParamsCallback(WiFiManagerCallback);   
  //Auto-connect to previous network if available.
  //If connection fails, ESP32 goes from being a station to being an access point.
  Serial.print(wm.autoConnect("UTILITY")); 
  Serial.println("-->WiFi status");   
  bool accessPointMode = false;
  uint32_t startTime = 0;    
  
  while(1)
  {
    wm.process();
    if(WiFi.status() != WL_CONNECTED)
    {
      if(!accessPointMode)
      {
        if(!wm.getConfigPortalActive())
        {
          wm.autoConnect("UTILITY"); 
        }
        accessPointMode = true; 
        startTime = millis(); 
      }
      else
      {
        //reset after a timeframe (device shouldn't spend too long as an access point)
        if((millis() - startTime) >= accessPointTimeout)
        {
          Serial.println("\nAP timeout reached, system will restart for better connection");
          vTaskDelay(pdMS_TO_TICKS(1000));
          esp_restart();
        }
      }
    }
    else
    {
      if(accessPointMode)
      {   
        accessPointMode = false;
        Serial.println("Successfully connected, system will restart now");
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_restart();
      }
    }    
  }
}

/**
 * @brief Handles communication with the HiveMQ broker.
*/
void MqttTask(void* pvParameters)
{
  static WiFiClient wifiClient;
  static PubSubClient mqttClient(wifiClient);
  static char dataToPublish[120];
  
  char prevSubTopic[SIZE_TOPIC] = {0};
  const char *mqttBroker = "broker.hivemq.com";
  const uint16_t mqttPort = 1883;  
  sensor_t sensorData = {};
  
  while(1)
  {
    if(WiFi.status() == WL_CONNECTED)
    {       
      if(!mqttClient.connected())
      { 
        preferences.getBytes("5",prevSubTopic,SIZE_TOPIC);
        mqttClient.setServer(mqttBroker,mqttPort);
        while(!mqttClient.connected())
        {
          String clientID = String(WiFi.macAddress());
          if(mqttClient.connect(clientID.c_str()))
          {
            Serial.println("Connected to HiveMQ broker");
          }
        } 
      }
      else
      {
        //Receive 'units consumed' by users from Utility task
        if(xQueueReceive(queue.utilToMqtt,&sensorData,0) == pdPASS)
        {
          //Converting the user units (volumes) from mL to L
          sensorData.volume1 /= 1000.0;
          sensorData.volume2 /= 1000.0;
          sensorData.volume3 /= 1000.0;
          Serial.println("Util-MQTT RX PASS\n");

          char volume1Buff[11] = {0};
          char volume2Buff[11] = {0};
          char volume3Buff[11] = {0};

          FloatToString(sensorData.volume1,volume1Buff,100);
          FloatToString(sensorData.volume2,volume2Buff,100);
          FloatToString(sensorData.volume3,volume3Buff,100);

          strcat(dataToPublish,"User1's unit: ");
          strcat(dataToPublish,volume1Buff);
          strcat(dataToPublish," L\n");
          strcat(dataToPublish,"User2's unit: ");
          strcat(dataToPublish,volume2Buff);
          strcat(dataToPublish," L\n");
          strcat(dataToPublish,"User3's unit: ");
          strcat(dataToPublish,volume3Buff);
          strcat(dataToPublish," L");
                                                     
          mqttClient.publish(prevSubTopic,dataToPublish);
          uint32_t dataLen = strlen(dataToPublish);
          memset(dataToPublish,'\0',dataLen);
        }  
      }
    }
  }
}

/**
 * @brief Handles main application logic
*/
void ApplicationTask(void* pvParameters)
{
  static SIM800L gsm(&Serial2);
  bool isWifiTaskSuspended = false;
  recharge_util_t recharge = {};
  char otp[SIZE_OTP] = {0};
  
  while(1)
  {
    //Suspend WiFi Management task if the system is already.... 
    //connected to a Wi-Fi network
    if(WiFi.status() == WL_CONNECTED && !isWifiTaskSuspended)
    {
      Serial.println("WIFI TASK: SUSPENDED");
      vTaskSuspend(wifiTaskHandle);
      isWifiTaskSuspended = true;
    }
    else if(WiFi.status() != WL_CONNECTED && isWifiTaskSuspended)
    {
      Serial.println("WIFI TASK: RESUMED");
      vTaskResume(wifiTaskHandle);
      isWifiTaskSuspended = false;
    }
    //Receive recharge details (phone number & units) from Utility task
    if(xQueueReceive(queue.utilToApp1,&recharge,0) == pdPASS)
    {
      Serial.println("Util-App recharge RX PASS\n");
    }
    //Receive OTP from Utility task
    if(xQueueReceive(queue.utilToApp2,otp,0) == pdPASS)
    {
      Serial.println("Util-App OTP RX PASS\n");
    }
    //Send SMS containing OTP and units to the user
    if(strcmp(recharge.phoneNum,"") && recharge.units > 0 && strcmp(otp,""))
    { 
      const char msg1[] = "OTP for a recharge of ";
      const char msg2[] = " units is: ";      
      char unitsStr[SIZE_REQUEST] = {0};
      IntegerToString(recharge.units,unitsStr);
      
      //Structuring the SMS to be sent to the user
      uint8_t msgSize = strlen(msg1) + strlen(unitsStr) + strlen(msg2) + SIZE_OTP;
      char message[msgSize] = {0};      
      strcat(message,msg1);
      strcat(message,unitsStr);
      strcat(message,msg2);
      strcat(message,otp);

      char ccPhoneNum[SIZE_PHONE + 3] = "+234"; //country code (+234:NG)
      strcpy(ccPhoneNum + 4,recharge.phoneNum + 1);
      gsm.SendSMS(ccPhoneNum,message);

      Serial.print("MESSAGE: ");
      Serial.println(message);
      Serial.print("CC PHONE: ");
      Serial.println(ccPhoneNum);
      
      memset(recharge.phoneNum,'\0',SIZE_PHONE);
      recharge.units = 0;
      memset(otp,'\0',SIZE_OTP);
    }
  }
}

/**
 * @brief Handles communication with the meter.
*/
void MeterTask(void* pvParameters)
{
  const uint8_t chipEn = 15;
  const uint8_t chipSel = 5; 
  const byte addr[][6] = {"00001","00002"};
  static RF24 nrf24(chipEn,chipSel);

  nrf24.begin();
  nrf24.openWritingPipe(addr[0]);
  nrf24.openReadingPipe(1,addr[1]);
  nrf24.setPALevel(RF24_PA_MAX);
  nrf24.startListening();
    
  while(1)
  {
    if(nrf24.available())
    {
      meter_util_t meterToUtil = {};
      nrf24.read(&meterToUtil,sizeof(meterToUtil));
      //Debug
      Serial.println("User volumes: ");
      Serial.println(meterToUtil.sensorData.volume1);
      Serial.println(meterToUtil.sensorData.volume2);
      Serial.println(meterToUtil.sensorData.volume3);
      Serial.print("Phone: ");
      Serial.println(meterToUtil.recharge.phoneNum);
      Serial.print("Units required: ");
      Serial.println(meterToUtil.recharge.units);
      //Send 'units consumed' by users to the MQTT task
      if(xQueueSend(queue.utilToMqtt,&meterToUtil.sensorData,0) == pdPASS)
      {
        Serial.println("Util-MQTT TX PASS\n");
      }
      if(strcmp(meterToUtil.recharge.phoneNum,"") && (meterToUtil.recharge.units > 0))
      {
        char otp[SIZE_OTP] = {0};
        RandomizeOtp(otp);
        SendOtpToMeter(nrf24,otp);
        Serial.print("OTP = ");
        Serial.println(otp);
        //Send recharge details (phone number & units) to App task
        if(xQueueSend(queue.utilToApp1,&meterToUtil.recharge,0) == pdPASS)
        {
          Serial.println("Util-App recharge TX PASS\n");
        }
        //Send OTP to App task
        if(xQueueSend(queue.utilToApp2,otp,0) == pdPASS)
        {
          Serial.println("Util-App OTP TX PASS\n");
        }
      }
    }
  }
}

/**
 * @brief Callback function that is called whenever WiFi
 * manager parameters are received
*/
void WiFiManagerCallback(void) 
{
  char prevSubTopic[SIZE_TOPIC] = {0};
  preferences.getBytes("5",prevSubTopic,SIZE_TOPIC);  
  StoreNewFlashData("5",subTopic.getValue(),prevSubTopic,SIZE_TOPIC);
}
