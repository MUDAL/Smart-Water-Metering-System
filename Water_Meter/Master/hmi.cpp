#include <Arduino.h>
#include <string.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h> //Version 1.1.2
#include "keypad.h"
#include "hmi.h"

/**
 * @brief Converts a string to an integer.
 * e.g.
 * "932" to 932:
 * '9' - '0' = 9
 * '3' - '0' = 3
 * '2' - '0' = 2
 * 9 x 10^2 + 3 x 10^1 + 2 x 10^0 = 932.
*/
void StringToInteger(char* stringPtr,uint32_t* integerPtr)
{
  uint32_t integer = 0;
  uint8_t len = strlen(stringPtr);
  uint32_t j = 1;
  for(uint8_t i = 0; i < len; i++)
  {
    *integerPtr += ((stringPtr[len - i - 1] - '0') * j);
    j *= 10;
  }
}

/**
 * @brief Configures parameters (e.g. user ID, pin, phone number).
 * @param col: Column in which user input will start being displayed.  
 * @param row: Row in which user input will start being displayed.  
 * @param param: Parameter to configure via the HMI.  
 * @param counterRef: Reference to the counter for the parameter being
 * configured. The counter helps in updating 'param' with new user inputs
 * (or key presses).   
 * @param paramSize: Size of the 'param' buffer.   
 * @param isHidden: If set to false, the parameter being configured will be  
 * displayed normally on the LCD otherwise, the parameter will be hidden for  
 * security reasons.  
 * @return None
*/
void HMI::SetParam(uint8_t col,uint8_t row,
                   char* param,uint8_t& counterRef,
                   uint8_t paramSize,bool isHidden)
{
  lcdPtr->setCursor(1,row);
  lcdPtr->print('>');
  //Reset paramter and counter(reference), and refresh display 
  uint8_t hiddenCol = col;
  memset(param,'\0',paramSize);
  counterRef = 0;
  lcdPtr->setCursor(col,row);
  for(uint8_t i = col; i < 20; i++)
  {
    lcdPtr->print(' ');
  }
  
  while(1)
  {
    char key = keypadPtr->GetChar();
    switch(key)
    {
      case '\0':
        break;
      case '#':
        return;
      default:
        param[counterRef] = key;
        if(!isHidden)
        {
          lcdPtr->setCursor(col,row);
          lcdPtr->print(param); 
        }
        else
        {//to hide parameter being set (for security reasons)
          lcdPtr->setCursor(hiddenCol,row);
          lcdPtr->print('X'); 
          hiddenCol++;
          if(hiddenCol == 20)
          {//must not go beyond 20th column of the LCD.
            hiddenCol = col;
          }
        }
        counterRef++;
        counterRef %= (paramSize - 1);
        break;
    }
  }
}

void HMI::DisplayParam(uint8_t col,uint8_t row,char* param,bool isHidden)
{
  lcdPtr->setCursor(col,row);
  if(!isHidden)
  {
    lcdPtr->print(param); 
  }
  else
  {
    uint8_t len = strlen(param);
    for(uint8_t i = 0; i < len; i++)
    {
      lcdPtr->print('X');
    }
  }
}

void HMI::DisplayPageNumber(uint8_t row,uint8_t currentPage,uint8_t lastPage)
{
  lcdPtr->setCursor(0,row);
  lcdPtr->print("C:<<");
  lcdPtr->setCursor(7,row);
  lcdPtr->print(currentPage);
  lcdPtr->print('/');
  lcdPtr->print(lastPage);
  lcdPtr->setCursor(16,row);
  lcdPtr->print("D:>>");  
}

void HMI::DisplayHelpPage1(void)
{
  HMI::DisplayPageNumber(ROW1,PAGE1,PAGE4);
  lcdPtr->setCursor(0,ROW2);
  lcdPtr->print("- Log in with your");
  lcdPtr->setCursor(0,ROW3);
  lcdPtr->print("user ID and PIN. ");
  lcdPtr->setCursor(0,ROW4);
  lcdPtr->print("Check your profile.");  
}

