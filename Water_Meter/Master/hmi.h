#pragma once   

class HMI
{
  private:
    //Types
    enum State {ST_MAIN_MENU, ST_LOGIN_MENU};
    enum Row {ROW1, ROW2, ROW3, ROW4};
    typedef struct
    {
      uint8_t mainMenu;
    }row_t; //For LCD
    
    //Objects and variables
    LiquidCrystal_I2C* lcdPtr;
    Keypad* keypadPtr;
    State currentState;
    row_t currentRow; 
    
    //Methods
    void PointToRow(char* heading1,char* heading2,
                    char* heading3,char* heading4,
                    uint8_t row);
    void ChangeStateTo(State nextState);
    void StateFunc_MainMenu(void);
    
  public:
    HMI(LiquidCrystal_I2C* lcdPtr,Keypad* keypadPtr);
    void Start(void);
};

