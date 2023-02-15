#pragma once

class FlowSensor
{
  private:
    uint8_t pin;
    bool isPrevHigh;
    int volume;
    
  public:
    FlowSensor(uint8_t pin);
    void UpdateVolume(int volume);
    int GetVolume(void);
};
