/*
 PROJECT: MySensors / Wiegand card reader with display
 PROGRAMMER: AWI
 DATE: 20160920 last update: 20161020
 FILE: AWI_Cardreader.ino
 LICENSE: Public domain

 Hardware: ATMega328p board w/ RS485
	and MySensors 2.0 
		
Special:
	
Summary:
	Wiegand reader - universal card reader
	4 7-segment display
	RS485 interface with MySensors (wired for security, cardID's are sent to controller)
	
	1. A local database with Card id's is kept in the node. Card id 0 is the "master master" card. With the master card you can:
	- Open the door lock (just present it)
	- Include other cards (present it twice and present the new card)
	- Delete cards (present it three times, present the card to be deleted and confirm with master card)
	- Browse the cards (present it four times), the display shows the card indexes with their status.

	2. Any registered card opens the lock for a short time. A non registered card shows "Err"
	
	3. A new (included) card is presented to the controller as a switch with "On" status and a node-id which is the same as the card index. The CardID is added as text
	
	4. A card deletion will set the corresponding switch to off (to be implemented = switching a card to off in the controller will delete the card)
	
	5. The card "browse" function will (re)"present" all the activated cards (again) to the controller
	
	
Remarks:
	Fixed node-id
	State machine based on FiniteStateMachine library
	
Change log:
20160920 - created
20161020 - updated to include MySensors V_TEXT status log. Log should be kept by controller
20161023 - clean & comment code
*/
#define MY_NODE_ID 10
#define NODE_TXT "Cardreader 10"					// Text to add to sensor name
//#define MY_PARENT_NODE_ID 3						// fixed parent to controller when 0 (else comment out = AUTO)
#define MY_DEBUG 									// Enable MySensors debug to serial
#define MY_PARENT_NODE_ID 0							// define if fixed parent
#define MY_PARENT_NODE_IS_STATIC
#undef MY_REGISTRATION_FEATURE						// sketch moves on if no registration
#define MY_TRANSPORT_DONT_CARE_MODE					// transport connection to Gateway not essential/ needs to work without

// Enable RS485 transport layer
#define MY_RS485
#define MY_RS485_DE_PIN 4							// Enables DE-pin management 
#define MY_RS485_BAUD_RATE 19200					// Set RS485 baud rate (max determined by AltsoftSerial)
// or use radio:
//#define MY_RADIO_NRF24							// Enable and select radio type attached
//#define MY_BAUD_RATE 9600

#include <SPI.h>
#include <MySensors.h>

#include "CardDB.h"									// AWI: local lib to store cards
#include "Wiegand.h"								// Wiegand protocol lib https://github.com/monkeyboard/Wiegand-Protocol-Library-for-Arduino
#include "FiniteStateMachine.h"						// FiniteStateMachine https://github.com/gusgonnet/particle-fsm/tree/master/firmware
#include "LedFlash.h"								// AWI: non blocking class for flexible LED/ buzzer 
#include "SevenSegmentTM1637.h"						// 4 digit 7 segment display https://github.com/bremme/arduino-tm1637

// helpers
#define LOCAL_DEBUG
#ifdef LOCAL_DEBUG
	#define Sprint(...) (Serial.print( __VA_ARGS__))				// macro's as substitute for print,println 
	#define Sprintln(...) (Serial.println( __VA_ARGS__))
#else
	#define Sprint(...)
	#define Sprintln(...)
#endif
//** door lock 
const byte DOORLOCK = 5 ;
//** LedFlash lib used for the Buzzer and Led on the cardreader */
const byte LED_PIN = 6;												// status led
const byte BEEP_PIN = 7 ; 											// beep
//** LED Display connections (library serial protocol)
const byte PIN_CLK = A4;   											// define CLK pin (any digital pin)
const byte PIN_DIO = A5;   											// define DIO pin (any digital pin)
//** MySensors children
const byte CARD_CHILD = 0 ; 										// MySensors master card child (rest of cards are dynamic)
const byte CARD_ID_CHILD = 1 ; 										// MySensors card id/ log sensor 

const unsigned long MASTERCARD = xxxxxxx ;							// Hardcoded MASTERCARD, insert you master Rfid code here