void HMI::DisplayHelpPage2(void)
{
  HMI::DisplayPageNumber(ROW1,PAGE2,PAGE4);
  lcdPtr->setCursor(0,ROW2);
  lcdPtr->print("- Recharge request:");
  lcdPtr->setCursor(0,ROW3);
  lcdPtr->print("Enter units e.g.");  
  lcdPtr->setCursor(0,ROW4);
  lcdPtr->print("25 for 25 litres");   
}

void HMI::DisplayHelpPage3(void)
{
  HMI::DisplayPageNumber(ROW1,PAGE3,PAGE4);
  lcdPtr->setCursor(0,ROW2);
  lcdPtr->print("- After the request,");
  lcdPtr->setCursor(0,ROW3);
  lcdPtr->print("you will get an SMS.");
  lcdPtr->setCursor(0,ROW4);
  lcdPtr->print("Enter the token.");  
}

void HMI::DisplayHelpPage4(void)
{
  HMI::DisplayPageNumber(ROW1,PAGE4,PAGE4);
  lcdPtr->setCursor(0,ROW2);
  lcdPtr->print("Hit * to recharge,");
  lcdPtr->setCursor(0,ROW3);
  lcdPtr->print("save your ID, PIN,");
  lcdPtr->setCursor(0,ROW4);
  lcdPtr->print("and phone number.");  
}

void HMI::DisplayInstructions(void)
{
  //Simple FSM to change info to be displayed
  enum {DISP_ST1 = 0, DISP_ST2, DISP_ST3, DISP_ST4};
  uint8_t displayState = DISP_ST1; 
  lcdPtr->clear();
    
  while(1)
  {
    char key = keypadPtr->GetChar();
    switch(key)
    {
      case '#':
        lcdPtr->clear();
        return;
      case 'C':
        if(displayState > DISP_ST1)
        {
          lcdPtr->clear();
          displayState--;
        }
        break;
      case 'D':
        if(displayState < DISP_ST4)
        {
          lcdPtr->clear();
          displayState++;
        }
        break;
    }
    
    switch(displayState)
    {
      case DISP_ST1:
        HMI::DisplayHelpPage1();
        break;
      case DISP_ST2:
        HMI::DisplayHelpPage2();
        break;
      case DISP_ST3:
        HMI::DisplayHelpPage3();
        break;
      case DISP_ST4:
        HMI::DisplayHelpPage4();
        break;
    }
  }
}

void HMI::DisplayLoginError(void)
{
  lcdPtr->clear();
  lcdPtr->print("LOGIN ERROR!!");
  lcdPtr->setCursor(0,ROW2);
  lcdPtr->print("- TRY AGAIN OR ");
  lcdPtr->setCursor(0,ROW3);
  lcdPtr->print("- CALL THE UTILITY");  
}

void HMI::DisplayLoginSuccess(void)
{
  lcdPtr->clear();
  lcdPtr->print("LOGIN SUCCESSFUL");
  lcdPtr->setCursor(0,ROW2);
  lcdPtr->print("WELCOME USER: ");
  lcdPtr->print(userIndex + 1);   
}

void HMI::DisplaySaveSuccess(char* infoToDisplayAfterSave,
                             uint32_t displayPeriodMillis)
{
  lcdPtr->clear();
  lcdPtr->print("SAVE SUCCESS:");
  lcdPtr->setCursor(0,ROW2);
  lcdPtr->print(infoToDisplayAfterSave); 
  vTaskDelay(pdMS_TO_TICKS(displayPeriodMillis));
  lcdPtr->clear();
}

