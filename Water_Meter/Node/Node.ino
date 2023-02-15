#include <SoftwareSerial.h>
#include "MNI.h"
#include "FlowSensor.h"

/**
 * @breif Description of the node.
 * 
 * Receives a 'request-to-send' from the Master. It sends the available units
 * or volume (in centi-litres) for the 3 users to the master.
 * Stores the readings in an SD card (periodically) to prevent loss of data if  
 * a power outage occurs.  
 * 
 * NB: When water flows through the flow sensors, they generate pulses. The sensors 
 * output roughly 486 pulses per litre. For every pulse detected, 2.1mL of water must 
 * have flown through the sensor. For ease of calculations, cL is used. 1 pulse is equivalent 
 * to 21cL.
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

int testVolume1 = 25000;

void setup() 
{
  Serial.begin(9600);
}

void loop() 
{
  if(mni.ReceivedData())
  {
    if(mni.DecodeData(MNI::RxDataId::DATA_QUERY) == MNI::QUERY)
    {
      testVolume1 = testVolume1 - 21;
      if(testVolume1 < 0)
      {
        testVolume1 = 0;
      }
      //Debug
      Serial.print("Test volume: ");
      Serial.println(testVolume1);   
      //Encode $ transmit
      mni.EncodeData(MNI::ACK,MNI::TxDataId::DATA_ACK);
      mni.EncodeData(testVolume1,MNI::TxDataId::USER1_VOLUME);
      mni.TransmitData();
    }
  } 
}