// Instantiate library objects
SevenSegmentTM1637    display(PIN_CLK, PIN_DIO);					// LED display
LedFlash statusLed(LED_PIN,true, 50, 400);							// status led (active on, flash on 50ms/ period 400ms )
LedFlash statusBeep(BEEP_PIN,true, 2, 400);							// buzzer (active on, flash on 2ms/ period 400ms )
CardDB cardDB ; 													// EEPROM database routines
WIEGAND wg;															// instantiate Wiegand

// state machine definitions (&routines need to be defined)
FState idleState( &idleEnter, &idleUpdate, NULL );  				// Idle state (doe not need exit routine)
FState delayState( &delayEnter, &delayUpdate, &delayExit);	  		// delay after invalid card
FState unlockState( &unlockEnter, &unlockUpdate, &unlockExit);  	// Unlocks after valid card
FState includeState( &includeEnter, &includeUpdate, &includeExit);  // waiting for inclusion of new card
FState deleteState( &deleteEnter, &deleteUpdate, &deleteExit);  	// deletion of card
FState confirmState( &confirmEnter, &confirmUpdate, &confirmExit);  // confirmation of deletion
FState browseState( &browseEnter, &browseUpdate, &browseExit);  	// browse cards
FiniteStateMachine stateMachine(idleState) ; 						//initialize state machine, start in state: noop

const unsigned long idleTime = 2000UL ;								// delay to return to idle
unsigned long browseTimer = millis() ;								// timer for browsing
const unsigned long browseTime = 800UL ;							// detay for browsing

unsigned long heartbeat = 60000UL ;									// heartbeat every hour
unsigned long lastHeartbeat = millis() ; 

unsigned long lastUpdate = millis(); 								// timer value


unsigned long lastCardID = 0 ;										// holds last card value for inclusion / deletion
int curCard = 0 ;													// Used as a browse pointer and temp store for deletion/ inclusion
bool newCard = false ;												// global to indicate new card is available

// MySensor messages
MyMessage cardStatusMsg(0,V_STATUS);								// Each card id has its own "Switch", which is presented at inclusion
MyMessage cardIdMsg(0,V_TEXT);										// Each card id has its own identifier, sent a text to controller 


void setup() {
	pinMode(DOORLOCK, OUTPUT) ;										// doorlock connection
	display.begin();												// initializes the display
	display.setBacklight(10);										// set the brightness to x %
	display.print("INIT");											// display INIT on the display
	wait(1000) ;
	wg.begin();														// activate wiegand
	lastUpdate = millis() ;
	cardDB.initDB();												// ONLY in the first run to clear the EEPROM store (comment later)
	cardDB.writeCardIdx(0, MASTERCARD);								// MASTER card (HARD CODED)
	cardDB.setCardTypeIdx(0, CardDB::masterCard) ;					// write to 0 index in database
	Sprint("EEPROM: ");
	Sprintln(EEPROM_LOCAL_CONFIG_ADDRESS, HEX) ;
}

void presentation(){
	sendSketchInfo("AWI " NODE_TXT, "1.2");							// Sketch version to gateway and Controller
	presentCard(CARD_CHILD) ;										// present the master card (index == 0)
	present(CARD_ID_CHILD, S_INFO, "SwitchID " NODE_TXT);			// present the log child
}

void loop() {
    unsigned long now = millis();									// timer value for "once in a while" events
    if (now-lastUpdate > 5000UL) {									// change after 5 seconds
		//cardDB.printDB() ;										// only for debug
		lastUpdate = now;
	}
	newCard = wg.available() ;
	if(newCard){
		Sprint("Wiegand HEX = ");
		Sprint(wg.getCode(),HEX);
		Sprint(", DECIMAL = ");
		Sprint(wg.getCode());
		Sprint(", Type W ");
		Sprintln(wg.getWiegandType());
		//sendLog(wg.getCode(), "presented");
	}
	stateMachine.update();											// check and update non blocking
	statusLed.update() ;
	statusBeep.update() ;
	}

	
/* 	State machine
**	Each state can have functions for enter/ update / exit (defined earlier)
*/

//** ILDE tate **//
void idleEnter() {Sprintln(" idle enter") ;
	statusLed.off() ;
	statusBeep.off() ;
	display.clear();													// emptdisplay
}
void idleUpdate(){
	if (newCard){
		curCard = cardDB.readCard(wg.getCode()) ;						
		if(curCard != cardDB.maxCards){									// card found
			if(cardDB.readCardTypeIdx(curCard) == CardDB::masterCard || cardDB.readCardTypeIdx(curCard) == CardDB::idCard) { // only open if id or master card
				stateMachine.transitionTo(unlockState);
			} else if (cardDB.readCardTypeIdx(curCard) == CardDB::delCard){// card found but deleted
				display.print("Errd");
				sendLog(wg.getCode(), "Deleted Card");
				stateMachine.transitionTo(delayState);
			}
 		} else {														// card not found
			display.print("Err");
			sendLog(wg.getCode(), "Unknown Card");
			stateMachine.transitionTo(delayState);
		}
	}
}

