#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h> //Version 1.1.2
#include "keypad.h"
#include "hmi.h"

void HMI::DisplayInstructions(void)
{
  //Simple FSM to switch change info to be displayed
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
        break;
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
  char heading1[] = "  USER ID: ";
  char heading2[] = "  PIN: ";
  char heading3[] = "  PROCEED"; 
  char heading4[] = "  BACK";  

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
          break;
        case ROW2:
          break;
        case ROW3:
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

