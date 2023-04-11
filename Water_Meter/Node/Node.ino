#include <SoftwareSerial.h>
#include <SPI.h>
#include <SD.h>
#include "MNI.h"
#include "FlowSensor.h"

/**
 * @brief Description of the node.
 * 
 * Receives a 'request-to-send' from the Master. It sends the available units
 * or volume for the 3 users to the master.
 * Stores the readings in an SD card (periodically) to prevent loss of data if  
 * a power outage occurs.  
 * 
 * NB: When water flows through the flow sensors, they generate pulses. The sensors 
 * output roughly 482 pulses per litre. For every pulse detected, 2.1mL of water must 
 * have flown through the sensor.
 * 
 * Type of solenoid valve used: Normally Open (NO)
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

const uint8_t numOfUsers = 3;
static bool hasVolumeChanged[numOfUsers];
static bool noFlow[numOfUsers];

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
  *integerPtr = 0;
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
  File file = SD.open(path,FILE_WRITE | O_TRUNC);
  if(file)
  {
    file.print(message); 
  }
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
  if(file)
  {
    while(file.available())
    {
      if(i < buffLen)
      {
        buff[i] = file.read();
        i++;
      }
    } 
  }
  buff[i] = '\0';
  file.close();
}

/**
 * @brief Get user's units (or volume) from the SD card.
*/
static uint32_t GetUnitsFromSD(User user)
{
  uint32_t approxVolume = 0;
  const uint8_t buffLen = 10;
  char fileBuff[buffLen + 1] = {0};
  
  switch(user)
  {
    case USER1:
      SD_ReadFile("user1.txt",fileBuff,buffLen);
      break;
    case USER2:
      SD_ReadFile("user2.txt",fileBuff,buffLen);
      break;
    case USER3:
      SD_ReadFile("user3.txt",fileBuff,buffLen);
      break;
  }
  Serial.print("data read from SD = ");
  Serial.println(fileBuff);
  StringToInteger(fileBuff,&approxVolume);
  return approxVolume;
}

/**
 * @brief Store user's units (or volume) on the SD card
*/
static void PutUnitsIntoSD(User user,uint32_t* approxVolumePtr)
{
  const uint8_t buffLen = 10;
  char fileBuff[buffLen + 1] = {0};
  
  IntegerToString(approxVolumePtr[user],fileBuff);
  Serial.print("data written to SD = ");
  Serial.println(fileBuff); 
  switch(user)
  {
    case USER1:
      SD_WriteFile("user1.txt",fileBuff);
      break;
    case USER2:
      SD_WriteFile("user2.txt",fileBuff);
      break;
    case USER3: 
      SD_WriteFile("user3.txt",fileBuff);
      break;
  }
}

/**
 * @brief Drive the solenoid valve
*/
static void DriveSolenoidValve(User user,uint32_t* oldApproxVolumePtr)
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
  if(newApproxVolume != oldApproxVolumePtr[user])
  {
    oldApproxVolumePtr[user] = newApproxVolume;
    hasVolumeChanged[user] = true;
    noFlow[user] = false;
  }
  else
  {
    noFlow[user] = true;
  }
  if(newApproxVolume > 0)
  {
    digitalWrite(valvePin,LOW); //turn valve on
  }
  else
  {
    digitalWrite(valvePin,HIGH); //turn valve off
  }
}

void setup() 
{
  Serial.begin(9600);
  if(SD.begin(Pin::chipSelect))
  { 
    Serial.println("SD INIT: SUCCESS");
    flowSensor1.UpdateVolume(GetUnitsFromSD(USER1));
    flowSensor2.UpdateVolume(GetUnitsFromSD(USER2));
    flowSensor3.UpdateVolume(GetUnitsFromSD(USER3));
  }  
  pinMode(Pin::solenoidValve1,OUTPUT);
  pinMode(Pin::solenoidValve2,OUTPUT);
  pinMode(Pin::solenoidValve3,OUTPUT);  
  TimerInit();  
}

void loop() 
{
  const User user[numOfUsers] = {USER1,USER2,USER3};
  static uint32_t oldApproxVolume[numOfUsers]; //Array of previous value of units (or volumes) consumed
  uint32_t rechargedUnits[numOfUsers] = {0}; //Array of recharged units
  const uint8_t rxBufferSize = sizeof(rechargedUnits);
  
  if(mni.IsReceiverReady(rxBufferSize))
  {
    //Receive recharged units and convert from L to mL
    mni.ReceiveData(rechargedUnits,rxBufferSize);   
    flowSensor1.UpdateVolume(rechargedUnits[USER1] * 1000);
    flowSensor2.UpdateVolume(rechargedUnits[USER2] * 1000);
    flowSensor3.UpdateVolume(rechargedUnits[USER3] * 1000);
    //Debug
    Serial.print("volume 1: ");
    Serial.println(sensorData.volume1);   
    Serial.print("volume 2: ");
    Serial.println(sensorData.volume2); 
    Serial.print("volume 3: ");
    Serial.println(sensorData.volume3);     
    mni.TransmitData(&sensorData,sizeof(sensorData));
  }

  for(uint8_t i = 0; i < numOfUsers; i++)
  {
    DriveSolenoidValve(user[i],oldApproxVolume);
    if(hasVolumeChanged[i] && noFlow[i])
    {
      PutUnitsIntoSD(user[i],oldApproxVolume);
      hasVolumeChanged[i] = false; 
    }     
  }
}

ISR(TIMER1_COMPA_vect)
{
  sensorData.volume1 = flowSensor1.GetVolume();
  sensorData.volume2 = flowSensor2.GetVolume();
  sensorData.volume3 = flowSensor3.GetVolume();
}
