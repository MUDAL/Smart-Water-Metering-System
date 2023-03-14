#include <SoftwareSerial.h>
#include <SPI.h>
#include <SD.h>
#include "MNI.h"
#include "FlowSensor.h"

/**
 * @breif Description of the node.
 * 
 * Receives a 'request-to-send' from the Master. It sends the available units
 * or volume for the 3 users to the master.
 * Stores the readings in an SD card (periodically) to prevent loss of data if  
 * a power outage occurs.  
 * 
 * NB: When water flows through the flow sensors, they generate pulses. The sensors 
 * output roughly 482 pulses per litre. For every pulse detected, 2.1mL of water must 
 * have flown through the sensor.
*/

enum User
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
}sensor_t; //mL

namespace Pin
{
  const uint8_t flowSensor1 = 3;
  const uint8_t flowSensor2 = 4;
  const uint8_t flowSensor3 = 5;
  const uint8_t nodeRx = 6;
  const uint8_t nodeTx = 7;
  const uint8_t solenoidValve1 = A2;
  const uint8_t solenoidValve2 = A3;
  const uint8_t solenoidValve3 = A4;
  const uint8_t chipSelect = 10;
};

//Master-Node-Interface 
SoftwareSerial nodeSerial(Pin::nodeRx,Pin::nodeTx);
MNI mni(&nodeSerial);

//Flow sensors
FlowSensor flowSensor1(Pin::flowSensor1);
FlowSensor flowSensor2(Pin::flowSensor2);
FlowSensor flowSensor3(Pin::flowSensor3);

//Current readings of the flow sensors (in mL)
static volatile sensor_t sensorData;

