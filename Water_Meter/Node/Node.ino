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
 * output roughly 486 pulses per litre. For every pulse detected, 2.1mL of water must 
 * have flown through the sensor.
*/

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
volatile float volume1;
volatile float volume2;
volatile float volume3;

/**
 * @brief Initialize hardware timer 1.
 * Enable interrupt with 1ms repetition rate
*/
void TimerInit(void)
{
  TCCR1B = (1<<WGM12)|(1<<CS11)|(1<<CS10); //CTC mode, prescalar = 64
  OCR1A = 249; //1ms repetition rate (250 cycles)
  sei();//enable global interrupt
  TIMSK1 = (1<<OCIE1A); //enable timer 1 compare match A interrupt   
}

void setup() 
{
  Serial.begin(9600);
  TimerInit();
  flowSensor1.UpdateVolume(25000);
}

void loop() 
{
  if(mni.ReceivedData())
  {
    if(mni.DecodeData(MNI::RxDataId::DATA_QUERY) == MNI::QUERY)
    {
      //Debug
      Serial.print("volume 1: ");
      Serial.println(volume1);   
      Serial.print("volume 2: ");
      Serial.println(volume2); 
      Serial.print("volume 3: ");
      Serial.println(volume3); 
      /*
       * Encode each volume data with a factor of 100 (the master should divide 
       * the encoded data by 100 in order to get the volume in mL.
      */
      mni.EncodeData(MNI::ACK,MNI::TxDataId::DATA_ACK);
      mni.EncodeData(volume1*100,MNI::TxDataId::USER1_VOLUME);
      mni.EncodeData(volume2*100,MNI::TxDataId::USER2_VOLUME);
      mni.EncodeData(volume3*100,MNI::TxDataId::USER3_VOLUME);
      mni.TransmitData();
    }
  }
}

ISR(TIMER1_COMPA_vect)
{
  volume1 = flowSensor1.GetVolume();
  volume2 = flowSensor2.GetVolume();
  volume3 = flowSensor3.GetVolume();
}
