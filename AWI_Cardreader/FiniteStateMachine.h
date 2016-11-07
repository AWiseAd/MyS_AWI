//#ifndef FiniteStateMachine_h
//#define FiniteStateMachine_h

#include <inttypes.h>

//define the functionality of the states
class FState {
	public:
		FState( void (*updateFunction)() );
		FState( void (*enterFunction)(), void (*updateFunction)(), void (*exitFunction)() );
		//FState( byte newId, void (*enterFunction)(), void (*updateFunction)(), void (*exitFunction)() );
		
		//void getId();
		void enter();
		void update();
		void exit();
	private:
		//byte id;
		void (*userEnter)();
		void (*userUpdate)();
		void (*userExit)();
};


//define the finite state machine functionality
class FiniteStateMachine {
	public:
		FiniteStateMachine(FState& current);
		
		FiniteStateMachine& update();
		FiniteStateMachine& transitionTo( FState& state );
		FiniteStateMachine& immediateTransitionTo( FState& state );
		
		FState& getCurrentState();
		bool isInState( FState &state ) const;
		
		unsigned long timeInCurrentState();
		
	private:
		bool 	needToTriggerEnter;
		FState* 	currentState;
		FState* 	nextState;
		unsigned long stateChangeTime;
};
