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
  QueueHandle_t utilToApp;
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

void setup() 
{
  // put your setup code here, to run once:
  setCpuFrequencyMhz(80);
  Serial.begin(115200);  
  preferences.begin("Utility",false);
  queue.utilToMqtt = xQueueCreate(1,sizeof(sensor_t));
  queue.utilToApp = xQueueCreate(1,sizeof(recharge_util_t));
  if(queue.utilToMqtt != NULL && queue.utilToApp != NULL)
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
          String dataToPublish = "USER1: " + String(sensorData.volume1,2) + " L\n" +
                                 "USER2: " + String(sensorData.volume2,2) + " L\n" +
                                 "USER3: " + String(sensorData.volume3,2) + " L";
          mqttClient.publish(prevSubTopic,dataToPublish.c_str());
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
      if(strcmp(meterToUtil.recharge.phoneNum,"") && 
        (meterToUtil.recharge.units > 0))
      {
        char otp[SIZE_OTP] = {0};
        RandomizeOtp(otp);
        SendOtpToMeter(nrf24,otp);
        Serial.print("OTP = ");
        Serial.println(otp);
        /*TO-DO: Add code to send phone number and OTP to App task via a Queue*/
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
