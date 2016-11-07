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
By AWI () 2015
 Class inspired by "Bounce2" library. 
 Main code by Thomas O Fredericks (tof@t-o-f.info)
 Previous contributions by Eric Lowry, Jim Schimpf and Tom Harkaway
* * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#if defined(ARDUINO) && ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif
#include "LedFlash.h"

  // Create an instance of LedFlash
  LedFlash::LedFlash(int pin, bool activeHigh, unsigned long flash_pulse, unsigned long flash_period ) {
	_flash_freq = flash_period;				// default flash period (ms)
	_flashPause = flash_period-flash_pulse;
	_flashPulse = flash_pulse;
	_pin = pin;					// assign pin
	_curState = LOW;
	_flash_stat = F_OFF ;
	_activeHigh = activeHigh ;
	pinMode(_pin, OUTPUT );
	digitalWrite(_pin,_activeHigh ^ LOW);	// set Led to low
	_lastUpdate = millis();				// set timer value
}

  // Attach to a pin and set initial state
  void LedFlash::attach(int pin) {
	_pin = pin;					// assign pin
	_curState = LOW;
	_flash_stat = F_OFF ;
	digitalWrite(_pin, _activeHigh ^ LOW);	// set Led to low
	_lastUpdate = millis();				// set timer value
	}

	// Sets the led flash period
  void LedFlash::period(unsigned long flash_freq, unsigned long flashPulse) {
	_flash_freq = flash_freq;
	if (flashPulse == 0){
		_flashPulse = flash_freq / 2 ; 				// if pulse width not specified then half period
	} else {
		_flashPulse = flashPulse ;
	}
	_flashPause = flash_freq - _flashPulse;
	}	

  // Sets the led to flash
  void LedFlash::flash(int duration) {
	_flash_stat = F_FLASH;
	_lastUpdate = millis();				// set timer value
	_curState = HIGH;					// start with LED on (pulse)
	if (duration != 0){
		_flashTime = duration * 1000 + millis() ;  // set the timer values to milliseconds from now
		_flashTimer = true ;
	}
  }	

  // Sets the led to flash
  void LedFlash::count(int counts) {
	_flash_stat = F_FLASH;
	_lastUpdate = millis();				// set timer value
	_curState = HIGH;					// start with LED on (pulse)
	_count = 1 ;						// start with one count
	if (counts != 0){
		_flashCount = counts ; 		 	// set the counter
		_flashCounter = true ;
		_flashTimer = false ;
	}
  }	

  
  // Sets the led to on
  void LedFlash::on(int duration) {
	_flash_stat = F_ON;
	_curState = HIGH;
	if (duration != 0){
		_flashTime = duration * 1000 + millis() ;  // set the timer values to milliseconds from now
		_flashTimer = true ;
		_flashCounter = false ;
	}
  }	

  // Sets the led to off
  void LedFlash::off(void) {
	_flash_stat = F_OFF;
	_curState = LOW;
	_count = 0;
  }	

  // Sets the Led to perform current (or future) action for x seconds
  void LedFlash::timer(int Seconds){
	_flashTime = Seconds * 1000 + millis() ;  // set the timer values to milliseconds from now
	_flashTimer = true ;
	_flashCounter = false ;
	_count = 1; // Starts with at least 1 flash
  }

  // Sets the Led to perform current (or future) action for x counts
  void LedFlash::counter(int Counts){
	_flashCount = Counts ;  				// set the counter to 
	_flashCounter = true ;
	_flashTimer = false ;
	_count = 0; // Starts at count == 0
  }

  
  // returns the number of pulses from start of last flash
  unsigned int LedFlash::count(void){
	  return _count;
  }
  
	// Updates the led
	// Returns true if On, false if Off
  bool LedFlash::update(void) {
	if (_flash_stat == F_FLASH){ 				// if flashing change state after xx millis
		if (_curState == LOW){					// pause period 
			if (millis()-_lastUpdate > _flashPause){
				_curState = HIGH;
				_count++ ; 						// increment flash counter
				_lastUpdate = millis();
			}
		} else {								// pulse period
			if (millis()- _lastUpdate > _flashPulse){
				_curState = LOW;
				_lastUpdate = millis();
			}
		}
	}
	if (_flashTimer){					// if timer is on check if expired
		if (millis() > _flashTime){
			_flashTimer = false ; 		// if expires switch off and reset Timer status
			_flash_stat = F_OFF ;
			_curState = LOW ;
		}
	}
	if (_flashCounter){					// if counter is on check if expired
		if (_count > _flashCount){
			_flashCounter = false ;		// if expires switch off and reset Timer status
			_flash_stat = F_OFF ;
			_curState = LOW ;
		}

	}
	digitalWrite(_pin, _activeHigh ^ _curState);	// update led with current status
	// Returns the updated pin state
	return _curState;
  }