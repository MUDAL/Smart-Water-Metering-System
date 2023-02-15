#include <Arduino.h>
#include "FlowSensor.h"

FlowSensor::FlowSensor(uint8_t pin)
{
  this->pin = pin;
  this->isPrevHigh = false;
  this->volume = 0;
  pinMode(this->pin,INPUT);
}

void FlowSensor::UpdateVolume(int volume)
{
  this->volume += volume;
}

int FlowSensor::GetVolume(void)
{
  //Deduct 21cL if a pulse is detected
  if(digitalRead(pin) && !isPrevHigh)
  {
    volume = volume - 21;
    if(volume < 0)
    {
      volume = 0;
    }
    isPrevHigh = true;
  }
  else if(!digitalRead(pin) && isPrevHigh)
  {
    isPrevHigh = false;
  }
  return volume;
}
