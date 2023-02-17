# Smart-Water-Metering-System
Smart water metering system proposed by Tosin

## How it works  
The system is divided into two parts.  
1. The smart meter  
2. The utility system  

Functions of the meter:  
1. Measures and displays of volume of water used  
2. Disables the flow of water if units have been used up  
3. Enables recharging of units via requests to the utility    

Functions of the utility:  
1. Generates token required to recharge units   
2. Sends token:  
	a. via SMS to the user  
	b. wirelessly to the meter  
3. Sends current user's current units to the cloud.  

In this project, a single meter is assigned to 3 users.  
Each user has a unique ID and PIN in order to log in to the smart meter.  

## Credits  
1. Arduino Interrupts: https://dronebotworkshop.com/interrupts/   
2. NRF24L01 communication: https://www.instructables.com/Arduino-Wireless-Communication-Using-NRF24L01/   