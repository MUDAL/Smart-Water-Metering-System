#include <Arduino.h>
#include <string.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h> //Version 1.1.2
#include "keypad.h"
#include "hmi.h"

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

void HMI::DisplayInstructions(void)
{
  //Simple FSM to change info to be displayed
  const uint8_t displayState1 = 0;
  const uint8_t displayState2 = 1;
  uint8_t displayState = displayState1;
  uint32_t prevTime = millis();  
  lcdPtr->clear();
    
  while(1)
  {
    char key = keypadPtr->GetChar();
    if(key == '#')
    {
      lcdPtr->clear();
      break;
    }
    //FSM
    switch(displayState)
    {
      case displayState1:
        lcdPtr->setCursor(7,0);
        lcdPtr->print("<1/2>");
        lcdPtr->setCursor(0,1);
        lcdPtr->print("- Log in with your");
        lcdPtr->setCursor(0,2);
        lcdPtr->print("user ID and PIN. ");
        lcdPtr->setCursor(0,3);
        lcdPtr->print("Check your profile.");
        if((millis() - prevTime) >= 4000)
        {
          displayState = displayState2;
          prevTime = millis();
          lcdPtr->clear();
        }
        break;
        
      case displayState2:
        lcdPtr->setCursor(7,0);
        lcdPtr->print("<2/2>");
        /*TO-DO: Add code to explain recharging process*/
        if((millis() - prevTime) >= 4000)
        {
          lcdPtr->clear();
          return;
        }
    }
  }
}

void HMI::DisplayLoginError(void)
{
  lcdPtr->clear();
  lcdPtr->print("LOGIN ERROR!!");
  lcdPtr->setCursor(0,1);
  lcdPtr->print("- TRY AGAIN OR ");
  lcdPtr->setCursor(0,2);
  lcdPtr->print("- CALL THE UTILITY");  
}

void HMI::DisplayLoginSuccess(void)
{
  lcdPtr->clear();
  lcdPtr->print("LOGIN SUCCESSFUL");
  lcdPtr->setCursor(0,1);
  lcdPtr->print("WELCOME USER: ");
  lcdPtr->print(userIndex);   
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
          lcdPtr->setCursor(1,ROW1);
          lcdPtr->print('>');
          HMI::SetParam(idColumn,ROW1,id,counter.id,SIZE_ID);
          break;
        case ROW2:
          lcdPtr->setCursor(1,ROW2);
          lcdPtr->print('>');
          HMI::SetParam(pinColumn,ROW2,pin,counter.pin,SIZE_PIN,true);
          break;
        case ROW3:
          userIndex = ValidateLogin(id,SIZE_ID,pin,SIZE_PIN);
          GetPhoneNum(userIndex,phoneNum,SIZE_PHONE);
          if(userIndex != 0 && userIndex != 1 && userIndex != 2)
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
  char heading1[] = "* UNITS:";
  char heading2[] = "  REQUEST:";
  char heading3[] = "  TOKEN:"; 
  char heading4[] = "  C:<< D:>> 1/3";

  HMI::PointToRow(heading1,heading2,
                  heading3,heading4,
                  currentRow.userMenu1); 
  //Get volume
  GetUnits(userIndex,&volume);
  //LCD column where the display of user input begins
  uint8_t unitsColumn = strlen(heading1);
  lcdPtr->setCursor(unitsColumn,ROW1);
  lcdPtr->print((volume / 100.0),2); //display units(or volume in litres) in 2dp
  lcdPtr->print("L    "); //some spaces (4) to clear possible leftovers from previous display
                     
  char key = keypadPtr->GetChar();
  switch(key)
  {
    case 'A':
      currentRow.userMenu1 = ROW2;
      break;
    case 'B':
      currentRow.userMenu1 = ROW3;
      break;  
    case 'C':
      HMI::ChangeStateTo(ST_USER_MENU3);
      break;
    case 'D':
      HMI::ChangeStateTo(ST_USER_MENU2);
      break;
    case '#':
      switch(currentRow.userMenu1)
      {
        case ROW2:
          break;
        case ROW3:
          break;
      }
      break;
  }                  
}

void HMI::StateFunc_UserMenu2(void)
{
  char heading1[] = "* CHANGE";
  char heading2[] = "  USER ID:";
  char heading3[] = "  PIN:"; 
  char heading4[] = "  C:<< D:>> 2/3";

  HMI::PointToRow(heading1,heading2,
                  heading3,heading4,
                  currentRow.userMenu2); 
                   
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
          break;
        case ROW3:
          break;
      }
      break;
  }   
}

void HMI::StateFunc_UserMenu3(void)
{
  char heading1[] = "* CHANGE";
  char heading2[] = "  PHONE:";
  char heading3[] = "  MENU"; 
  char heading4[] = "  C:<< D:>> 3/3";

  HMI::PointToRow(heading1,heading2,
                  heading3,heading4,
                  currentRow.userMenu3); 
  //LCD column where the display of user input begins
  uint8_t phoneColumn = strlen(heading2);
  lcdPtr->setCursor(phoneColumn,ROW2);
  lcdPtr->print(phoneNum);
  
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
    case 'D':
      HMI::ChangeStateTo(ST_USER_MENU1);
      break;
    case '#':
      switch(currentRow.userMenu3)
      {
        case ROW2:
          break;
        case ROW3:
          HMI::ChangeStateTo(ST_MAIN_MENU);
          break;
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
  memset(id,'\0',SIZE_ID);
  memset(pin,'\0',SIZE_PIN);
  memset(phoneNum,'\0',SIZE_PHONE);
  userIndex = -1; //valid values are 0,1,and 2
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

void HMI::RegisterCallback(int(*ValidateLogin)(char*,uint8_t,char*,uint8_t))
{
  Serial.println("Registered {ValidateLogin} callback");
  this->ValidateLogin = ValidateLogin;
}

void HMI::RegisterCallback(void(*GetPhoneNum)(int,char*,uint8_t))
{
  Serial.println("Registered {GetPhoneNum} callback");
  this->GetPhoneNum = GetPhoneNum;
}

void HMI::RegisterCallback(void(*GetUnits)(int,int*))
{
  Serial.println("Registered {GetUnits} callback");
  this->GetUnits = GetUnits;  
}

