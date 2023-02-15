#pragma once

class FlowSensor
{
  private:
    uint8_t pin;
    bool isPrevHigh;
    float volume;
    
  public:
    FlowSensor(uint8_t pin);
    void UpdateVolume(int volume);
    float GetVolume(void);
};
