#include <WiFi.h>
#include <Preferences.h>
#include <WiFiManager.h> //Version 2.0.12-beta (tzapu)
#include <PubSubClient.h>
#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h> //Version 1.4.6
#include "sim800l.h"

#define SIZE_TOPIC            30 //MQTT topic: Max number of characters
#define SIZE_PHONE            12

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
  sensor_t sensorData;
}meter_util_t;

WiFiManagerParameter subTopic("5","HiveMQ Subscription topic","",SIZE_TOPIC);
Preferences preferences; //for accessing ESP32 flash memory
TaskHandle_t wifiTaskHandle;

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

void setup() 
{
  // put your setup code here, to run once:
  setCpuFrequencyMhz(80);
  Serial.begin(115200);  
  preferences.begin("Utility",false);
  xTaskCreatePinnedToCore(WiFiManagementTask,"",7000,NULL,1,&wifiTaskHandle,1);
  xTaskCreatePinnedToCore(MqttTask,"",7000,NULL,1,NULL,1);
  xTaskCreatePinnedToCore(ApplicationTask,"",20000,NULL,1,NULL,1); 
  xTaskCreatePinnedToCore(MeterTask,"",20000,NULL,1,NULL,1);  
}

void loop() 
{
  // put your main code here, to run repeatedly:

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
  uint32_t prevTime = millis();
  
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
        if((millis() - prevTime) >= 500)
        {
          String dataToPublish = "";
          mqttClient.publish(prevSubTopic,dataToPublish.c_str());
          prevTime = millis();
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
  static SIM800L gsm(&Serial2,9600);
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
      Serial.print("Struct size: ");
      Serial.println(sizeof(meterToUtil));
      Serial.println("User volumes: ");
      Serial.println(meterToUtil.sensorData.volume1);
      Serial.println(meterToUtil.sensorData.volume2);
      Serial.println(meterToUtil.sensorData.volume3);
      Serial.print("Phone: ");
      Serial.println(meterToUtil.phoneNum);
      Serial.print("Units required: ");
      Serial.println(meterToUtil.units);
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