void HMI::DisplayRequestSuccess(uint32_t displayPeriodMillis)
{
  lcdPtr->clear();
  lcdPtr->print("YOUR REQUEST HAS");  
  lcdPtr->setCursor(0,ROW2);
  lcdPtr->print("BEEN SENT TO THE");
  lcdPtr->setCursor(0,ROW3);
  lcdPtr->print("UTILITY. WAIT FOR");
  lcdPtr->setCursor(0,ROW4);
  lcdPtr->print("YOUR TOKEN (SMS). ");
  vTaskDelay(pdMS_TO_TICKS(displayPeriodMillis));
  lcdPtr->clear();  
}

void HMI::PointToRow(char* heading1,char* heading2,
                     char* heading3,char* heading4,
                     uint8_t row)
{
  const uint8_t numOfRows = 4;
  char* heading[] = {heading1,heading2,heading3,heading4};
  for(uint8_t i = 0; i < numOfRows; i++)
  {
    if(row == i)
    {
      heading[i][0] = '-'; //highlight current row
    }
    lcdPtr->setCursor(0,i);
    lcdPtr->print(heading[i]);
  }    
}

void HMI::ChangeStateTo(State nextState)
{
  currentState = nextState;
  lcdPtr->clear();
}

void HMI::StateFunc_MainMenu(void)
{
  char heading1[] = "**** MAIN MENU ****";
  char heading2[] = "  LOG IN ";
  char heading3[] = "  HELP "; 
  char heading4[] = "";

  HMI::PointToRow(heading1,heading2,
                  heading3,heading4,
                  currentRow.mainMenu);
  
  char key = keypadPtr->GetChar();
  switch(key)
  {
    case 'A':
      currentRow.mainMenu = ROW2;
      break;
    case 'B':
      currentRow.mainMenu = ROW3;
      break;  
    case '#':
      switch(currentRow.mainMenu)
      {
        case ROW2:
          HMI::ChangeStateTo(ST_LOGIN_MENU);
          break;
        case ROW3:
          HMI::DisplayInstructions();
          break;
      }
      break;
  }
}

void HMI::StateFunc_LoginMenu(void)
{ 
  char heading1[] = "  USER ID:";
  char heading2[] = "  PIN:";
  char heading3[] = "  PROCEED"; 
  char heading4[] = "  MENU";  
  //LCD columns where the display of user input begins
  uint8_t idColumn = strlen(heading1);
  uint8_t pinColumn = strlen(heading2);
  
  HMI::PointToRow(heading1,heading2,
                  heading3,heading4,
                  currentRow.loginMenu);  
                  
  char key = keypadPtr->GetChar();
  switch(key)
  {
    case 'A':
      if(currentRow.loginMenu > ROW1)
      {
        currentRow.loginMenu--;
      }
      break;
    case 'B':
      if(currentRow.loginMenu < ROW4)
      {
        currentRow.loginMenu++;
      }
      break;  
    case '#':
      switch(currentRow.loginMenu)
      {
        case ROW1:
          HMI::SetParam(idColumn,ROW1,id,counter.id,SIZE_ID);
          break;
        case ROW2:
          HMI::SetParam(pinColumn,ROW2,pin,counter.pin,SIZE_PIN,true);
          break;
        case ROW3:
          userIndex = ValidateLogin(id,SIZE_ID,pin,SIZE_PIN);
          GetPhoneNum(userIndex,phoneNum,SIZE_PHONE);
          if(userIndex == USER_UNKNOWN)
          {
            HMI::DisplayLoginError();
            vTaskDelay(pdMS_TO_TICKS(3000));
            HMI::ChangeStateTo(ST_MAIN_MENU);
          }
          else
          {
            HMI::DisplayLoginSuccess();
            vTaskDelay(pdMS_TO_TICKS(3000));
            HMI::ChangeStateTo(ST_USER_MENU1);
          }
          break;
        case ROW4:
          HMI::ChangeStateTo(ST_MAIN_MENU);
          break;
      }
      break;
  }                  
}

