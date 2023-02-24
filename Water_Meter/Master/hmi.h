#pragma once   

enum UserIndex
{
  USER1 = 0,
  USER2,
  USER3,
  USER_UNKNOWN
};
enum UserParam {ID = 0, PIN, PHONE};
enum ParamSize {SIZE_ID = 11, SIZE_PIN = 11, SIZE_PHONE = 12};
enum BufferSize {SIZE_REQUEST = 11, SIZE_OTP = 11};
/*NB: The Param and Buffer sizes include NULL*/

class HMI
{
  private:
    enum State 
    {
      ST_MAIN_MENU, 
      ST_LOGIN_MENU, 
      ST_USER_MENU1,
      ST_USER_MENU2,
      ST_USER_MENU3
    };
    enum Row {ROW1, ROW2, ROW3, ROW4};
    enum Page {PAGE1 = 1, PAGE2, PAGE3, PAGE4};
    enum OtpStatus {OTP_FAIL, OTP_PASS};
    typedef struct
    {
      uint8_t mainMenu;
      uint8_t loginMenu;
      uint8_t userMenu1;
      uint8_t userMenu2;
      uint8_t userMenu3;
    }row_t; //For LCD
    typedef struct
    {
      uint8_t id;
      uint8_t pin;
      uint8_t phoneNum;  
      uint8_t request;
      uint8_t otp;
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
    char reqBuff[SIZE_REQUEST];
    char otpBuff[SIZE_OTP];
    UserIndex userIndex;
    float volume;

    //Function pointer(s) for callback(s)
    UserIndex(*ValidateLogin)(char*,uint8_t,char*,uint8_t); 
    void(*GetPhoneNum)(UserIndex,char*,uint8_t);
    void(*GetUnits)(UserIndex,float*);
    bool(*StoreUserParam)(UserIndex,UserParam,char*,uint8_t);
    bool(*HandleRecharge)(UserIndex,uint32_t);
    bool(*VerifyOtp)(UserIndex,char*);
    //Methods
    void SetParam(uint8_t col,uint8_t row,
                  char* param,uint8_t& counterRef,
                  uint8_t paramSize,bool isHidden = false);
    void ClearParamDisplay(uint8_t col,uint8_t row,uint8_t numOfSpaces);
    void DisplayParam(uint8_t col,uint8_t row,char* param,bool isHidden = false);
    void DisplayPageNumber(uint8_t row,uint8_t currentPage,uint8_t lastPage);
    void DisplayHelpPage1(void);
    void DisplayHelpPage2(void);
    void DisplayHelpPage3(void);
    void DisplayHelpPage4(void);
    void DisplayInstructions(void);
    void DisplayLoginError(void);
    void DisplayLoginSuccess(void);
    void DisplaySaveSuccess(char* infoToDisplayAfterSave,
                            uint32_t displayPeriodMillis);
    void DisplayRequestSuccess(uint32_t displayPeriodMillis);
    void DisplayOtpStatus(OtpStatus otpStatus,
                          uint32_t displayPeriodMillis);
    void PointToRow(char* heading1,char* heading2,
                    char* heading3,char* heading4,
                    uint8_t row);
    void ChangeStateTo(State nextState);
    void StateFunc_MainMenu(void);
    void StateFunc_LoginMenu(void);
    void StateFunc_UserMenu1(void);
    void StateFunc_UserMenu2(void);
    void StateFunc_UserMenu3(void);
    
  public:
    HMI(LiquidCrystal_I2C* lcdPtr,Keypad* keypadPtr);
    void Start(void);
    void RegisterCallback(UserIndex(*ValidateLogin)(char*,uint8_t,char*,uint8_t));
    void RegisterCallback(void(*GetPhoneNum)(UserIndex,char*,uint8_t));
    void RegisterCallback(void(*GetUnits)(UserIndex,float*));
    void RegisterCallback(bool(*StoreUserParam)(UserIndex,UserParam,char*,uint8_t));
    void RegisterCallback(bool(*HandleRecharge)(UserIndex,uint32_t));
    void RegisterCallback(bool(*VerifyOtp)(UserIndex,char*));
};