//** DELAY state **//
void delayEnter() {	Sprintln(" delay enter") ;
}
void delayUpdate() {
	if (stateMachine.timeInCurrentState() > idleTime){
		Sprintln(" to idle") ; stateMachine.transitionTo(idleState);
	}
}
void delayExit(){Sprintln(" delay exit") ;}

//** UNLOCK state **//
void unlockEnter() {Sprintln(" unlock enter") ;
	display.print(curCard);												// display Card index (curCard) on the display
	Sprintln("Door unlocked");
	sendLog(wg.getCode(), "Unlocked");
	lockDoor(false) ; 													// Unlock the door
	send(cardStatusMsg.setSensor(curCard).set(1));						// send update for sensor (card) to show its usage.
}
void unlockUpdate() {
	if (stateMachine.timeInCurrentState() > idleTime){
		Sprintln(" to idle") ;
		stateMachine.transitionTo(idleState);
		}
	if (newCard){														// new tag
		if(cardDB.readCardType(wg.getCode()) == CardDB::masterCard){	// master card, prepare for inclusion
			Sprintln(" to include") ;
			stateMachine.transitionTo(includeState);
		}
	}
}
void unlockExit(){Sprintln(" unlock exit") ;
	lockDoor(true) ; 													// Lock the door
	Sprintln("Door locked");
}

//** INCLUDE state
void includeEnter() {
	Sprintln(" include enter") ;
	display.print("Incl");
	statusLed.flash() ;
	statusBeep.flash() ;
};
void includeUpdate(){
	if (stateMachine.timeInCurrentState() > idleTime){
		Sprintln(" to idle") ;
		stateMachine.transitionTo(idleState);
	}
	if (newCard){														// new tag
		curCard = cardDB.readCard(wg.getCode()) ;
		if (curCard != cardDB.maxCards){								// known card
			if(cardDB.readCardTypeIdx(curCard) == CardDB::masterCard){	//  master card, goto delete state
				Sprintln(" to delete") ;
				stateMachine.transitionTo(deleteState);
			} else {													// known other card, so only change card type
				cardDB.setCardTypeIdx(curCard, CardDB::idCard) ;					
				display.clear(); display.print(curCard);
				sendLog(wg.getCode(), "re-included");
				presentCard(curCard); 									// present the new card as a switch and switch on
				Sprintln(" to delay") ; stateMachine.transitionTo(delayState); // delay only to extend display time
			}
		} else {														// unknown card, so add in empty spot
			curCard = cardDB.writeCard(wg.getCode()) ;
			if(curCard == cardDB.maxCards){								//  if maxCards, database full
				display.clear(); display.print("Full") ;
				sendLog(wg.getCode(), "DB full");
				Sprintln(" to delay") ; stateMachine.transitionTo(delayState); // delay only to extend display time
			} else {													// include 
				display.clear(); display.print(curCard);
				sendLog(wg.getCode(), "included");
				presentCard(curCard); 									// present the new card as a switch and switch on
				Sprintln(" to delay") ; stateMachine.transitionTo(delayState); // delay only to extend display time
			}
		}
	}
}
void includeExit(){
	Sprintln(" include exit") ;
	statusLed.off() ;
	statusBeep.off() ;
	}

//** DELETE state
void deleteEnter() {
	Sprintln(" delete enter") ;
	display.print("Del");
	statusLed.flash() ;
	statusBeep.flash() ;
};
void deleteUpdate(){
	if (stateMachine.timeInCurrentState() > idleTime){
		Sprintln(" to idle") ; stateMachine.transitionTo(idleState);
	}
	if (newCard) {
		if(cardDB.readCardType(wg.getCode()) == CardDB::masterCard){	//  master card, goto Browse
			Sprintln(" to browse") ;
			stateMachine.transitionTo(browseState);
		}else {	
			lastCardID = wg.getCode() ;									// store code for deletion
			Sprintln(" to Confirm") ; stateMachine.transitionTo(confirmState);
		}
	}
}
void deleteExit() {	Sprintln(" delete exit") ;}

