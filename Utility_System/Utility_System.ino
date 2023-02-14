#include <WiFi.h>
#include <Preferences.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include "sim800l.h"

//Maximum number of characters for HiveMQ topic(s)
#define SIZE_TOPIC            30
//Define textboxes for MQTT topic(s)
WiFiManagerParameter subTopic("5","HiveMQ Subscription topic","",SIZE_TOPIC);
//Object(s)
Preferences preferences; //for accessing ESP32 flash memory
//Task handle(s)
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
  static SIM800L gsm(&Serial1,9600,-1,5);
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
 * @brief Callback function that is called whenever WiFi
 * manager parameters are received
*/
void WiFiManagerCallback(void) 
{
  char prevSubTopic[SIZE_TOPIC] = {0};
  preferences.getBytes("5",prevSubTopic,SIZE_TOPIC);  
  StoreNewFlashData("5",subTopic.getValue(),prevSubTopic,SIZE_TOPIC);
}
