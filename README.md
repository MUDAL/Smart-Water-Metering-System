# Smart-Water-Metering-System
This repository contains code, schematic, images, and documentation for a smart water metering system.  

## How it works  
The system is divided into two parts.  
1. The smart meter  
2. The utility system  

Functions of the meter:  
1. Measures and displays of volume of water used  
2. Disables the flow of water if units have been used up  
3. Enables recharging of units via requests to the utility    

Functions of the utility:  
1. Generates an OTP required to recharge units   
2. Sends the OTP:  
	a. via SMS to the user's phone   
	b. wirelessly to the meter  
3. Sends current user's current units to the cloud.  

In this project, a single meter is assigned to 3 users.  
Each user has a unique ID and PIN in order to log in to the smart meter.  

## Software architecture  
![ss_sl drawio](https://user-images.githubusercontent.com/46250887/224769776-b892ccee-2bc6-4cb5-8b78-2ba92ab0d9e6.png)  

## Images of prototype  
![20230502_193608](https://user-images.githubusercontent.com/46250887/235760690-03b2623b-02af-4235-b862-dd89e8dd6ed7.jpg)  
![Screenshot_20230516-172842_Gallery](https://github.com/MUDAL/Smart-Water-Metering-System/assets/46250887/3339a2f5-efa4-4792-8685-fb433ba3cda9)  

## Credits  
1. Atmega328 Interrupts: https://dronebotworkshop.com/interrupts/   
2. NRF24L01 communication: https://www.instructables.com/Arduino-Wireless-Communication-Using-NRF24L01/   

## Recommendations  
1. Inclusion of a proper payment system  
2. Replacement of the NRF24L01 with a wireless system that can communicate over long distances e.g. LoRaWAN and GSM.   