//** CONFIRM state
void confirmEnter() { Sprintln(" confirm enter") ;
	display.print("Conf");
	statusLed.flash() ;
	statusBeep.flash() ;
}
void confirmUpdate(){
	if (stateMachine.timeInCurrentState() > idleTime){
		Sprintln(" to idle") ;
		stateMachine.transitionTo(idleState);
	}
	if (newCard) {
		if(cardDB.readCardType(wg.getCode()) == CardDB::masterCard){	//  master card means confirmed
			Sprint("Delete Card: "); Sprintln(lastCardID) ;
			cardDB.deleteCard(lastCardID);								// delete card (lib takes care of presence)
			sendLog(wg.getCode(), "deleted");
			send(cardStatusMsg.setSensor(lastCardID).set(0));			// switch controller status to "off"
			Sprintln(" to idle") ; stateMachine.transitionTo(idleState);
		}
	}
}
void confirmExit() {Sprintln(" confirm exit") ;}

//** BROWSE state
void browseEnter() {Sprintln(" browse enter") ;
	display.print("Brws");
	curCard = 0 ;
	browseTimer = millis() ;
	statusLed.off() ;
	statusBeep.off() ;

}
void browseUpdate(){
	unsigned long now = millis() ;
	if (newCard) {
		if(cardDB.readCardType(wg.getCode()) == CardDB::masterCard){	//  master card, return to IDLE
			Sprintln(" to idle") ; stateMachine.transitionTo(idleState);
		}
	}
	if (now >= browseTimer + browseTime){
		Sprint(" browse id: ") ; Sprintln(curCard);
		browseTimer = now ;
		display.clear();
		display.print(curCard);
		display.setCursor(0,2) ;
		if(cardDB.readCardTypeIdx(curCard)== CardDB::masterCard){ 
			display.print("ma");
			presentCard(curCard); 									// present the cards again when browsing (to sync controller)
		} else if(cardDB.readCardTypeIdx(curCard)== CardDB::idCard){ 
			display.print("id");
			presentCard(curCard);
		} else if(cardDB.readCardTypeIdx(curCard)== CardDB::delCard){ 
			display.print("dl");
			presentCard(curCard);
		} else if(cardDB.readCardTypeIdx(curCard)== CardDB::noCard){ display.print("no");}
		if (++curCard > cardDB.maxCards){								// idle if end of database
			Sprintln(" to idle") ; stateMachine.transitionTo(idleState);
		}
	}
}
void browseExit(){Sprintln(" browse exit") ;}
//** end of stae machine **//

// lock / unlock the door
void lockDoor(bool doorState){
	digitalWrite(DOORLOCK, doorState?HIGH:LOW) ;						// Adapt for active low of any other door unlock
}

// sends a log message to the controller containing a message and cardID
void sendLog(unsigned long cardID, const char logMessage[]){
	char tmpBuf[26] ;													// temporary store for message
	sprintf(tmpBuf, "Card %8lu %-10s", cardID, logMessage);				// sends unsigned long and message (be aware of message length)
	Sprintln(tmpBuf) ;
	send(cardIdMsg.setSensor(CARD_ID_CHILD).set(tmpBuf));
}

// present a (new) card to the controller by presenting it and switch it to state (master, id = On, deleted = Off)
void presentCard(byte cardIdx){
	char tmpBuf[26] ;													// temporary store for message
	sprintf(tmpBuf, "CardId %8lu", cardDB.readCardIdIdx(cardIdx));		// convert the cardID to text
	Sprintln(tmpBuf) ;
	present(cardIdx, S_BINARY, tmpBuf) ;								// present the (new) card (idx == child) to controller
	wait(50) ;															// give it some time to settle
	send(cardStatusMsg.setSensor(cardIdx).set(cardDB.readCardTypeIdx(cardIdx)==CardDB::delCard?0:1)); // switch according to type
}

// Handle incoming messages, remote card i.e. disable/ enable
void receive(const MyMessage &message) {  								// Expect few types of messages from controller
	if (message.type == V_STATUS){										// Switch "off" messages are handled as deletions
		if (message.sensor < cardDB.maxCards && message.sensor > 0){	// take care of non existing sensors and master
			cardDB.setCardTypeIdx( message.sensor, message.getBool()?CardDB::idCard:CardDB::delCard) ;	// set type according to payload
		}
	}
}
