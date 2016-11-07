
/*
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *      
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *      
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 */

/*  * * * * * * * * * * * * * * * * * * * * * * * * * * * 
By AWI () 2016
 Class handels a simple EEProm RFID card DB

 PROJECT: MySensors / Multisensor
 PROGRAMMER: AWI
 DATE: August27, 2016/ last update: August 27, 2016
 FILE: CardDB.h
 LICENSE: Public domain

 Hardware: 
	Nano breadboard w/ NRF24l01
	and MySensors 2.0 (Development)
		
Special:
	
Summary:
	
Remarks:
	
Change log:
20160727 - Updated sketch  
*/

#if defined(ARDUINO) && ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif
#include "CardDB.h"


	// Constructor
CardDB::CardDB(){};						// 

// cardType. reads the database and returns the type of card found (none, master, )
CardDB::cardTypes_t CardDB::readCardType(uint32_t cardKey){ ;
	int i = 0 ;
	while ( i < MAXCARDS){
		if (readCardEE(i).cardID == cardKey )	
			return readCardEE(i).cardType ;
		i++ ;
	}
	return noCard ;										// default = noCard
}
	
//readCardType by index: 
CardDB::cardTypes_t CardDB::readCardTypeIdx(int cardIndex){
	return readCardEE(cardIndex).cardType ;
}

//readCardkey by index: 
uint32_t CardDB::readCardIdIdx(int cardIndex){
	return readCardEE(cardIndex).cardID ;
}

// readCard: reads the database and returns the card Index
int CardDB::readCard(uint32_t cardKey){
	int i = 0 ;
	while ( i < MAXCARDS){
		if (readCardEE(i).cardID == cardKey ){		
			return i ;
		}
		i++ ;
	}
	return maxCards ;									// if not found return maxCards 
}

// writeCard: writes the database and returns the card Index, maxCards if error
int CardDB::writeCard(uint32_t cardKey){
	int i = 0 ;
	while (i < MAXCARDS){
		if (readCardTypeIdx(i) == noCard){
			CardDB::writeCardIdx(i, cardKey);
			return i ;
		}
		i++ ;
	}
}

// writeCard by index: writes the database
int CardDB::writeCardIdx(int cardIndex, uint32_t cardKey){
	recordType_t tempRec ;								// temporary storage
	tempRec.cardID = cardKey ;					
	tempRec.cardType = idCard ;							// default == idCard				
	writeCardEE(cardIndex, tempRec) ;
	return cardIndex ;
}

// setCardTypeIdx: set the card type
bool CardDB::setCardTypeIdx(int cardIdx, cardTypes_t cardType){
	recordType_t tempRec ;								// temporary storage
	tempRec = readCardEE(cardIdx) ;
	tempRec.cardType = cardType ;					
	writeCardEE(cardIdx, tempRec) ;
	return true ;
}

	
// deleteCard: writes the database and returns the card Index, maxCards if error
int CardDB::deleteCard(uint32_t cardKey){
	int cardIdx = CardDB::readCard(cardKey) ;			// get the card index
	if (cardIdx != maxCards){							// if found set type to noCard ;
		recordType_t tempRec ;							// temporary storage
		tempRec = readCardEE(cardIdx) ;					// read record				
		tempRec.cardType = delCard ;					// set card to deleted
		writeCardEE(cardIdx, tempRec) ;
	}
	return cardIdx ;
};

// initDB: empties the whole database
int CardDB::initDB(){
	recordType_t tempRec ;							// temporary storage
	for(int i=0 ; i < MAXCARDS ; i++){
		tempRec.cardID = 0 ;					
		tempRec.cardType = noCard ;					
		writeCardEE(i, tempRec) ;
	}
};

// printDB: prints the whole database
int CardDB::printDB(){
	for (int i=0 ; i < MAXCARDS ; i++ ){
		Serial.print(i) ; Serial.print(" ") ;
		Serial.print(readCardEE(i).cardID); Serial.print(" ") ;
		Serial.println(	readCardEE(i).cardType==CardDB::noCard?"noCard":
						readCardEE(i).cardType==CardDB::idCard?"idCard":
						readCardEE(i).cardType==CardDB::delCard?"delCard":"masterCard") ;
	}
};

// readCardEE: reads the record from the EEPROM
CardDB::recordType_t CardDB::readCardEE(uint8_t index){
	convertID_t convRecord ;							// temp storage for conversion
	for (int i=0 ; i < sizeof(convRecord); i++){
		convRecord.cardb[i] = EEPROM.read(EEPROM_Start + index * sizeof(convRecord) + i);
	}
	return convRecord.cardInfo ;
};
// writeCardEE: reads the record from the EEPROM
void CardDB::writeCardEE(uint8_t index, CardDB::recordType_t writeRecord){
	convertID_t convRecord ;	// temp storage for conversion
	convRecord.cardInfo = writeRecord ;
	for (int i=0 ; i < sizeof(convRecord) ; i++){
		EEPROM.write(EEPROM_Start + index * sizeof(convRecord) + i, convRecord.cardb[i]) ;
	}
};

