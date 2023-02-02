#pragma once   

class HMI
{
  private:
    //Objects
    LiquidCrystal_I2C* lcdPtr;
    Keypad* keypadPtr;
    
  public:
    HMI(LiquidCrystal_I2C* lcdPtr,Keypad* keypadPtr);
    void Start(void);
};

