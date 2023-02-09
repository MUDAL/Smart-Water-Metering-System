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
      if(currentRow.mainMenu > ROW2)
      {
        currentRow.mainMenu--;
      }
      break;
    case 'B':
      if(currentRow.mainMenu < ROW3)
      {
        currentRow.mainMenu++;
      }
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
  char heading4[] = "  BACK";  
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
          ValidateLogin(id,pin);
          break;
        case ROW4:
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
  counter.id = 0;
  counter.pin = 0;
  counter.phoneNum = 0;
  memset(id,'\0',SIZE_ID);
  memset(pin,'\0',SIZE_PIN);
  memset(phoneNum,'\0',SIZE_PHONE);
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
  }
}

void HMI::RegisterCallback(void(*ValidateLogin)(char*,char*))
{
  this->ValidateLogin = ValidateLogin;
}