void HMI::StateFunc_UserMenu1(void)
{
  char heading1[] = "  UNITS:";
  char heading2[] = "  REQUEST:";
  char heading3[] = "  TOKEN:"; 
  char heading4[] = "";
  uint8_t unitsColumn = strlen(heading1);
  uint8_t reqColumn = strlen(heading2);
  uint32_t request = 0;
  bool requestSent = false;
  
  HMI::PointToRow(heading1,heading2,
                  heading3,heading4,
                  currentRow.userMenu1);  
  GetUnits(userIndex,&volume); 
  lcdPtr->setCursor(unitsColumn,ROW1);
  lcdPtr->print((volume / 1000.0),2); //display units(or volume in litres) in 2dp
  lcdPtr->print("L    "); //some spaces (4) to clear possible leftovers from previous display
  HMI::DisplayPageNumber(ROW4,PAGE1,PAGE3);
                     
  char key = keypadPtr->GetChar();
  switch(key)
  {
    case 'A':
      currentRow.userMenu1 = ROW2;
      break;
    case 'B':
      currentRow.userMenu1 = ROW3;
      break;  
    case 'D':
      HMI::ChangeStateTo(ST_USER_MENU2);
      break;
    case '#':
      switch(currentRow.userMenu1)
      {
        case ROW2:
          HMI::SetParam(reqColumn,ROW2,reqBuff,counter.request,REQUEST_SIZE);
          break;
        case ROW3:
          break;
      }
      break;
    case '*':
      switch(currentRow.userMenu1)
      {
        case ROW2:
          StringToInteger(reqBuff,&request);
          requestSent = HandleRecharge(userIndex,request);
          memset(reqBuff,'\0',REQUEST_SIZE);
          if(requestSent)
          {
            HMI::DisplayRequestSuccess(3000);
          }
          break;
        case ROW3:
          break;
      }
      break;
  }                  
}

void HMI::StateFunc_UserMenu2(void)
{
  char heading1[] = "  PRESS * TO SAVE";
  char heading2[] = "  USER ID:";
  char heading3[] = "  PIN:"; 
  char heading4[] = "";
  uint8_t idColumn = strlen(heading2);
  uint8_t pinColumn = strlen(heading3);
  char infoToDisplayAfterSave[10] = {0};
  bool isIdSaved = false;
  bool isPinSaved = false;
  
  HMI::PointToRow(heading1,heading2,
                  heading3,heading4,
                  currentRow.userMenu2); 
  HMI::DisplayParam(idColumn,ROW2,id);
  HMI::DisplayParam(pinColumn,ROW3,pin,true);                  
  HMI::DisplayPageNumber(ROW4,PAGE2,PAGE3);
  
  char key = keypadPtr->GetChar();
  switch(key)
  {
    case 'A':
      currentRow.userMenu2 = ROW2;
      break;
    case 'B':
      currentRow.userMenu2 = ROW3;
      break;  
    case 'C':
      HMI::ChangeStateTo(ST_USER_MENU1);
      break;
    case 'D':
      HMI::ChangeStateTo(ST_USER_MENU3);
      break;
    case '#':
      switch(currentRow.userMenu2)
      {
        case ROW2:
          HMI::SetParam(idColumn,ROW2,id,counter.id,SIZE_ID);
          break;
        case ROW3:
          HMI::SetParam(pinColumn,ROW3,pin,counter.pin,SIZE_PIN,true);
          break;
      }
      break;
    case '*':
      isIdSaved = StoreUserParam(userIndex,ID,id,SIZE_ID);
      isPinSaved = StoreUserParam(userIndex,PIN,pin,SIZE_PIN);
      if(isIdSaved)
      {
        strcat(infoToDisplayAfterSave,"-ID");
      }
      if(isPinSaved)
      {
        strcat(infoToDisplayAfterSave,"-PIN");
      }
      if(isIdSaved || isPinSaved)
      {
        HMI::DisplaySaveSuccess(infoToDisplayAfterSave,2000); 
      }
      break;
  }   
}

