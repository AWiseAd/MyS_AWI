/*
||
|| @file FiniteStateMachine.cpp
|| @version 1.7
|| @author Alexander Brevig
|| @contact alexanderbrevig@gmail.com
||
|| @description
|| | Provide an easy way of making finite state machines
|| #
||
|| @license
|| | This library is free software; you can redistribute it and/or
|| | modify it under the terms of the GNU Lesser General Public
|| | License as published by the Free Software Foundation; version
|| | 2.1 of the License.
|| |
|| | This library is distributed in the hope that it will be useful,
|| | but WITHOUT ANY WARRANTY; without even the implied warranty of
|| | MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
|| | Lesser General Public License for more details.
|| |
|| | You should have received a copy of the GNU Lesser General Public
|| | License along with this library; if not, write to the Free Software
|| | Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
|| #
||
*/


#if defined(ARDUINO) && ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif
#include "FiniteStateMachine.h"


//FINITE State
FState::FState( void (*updateFunction)() ){
	userEnter = 0;
	userUpdate = updateFunction;
	userExit = 0;
}

FState::FState( void (*enterFunction)(), void (*updateFunction)(), void (*exitFunction)() ){
	userEnter = enterFunction;
	userUpdate = updateFunction;
	userExit = exitFunction;
}

//what to do when entering this state
void FState::enter(){
	if (userEnter){
		userEnter();
	}
}

//what to do when this state updates
void FState::update(){
	if (userUpdate){
		userUpdate();
	}
}

//what to do when exiting this State
void FState::exit(){
	if (userExit){
		userExit();
	}
}
//END FINITE STATE


//FINITE STATE MACHINE
FiniteStateMachine::FiniteStateMachine(FState& current){
	needToTriggerEnter = true;
	currentState = nextState = &current;
	stateChangeTime = 0;
}

FiniteStateMachine& FiniteStateMachine::update() {
	//simulate a transition to the first state
	//this only happens the first time update is called
	if (needToTriggerEnter) { 
		currentState->enter();
		needToTriggerEnter = false;
	} else {
		if (currentState != nextState){
			immediateTransitionTo(*nextState);
		}
		currentState->update();
	}
	return *this;
}

FiniteStateMachine& FiniteStateMachine::transitionTo(FState& state){
	nextState = &state;
	stateChangeTime = millis();
	return *this;
}

FiniteStateMachine& FiniteStateMachine::immediateTransitionTo(FState& state){
	currentState->exit();
	currentState = nextState = &state;
	currentState->enter();
	stateChangeTime = millis();
	return *this;
}

//return the current state
FState& FiniteStateMachine::getCurrentState() {
	return *currentState;
}

//check if state is equal to the currentState
boolean FiniteStateMachine::isInState( FState &state ) const {
	if (&state == currentState) {
		return true;
	} else {
		return false;
	}
}

unsigned long FiniteStateMachine::timeInCurrentState() { 
	return millis() - stateChangeTime; 
}
//END FINITE STATE MACHINE
