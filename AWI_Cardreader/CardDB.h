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
20160920 - Adapted it to use the internal EEPROM
*/

#ifndef CardDB_h
#define CardDB_h

#include <inttypes.h>

#define MY_CORE_ONLY

#include <EEPROM.h>
// #include <MySensors.h>  

#define MAXCARDS 10			// maximaum number of cards in DB (limited by EEPROM size)
#define EEPROM_Start 0x1A0	// >= MySensors eeprom EEPROM_LOCAL_CONFIG_ADDRESS


class CardDB
{


public:

	enum cardTypes_t: byte
	{
	   noCard, masterCard, idCard, delCard			// noCard = never used ; delCard = used and deleted
	};

	typedef struct {
		uint32_t cardID ;							// stores the card_id
		cardTypes_t cardType ;						// holds the card RFID
		} recordType_t ;
	
	typedef union {
			recordType_t cardInfo;					// hold the cardID & type to convert to bytes
			uint8_t cardb[5];
		} convertID_t;


	// Constructor
	CardDB() ;						// attach pin & set state

	// cardType. reads the database and returns the type of card found (noCard, masterCard, idCard, delCard)
	cardTypes_t readCardType(uint32_t cardKey) ;

	//readCardType by index: 
	cardTypes_t readCardTypeIdx(int cardIndex);

	//readCardID by index: 
	uint32_t readCardIdIdx(int cardIndex);

	// readCard: reads the database and return the card Index
	int readCard(uint32_t cardKey);

	// writeCard: writes the database and returns the card Index, maxCards if error
	int writeCard(uint32_t cardKey);

	// writeCard by index: writes the database
	int writeCardIdx(int cardIndex, uint32_t cardKey);

	// setCardType by index: writes the database 
	bool setCardTypeIdx(int cardIdx, cardTypes_t cardType);
	
	// deleteCard: writes the database and returns the card Index, NULL if error
	int deleteCard(uint32_t cardKey);

	// clearDB: empties the whole database
	int initDB();
	
	// printDB: empties the whole database
	int printDB();

	const int maxCards = MAXCARDS ;					// error value
	
private:
	recordType_t cardDatabase[MAXCARDS] ;
	unsigned long  _flash_freq, _flashPulse, _flashPause ; // frequency (=period), pulse and pause width in ms
	unsigned long  _lastUpdate, _flashTime, _count, _flashCount ;  // period, counters ()
	uint8_t _curState, _flash_stat ; 
	bool _flashTimer, _flashCounter, _activeHigh ; // status flags
	uint8_t _pin;

	// readCardEE: reads the record from the EEPROM
	recordType_t readCardEE(uint8_t index);
	// writeCardEE: reads the record from the EEPROM
	void writeCardEE(uint8_t index, recordType_t convRecord);	
};
#endif