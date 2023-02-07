#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h> //Version 1.1.2
#include "keypad.h"
#include "hmi.h"

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
  char heading1[] = "*****MAIN MENU*****";
  char heading2[] = "  LOG IN ";
  char heading3[] = "  HELP "; 
  char heading4[] = "";

  HMI::PointToRow(heading1,heading2,
                  heading3,heading4,
                  currentRow.mainMenu);
  
  char key = keypadPtr->GetChar();
  switch(key)
  {
    case 'C':
      if(currentRow.mainMenu > ROW2)
      {
        currentRow.mainMenu--;
      }
      break;
    case 'D':
      if(currentRow.mainMenu < ROW3)
      {
        currentRow.mainMenu++;
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
}

void HMI::Start(void)
{
  switch(currentState)
  {
    case ST_MAIN_MENU:
      HMI::StateFunc_MainMenu();
      break;
  }
}

