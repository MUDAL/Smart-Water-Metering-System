#include <SoftwareSerial.h>
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
}sensor_t;

namespace Pin
{
  const uint8_t flowSensor1 = 3;
  const uint8_t flowSensor2 = 4;
  const uint8_t flowSensor3 = 5;
  const uint8_t nodeRx = 6;
  const uint8_t nodeTx = 7;
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
 * @brief Initialize hardware timer 1.
 * Enable periodic interrupt
*/
void TimerInit(void)
{
  TCCR1B = (1<<WGM12)|(1<<CS11)|(1<<CS10); //CTC mode, prescalar = 64
  OCR1A = 149; //0.6ms repetition rate 
  sei();//enable global interrupt
  TIMSK1 = (1<<OCIE1A); //enable timer 1 compare match A interrupt   
}

void setup() 
{
  Serial.begin(9600);
  TimerInit();
}

void loop() 
{
  const uint8_t numOfUsers = 3;
  uint32_t recharge[numOfUsers] = {0};
  const uint8_t rxBufferSize = sizeof(recharge);
  
  if(mni.IsReceiverReady(rxBufferSize))
  {
    //Receive recharged units and convert from L to mL
    mni.ReceiveData(&recharge,rxBufferSize);   
    flowSensor1.UpdateVolume(recharge[USER1] * 1000);
    flowSensor2.UpdateVolume(recharge[USER2] * 1000);
    flowSensor3.UpdateVolume(recharge[USER3] * 1000);
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