/**
 * @brief Converts a string to an integer.
 * e.g.
 * "932" to 932:
 * '9' - '0' = 9
 * '3' - '0' = 3
 * '2' - '0' = 2
 * 9 x 10^2 + 3 x 10^1 + 2 x 10^0 = 932.
*/
static void StringToInteger(char* stringPtr,uint32_t* integerPtr)
{
  uint32_t integer = 0;
  uint8_t len = strlen(stringPtr);
  uint32_t j = 1;
  for(uint8_t i = 0; i < len; i++)
  {
    *integerPtr += ((stringPtr[len - i - 1] - '0') * j);
    j *= 10;
  }
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
 * @brief Initialize hardware timer 1.
 * Enable periodic interrupt
*/
static void TimerInit(void)
{
  TCCR1B = (1<<WGM12)|(1<<CS11)|(1<<CS10); //CTC mode, prescalar = 64
  OCR1A = 149; //0.6ms repetition rate 
  sei();//enable global interrupt
  TIMSK1 = (1<<OCIE1A); //enable timer 1 compare match A interrupt   
}

/**
 * @brief Writes data to a file stored in an SD card
 * @param path: path to the file to be written
 * @param message: data to be written to the file
 * @return None
*/
static void SD_WriteFile(const char* path,const char* message)
{
  File file = SD.open(path,FILE_WRITE);
  file.print(message);
  file.close();
}

/*
 * @brief Reads data from a file stored in an SD card
 * @param path: path to the file to be read
 * @param buff: Buffer to store the contents of the file
 * @param buffLen: Length of buffer
 * @return None
*/
static void SD_ReadFile(const char* path,char* buff,uint8_t buffLen)
{
  uint8_t i = 0;
  File file = SD.open(path);
  while(file.available())
  {
    if(i < buffLen)
    {
      buff[i] = file.read();
      i++;
    }
  }
  buff[i] = '\0';
  file.close();
}

/**
 * @brief Drive the solenoid valve based on water flow. e.g.
 * if water isn't flowing, deactivate the solenoid valve. If  
 * water is flowing, activate the solenoid valve.
*/
static void DriveValveBasedOnFlow(User user,uint32_t& oldApproxVolumeRef)
{
  uint8_t valvePin;
  uint32_t newApproxVolume;
  
  switch(user)
  {
    case USER1:
      valvePin = Pin::solenoidValve1;
      newApproxVolume = lround(sensorData.volume1);
      break;
    case USER2:
      valvePin = Pin::solenoidValve2;
      newApproxVolume = lround(sensorData.volume2);
      break;
    case USER3:
      valvePin = Pin::solenoidValve3;
      newApproxVolume = lround(sensorData.volume3);
      break;
  }
  if(newApproxVolume != oldApproxVolumeRef)
  {
    digitalWrite(valvePin,HIGH);
    oldApproxVolumeRef = newApproxVolume;
  }
  else
  {
    digitalWrite(valvePin,LOW);
  }
}

/**
 * @brief Get user's units (or volume) from the SD card.
*/
static uint32_t GetUnitsFromSD(User user)
{
  uint32_t approxVolume;
  const uint8_t buffLen = 30;
  char fileBuff[buffLen + 1] = {0};
  
  switch(user)
  {
    case USER1:
      SD_ReadFile("/user1.txt",fileBuff,buffLen);
      break;
    case USER2:
      SD_ReadFile("/user2.txt",fileBuff,buffLen);
      break;
    case USER3:
      SD_ReadFile("/user3.txt",fileBuff,buffLen);
      break;
  }
  StringToInteger(fileBuff,&approxVolume);
  return approxVolume;
}

/**
 * @brief Store user's units (or volume) on the SD card
*/
static void PutUnitsIntoSD(User user,uint32_t approxVolume)
{
  const uint8_t buffLen = 30;
  char fileBuff[buffLen + 1] = {0};
  
  IntegerToString(approxVolume,fileBuff);
  switch(user)
  {
    case USER1:
      SD_WriteFile("/user1.txt",fileBuff);
      break;
    case USER2:
      SD_WriteFile("/user2.txt",fileBuff);
      break;
    case USER3: 
      SD_WriteFile("/user3.txt",fileBuff);
      break;
  }
}

void setup() 
{
  Serial.begin(9600);
  TimerInit();
  
  pinMode(Pin::solenoidValve1,OUTPUT);
  pinMode(Pin::solenoidValve2,OUTPUT);
  pinMode(Pin::solenoidValve3,OUTPUT);
  digitalWrite(Pin::solenoidValve1,LOW);
  digitalWrite(Pin::solenoidValve2,LOW);
  digitalWrite(Pin::solenoidValve3,LOW);  
  
  if(SD.begin(Pin::chipSelect))
  {
    Serial.println("SD INIT: SUCCESS");
    flowSensor1.UpdateVolume(GetUnitsFromSD(USER1));
    flowSensor2.UpdateVolume(GetUnitsFromSD(USER2));
    flowSensor3.UpdateVolume(GetUnitsFromSD(USER3));
  }
}

void loop() 
{
  const uint8_t numOfUsers = 3;
  uint32_t units[numOfUsers] = {0}; //Array of recharged units
  const uint8_t rxBufferSize = sizeof(units);
  
  if(mni.IsReceiverReady(rxBufferSize))
  {
    //Receive recharged units and convert from L to mL
    mni.ReceiveData(units,rxBufferSize);   
    flowSensor1.UpdateVolume(units[USER1] * 1000);
    flowSensor2.UpdateVolume(units[USER2] * 1000);
    flowSensor3.UpdateVolume(units[USER3] * 1000);
    //Debug
    Serial.print("volume 1: ");
    Serial.println(sensorData.volume1);   
    Serial.print("volume 2: ");
    Serial.println(sensorData.volume2); 
    Serial.print("volume 3: ");
    Serial.println(sensorData.volume3);     
    mni.TransmitData(&sensorData,sizeof(sensorData));
  }
}

ISR(TIMER1_COMPA_vect)
{
  sensorData.volume1 = flowSensor1.GetVolume();
  sensorData.volume2 = flowSensor2.GetVolume();
  sensorData.volume3 = flowSensor3.GetVolume();
}