void HMI::StateFunc_UserMenu3(void)
{
  char heading1[] = "  PRESS * TO SAVE";
  char heading2[] = "  PHONE:";
  char heading3[] = "  MENU"; 
  char heading4[] = "";
  uint8_t phoneColumn = strlen(heading2);
  
  HMI::PointToRow(heading1,heading2,
                  heading3,heading4,
                  currentRow.userMenu3); 
  HMI::DisplayParam(phoneColumn,ROW2,phoneNum);
  HMI::DisplayPageNumber(ROW4,PAGE3,PAGE3); 
  
  char key = keypadPtr->GetChar();
  switch(key)
  {
    case 'A':
      currentRow.userMenu3 = ROW2;
      break;
    case 'B':
      currentRow.userMenu3 = ROW3;
      break;  
    case 'C':
      HMI::ChangeStateTo(ST_USER_MENU2);
      break;
    case '#':
      switch(currentRow.userMenu3)
      {
        case ROW2:
          HMI::SetParam(phoneColumn,ROW2,phoneNum,counter.phoneNum,SIZE_PHONE);
          break;
        case ROW3:
          HMI::ChangeStateTo(ST_MAIN_MENU);
          break;
      }
      break;
    case '*':
      if(StoreUserParam(userIndex,PHONE,phoneNum,SIZE_PHONE))
      {
        HMI::DisplaySaveSuccess("-PHONE NUMBER",2000);
      }
      break;
  }   
}

HMI::HMI(LiquidCrystal_I2C* lcdPtr,Keypad* keypadPtr)
{
  //Initialize private variables
  this->lcdPtr = lcdPtr;
  this->keypadPtr = keypadPtr;
  currentState = ST_MAIN_MENU;
  currentRow.mainMenu = ROW2;
  currentRow.loginMenu = ROW1;
  currentRow.userMenu1 = ROW2;
  currentRow.userMenu2 = ROW2;
  currentRow.userMenu3 = ROW2;
  counter.id = 0;
  counter.pin = 0;
  counter.phoneNum = 0;
  counter.request = 0;
  memset(id,'\0',SIZE_ID);
  memset(pin,'\0',SIZE_PIN);
  memset(phoneNum,'\0',SIZE_PHONE);
  memset(reqBuff,'\0',REQUEST_SIZE);
  userIndex = USER_UNKNOWN; 
  volume = 0;
}

void HMI::Start(void)
{
  switch(currentState)
  {
    case ST_MAIN_MENU:
      HMI::StateFunc_MainMenu();
      break;
    case ST_LOGIN_MENU:
      HMI::StateFunc_LoginMenu();
      break;
    case ST_USER_MENU1:
      HMI::StateFunc_UserMenu1();
      break;
    case ST_USER_MENU2:
      HMI::StateFunc_UserMenu2();
      break;
    case ST_USER_MENU3:
      HMI::StateFunc_UserMenu3();
      break;            
  }
}

void HMI::RegisterCallback(UserIndex(*ValidateLogin)(char*,uint8_t,char*,uint8_t))
{
  Serial.println("Registered {ValidateLogin} callback");
  this->ValidateLogin = ValidateLogin;
}

void HMI::RegisterCallback(void(*GetPhoneNum)(UserIndex,char*,uint8_t))
{
  Serial.println("Registered {GetPhoneNum} callback");
  this->GetPhoneNum = GetPhoneNum;
}

void HMI::RegisterCallback(void(*GetUnits)(UserIndex,float*))
{
  Serial.println("Registered {GetUnits} callback");
  this->GetUnits = GetUnits;  
}

void HMI::RegisterCallback(bool(*StoreUserParam)(UserIndex,UserParam,char*,uint8_t))
{
  Serial.println("Registered {StoreUserParam} callback");
  this->StoreUserParam = StoreUserParam;   
}

void HMI::RegisterCallback(bool(*HandleRecharge)(UserIndex,uint32_t))
{
  Serial.println("Registered {HandleRecharge} callback");
  this->HandleRecharge = HandleRecharge;   
}

