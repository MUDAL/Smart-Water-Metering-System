#include <Arduino.h>
#include "FlowSensor.h"

FlowSensor::FlowSensor(uint8_t pin)
{
  this->pin = pin;
  this->isPrevHigh = false;
  this->volume = 0;
  pinMode(this->pin,INPUT);
}

void FlowSensor::UpdateVolume(uint32_t volume)
{
  this->volume += volume;
}

float FlowSensor::GetVolume(void)
{
  //Deduct 2.1mL if a pulse is detected
  if(digitalRead(pin) && !isPrevHigh)
  {
    volume = volume - 2.1;
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
