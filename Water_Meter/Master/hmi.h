#pragma once   

class HMI
{
  private:
    //Types
    enum Size {SIZE_ID = 11, SIZE_PIN = 11, SIZE_PHONE = 12};
    enum State {ST_MAIN_MENU, ST_LOGIN_MENU};
    enum Row {ROW1, ROW2, ROW3, ROW4};
    typedef struct
    {
      uint8_t mainMenu;
      uint8_t loginMenu;
    }row_t; //For LCD
    typedef struct
    {
      uint8_t id;
      uint8_t pin;
      uint8_t phoneNum;  
    }counter_t; //Counter for setting configurable parameter
    
    //Objects and variables
    LiquidCrystal_I2C* lcdPtr;
    Keypad* keypadPtr;
    State currentState;
    row_t currentRow; 
    counter_t counter;
    char id[SIZE_ID];
    char pin[SIZE_PIN];
    char phoneNum[SIZE_PHONE];

    //Function pointer(s) for callbacks
    void(*ValidateLogin)(char*,char*); 
    
    //Methods
    void SetParam(uint8_t col,uint8_t row,
                  char* param,uint8_t& counterRef,
                  uint8_t paramSize,bool isHidden = false);
    void DisplayInstructions(void);
    void PointToRow(char* heading1,char* heading2,
                    char* heading3,char* heading4,
                    uint8_t row);
    void ChangeStateTo(State nextState);
    void StateFunc_MainMenu(void);
    void StateFunc_LoginMenu(void);
    
  public:
    HMI(LiquidCrystal_I2C* lcdPtr,Keypad* keypadPtr);
    void Start(void);
    void RegisterCallback(void(*ValidateLogin)(char*,char*));
};

