#pragma once

class FlowSensor
{
  private:
    uint8_t pin;
    bool isPrevHigh;
    float volume; //in mL
    
  public:
    FlowSensor(uint8_t pin);
    void UpdateVolume(uint32_t volume);
    float GetVolume(void);
};
