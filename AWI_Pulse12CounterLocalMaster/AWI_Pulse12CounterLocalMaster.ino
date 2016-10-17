/*-------------------------------------------------------------------------*
 * MySensors interface to AWI 12 input usage/ power Meter                  *
 * measures Power/ Usage/ Cumm. usage for upto 12 independent channels.    *
 * 20x4 (I2C) Display shows consumption                                    *
 * Update:                                                                 *
 * 20150821 - removed dependency on controller, added local history        *
 *	- New rotary encoder library with acceleration and click               *
 *	- Changed to simple State machine                                      *
 *                                                                         *
 *-------------------------------------------------------------------------*/ 
 /*
//
// Licence: GNU GPL
//
// Author: AWI 2015
// Updates: AWI 20150815, new RotaryEncoder & ArduinoJson libraries
// 20150821 - removed dependency on controller, added local history and update
// 20151211 - moved to MYsensors development 1.6
// 20150102 - Remove ArduinoJSON lib dependency
//	- Changed rotary encoder library
//	- Changed to simple "state machine"
//  
//
// Functionality:
// continuously:
// 1. read JSON strings from Pulse12Counter slave , format {"m":meter,"c":count,"r":rate, "cA":countAccum}
// 2. calculate real time Power from "pulse rate"
// 3. daily counters
// 4. send to controller
//
// Changed
// 1. Total Wh values are now stored local and are only sent to controller 
//
// Other:
// 1. display on LCD, time & energy values, usage can be shown as Daily & Total 
// 2. rotary encoder to browse the different displays
// 3. rotary encoder used to change the local values, stored in EEPROM every day and after update
// 4. depending on display encoder has "short/ long/ click/ double clik" functionality
// 5. Uses new (201509) V_TEXT to display text from controller. 
// Caution: pulse meter is connected to std serial (pin D0), Serial.print can still be used  
//    Disconnect pulse meter when programming via FTDI! (else sync error)
 */
#define MY_NODE_ID 24					// fixed MySensors node ID
#define NODE_TXT "Power 24"				// Text to add to sensor name

// #define MY_DEBUG 
#define MY_RADIO_NRF24					// Enable and select radio type attached
 
#include <MySensor.h>                 	// MySensors network
#include <SPI.h>
#include <LiquidCrystal_I2C.h>        	// display I2C
#include <Time.h> 
#include <Wire.h> 
#include <ClickEncoder.h>				// Lib includes click functionality: https://github.com/0xPIT/encoder/tree/arduino
#include <TimerOne.h>					// for ClickEncoder interrupt calls
//#include <ArduinoJson.h>          		// used to parse the simple JSON output of the pulse meter https://github.com/bblanchon/ArduinoJson

// Constants & globals
const int NO_METERS = 6 ;				// actual meters used (max 12)
const int NODE_ID = 24 ;             	
const int LCD1_CHILD = 20 ;				// custom text message child, Controller can display message 
const int JSON_LENGHT = 60 ;			// Maximum json string length

// new V_TEXT variable type (development 20150905)
//const int V_TEXT = 47 ;
// new S_INFO sensor type (development 20150905)
//const int S_INFO = 36 ;

char lastLCD1[21] = "--                  ";	// LCD message line

typedef enum meterTypes: int8_t {meterIn, meterOut, meterNeutral} ;				// metertype for addition in totals: in, out, neutral
const char METER_NAMES[NO_METERS][4] = {"tst", "Gr2","EA ","Pv1","Pv2", "Gr1"} ; // meter names
const meterTypes METER_TYPES[NO_METERS] = {meterNeutral, meterOut, meterOut, meterIn, meterIn, meterOut};

const unsigned long idleTime = 10000 ;	// Delay time for any of the states to return to idle
unsigned long idleTimer ;				// Delay timer for idleTime

// RotaryEncoder
const int8_t encPinA = A1;				// Encoder pins
const int8_t encPinB = A0;
const int8_t encPinButton = A2;
const int8_t encStepsNotch = 2;   		// tune for best stepsize (1..4, 1=default)
const bool encButton = LOW ;      		// active low pushbutton
ClickEncoder encoder(encPinA, encPinB, encPinButton, encStepsNotch, encButton);	// instantiate the encoder

union {									// used to convert long to bytes for EEPROM storage
	long kWhLongInt;
	uint8_t kWhLongByte[4];
	} kWhLong ;


// Possible states for the state machine.
// Idle: default state, show totals for in/out nett
// Browse: dive into meter details 
// Update: update meter value (Wh total)
// Reset: reset day values
enum States: int8_t {IDLE, BROWSE, UPDATE, RST} ;
uint8_t State = IDLE;					// current state machine state

// meter class, store all relevant meter data (equivalent to struct)
class pulseMeter              
{
public:                                  
	long UsageWh; 						// last (current) usage (in W) from pulse counter
	long UsageAccumWh;					// usage accumulator (to keep in sync) from pulse counter
	long PowerW;						// actual power, calculated from pulse "rate"
	long DayUsageWh;					// daily usage for display
	meterTypes Type;					// metertype for addition in totals: in, out, neutral
	char Name[4] ;						// meter name for display
};
pulseMeter pulseMeters[NO_METERS] ;		// define power meters
pulseMeter totalMeterIn, totalMeterOut, totalMeter;	//  accumulated
unsigned long tempUsageWh ;				// temporary store while manually updating (state UPDATE) 

// example: "{\"m\":1,\"c\":12,\"r\":120000,\"cA\":12345}"
char json[JSON_LENGHT] ;		// Storage for serial JSON string (init for example)

// flags & counters 
bool timeReceived = false;				// controller time
bool newDay = false;					// reset at 00:00:00
unsigned long lastUpdate=0, lastRequest=0, lastDisplay=0, lastSyncKWH=0;  // loop timers for once in while events
int updateMeter = 0;					// current meter for update
bool updateDisplayFlag = true ;			// indicate that display needs to be updated
int currentMeter = 0;					// active meter for update & check, cycles through meters (0..NO_METERS-1)
int lastRotary = 0;						// last rotary encoder position
int updateIncrement = 1000 ;			// Interval multiplier for Update 
int errCount = 0 ;						// error counter
// *** Definition and initialisation
// define the MySensor network
// MySensor gw;							// pins used RFX24(default 9,10)
    
// Initialize messages for sensor network
MyMessage powerMsg(0,V_WATT); 	        // message to send power in W
MyMessage usageMsg(0,V_KWH);    	    // message to send usage in kWH
MyMessage textMsg(0,V_TEXT);    	    // message to send/receive text


// Set the pins on the I2C chip used for LCD connections:
//                    addr, en,rw,rs,d4,d5,d6,d7,bl,blpol
LiquidCrystal_I2C lcd(0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);  // Set the LCD I2C address

// OPTIONAL: Custom characters for display - Units)
byte degCelcius[8] = { B01000, B10100, B01000, B00111, B00100, B00100, B00111, B00000};
//byte Pascal[8] = { B00000, B11100, B10100, B11100, B10010, B10111, B10101, B00000};
//byte hecto[8] = { B00000, B00000, B00000, B00100, B00110, B00101, B00101, B00000};
//byte Lux[8] = { B00000, B10000, B10101, B10010, B10010, B10101, B11100, B00000};
byte hr[8] = { B00000, B00000, B10000, B10000, B11000, B10100, B10100, B00000};			// small hour symbol
byte perDay[8] = { B00000, B00000, B00001, B00001, B00011, B00101, B00011, B00000};

void timerIsr() {						// RotaryEncoder timer interrupt routinge
	encoder.service();
	}

// function to reset the Arduino (jump to 0 address)
void(* resetFunc) (void) = 0;//declare reset function at address 0

void setup(void)
{
	//Send the sensor node sketch version information to the gateway
	sendSketchInfo("AWI-12ChannelPulse", "2.0");
	// Only for first start !!!! Initialize the meter totals in EEPROM
	//initMetersEE() ;
	// Register all Pulse counters to gw (they will be created as child devices from 0 to MAX-1)
	for (int x = 0; x < NO_METERS; x++){ 
		present(x, S_POWER, METER_NAMES[x]);             	// present power meters to gateway
		delay(10);                        					// give it some time to process
		}
	present(LCD1_CHILD, S_INFO, "Usage meter LCD");			// present the child for custom text line (20150906)
	delay(100);
	send(textMsg.setSensor(LCD1_CHILD).set("-"));			// initialize the V_TEXT at controller for sensor to none (trick for Domoticz)
	
	for (int x = 0; x < NO_METERS; x++){ 					// initialize previous kWh values from EEPROM and init
		for (int y = 0; y < 4 ; y++){						// convert from bytes
			kWhLong.kWhLongByte[y]= loadState(x *4 + y) ;// EEPROM position = meter number * 4 bytes
			}												// controller is updated later automatically
		pulseMeters[x].UsageWh = kWhLong.kWhLongInt;
		strcpy(pulseMeters[x].Name, METER_NAMES[x] ); 		// copy string for meter names
		pulseMeters[x].Type = METER_TYPES[x];				// Set type
		}
	
	// Initializations
	Wire.begin();                 							// I2C for display
	
	Timer1.initialize(1000);								// interrupt calls for RotaryEncoder
	Timer1.attachInterrupt(timerIsr);
	encoder.setAccelerationEnabled(true);

	requestTime();											// Request latest time from controller at startup

	// ** LCD display **
	lcd.begin(20, 4);										// LCD 2 lines * 16 char.
	lcd.setBacklight(HIGH);
	// send custom characters to display
	lcd.createChar(1, degCelcius);
	lcd.createChar(4, hr);
	lcd.createChar(7, perDay);
	lcd.setCursor(0, 0);             						// Reset cursor position
}

void loop(void)
{
	// Before specific states, perform generic tasks
	unsigned long now = millis();      						// Timer in loop for "once in a while" events
	// process() ;                      					// process incoming messages
	// If no time has been received yet, request it every 10 second from controller
	if ((!timeReceived && (now-lastRequest > 10UL * 1000UL)) ||	
		(now-lastRequest > 60UL*1000UL*60UL)){				// request update every hour to keep in sync
		//Serial.println(F("requesting time"));				// Request time from controller. 
		timeReceived = false;
		requestTime();  
		lastRequest = now;
    }
	// Check if new day has started (hour == 0) and reset day usage counters of meters
	if (hour()==0 && !newDay){
		newDay = true;
		for (int x = 0; x < NO_METERS; x++){ 
			pulseMeters[x].DayUsageWh = 0 ;					// reset daily counters
			saveMeters(x);									// save meter values to EEPROM
		}   
	} else if(hour() != 0 && newDay)						// reset newday flag if hour != 0
		{ newDay = false;}
  
	// State machine depends on RotaryEncoder and time actions
	ClickEncoder::Button rotaryButton = encoder.getButton();// Get the button state
	switch (State) {
        // Idle state, browse through displays, watches for key presses
        // and changes state accordingly, while taking care of next state init
        case IDLE:                
            if (rotaryButton == ClickEncoder::Clicked) {
				idleTimer = now ;										// set delay timer for return to idle
				State = BROWSE ;
				updateDisplayFlag = true ;}  							// only change state
            else if (rotaryButton == ClickEncoder::Held) {
				idleTimer = now ;
				updateDisplayFlag = true ;
				State = RST;}											// Enter Reset state with Long press.
            //else if (rotaryButton == ClickEncoder::Released) {} 		// do nothing
			//else if (rotaryButton == ClickEncoder::Pressed) {} 		// do nothing
            //else if (rotaryButton == ClickEncoder::DoubleClicked) {} 	// do nothing
			else {}// perform Idle actions: browse display)
			break ;
 		case BROWSE:
            if (rotaryButton == ClickEncoder::Clicked) {State = IDLE ;	// return to IDLE on click
				updateDisplayFlag = true ;} 
            else if (rotaryButton == ClickEncoder::Held) {				// update the current displayed value.
				tempUsageWh = pulseMeters[currentMeter].UsageWh ;		// use temp variable for update (background changes)
				idleTimer = now ;										// set delay timer for return to idle
				updateDisplayFlag = true ;
				State = UPDATE ; 
				}
            //else if (rotaryButton == ClickEncoder::Released) {break;} // do nothing
			//else if (rotaryButton == ClickEncoder::Pressed) {break;} 	// do nothing
            //else if (rotaryButton == ClickEncoder::DoubleClicked) {break;} // do nothing
			else if (now > idleTimer + idleTime){State = IDLE ;}		// return to idle after expiration of delay
			else {// perform Idle actions: browse display)
				int enc = encoder.getValue();							// get the value from the encoder
				if (enc != 0)idleTimer = now ;							// set delay timer for return to idle
				if( enc > 0) {											// cycle through displays depending on up/ down (actual value not used)
					if (currentMeter++ >= NO_METERS - 1 ) currentMeter = 0;
					lastRotary = enc ;
					updateDisplayFlag = true ;
				}
				else if (enc < 0) {
					if (currentMeter-- <=  0) currentMeter = NO_METERS - 1;
					lastRotary = enc ;
					updateDisplayFlag = true ;
				}
			}
			break ;
		case UPDATE:                									// Double state for large (start) & small increments
 			if (rotaryButton == ClickEncoder::Clicked) {
				if (updateIncrement == 1) {								// Save values and return to idle
					updateIncrement = 1000 ; 							// increment in kWh (x1000, for next time)
					pulseMeters[currentMeter].UsageWh = tempUsageWh;	// save temp to actual
					saveMeters(currentMeter);							// !!Save the current value to EEPROM (controller will be updated automatically)
					updateDisplayFlag = true ;
					State = BROWSE; 									// return to browse after update
				} else{													// increment in Wh (x1)
					updateIncrement = 1 ;
				}
			}
            //else if (rotaryButton == ClickEncoder::Held)  break; 		// (use as cancel function?)
            //else if (rotaryButton == ClickEncoder::Released) break; 	// do nothing
			//else if (rotaryButton == ClickEncoder::Pressed) break; 	// do nothing
            //else if (rotaryButton == ClickEncoder::DoubleClicked) break; 		// do nothing
			else if (now > idleTimer + idleTime){
				updateDisplayFlag = true ;
				State = IDLE ;}		// return to idle after expiration of delay
			else {														// perform Update actions: change current value
				int16_t enco = encoder.getValue();						// get the value from the encoder
				if (enco != 0) {
					updateDisplayFlag = true ;
					tempUsageWh += enco * updateIncrement ;				// update the temporary value from the encoder}
					idleTimer = now ;}									// set delay timer for return to idle
				}
            break ;
		case RST:														// reset (jump to reset if Held again)
			if (rotaryButton == ClickEncoder::DoubleClicked) {
				State = IDLE ; 											//reset
				for (int x = 0; x < NO_METERS; x++){ 
					pulseMeters[x].DayUsageWh = 0 ;						// reset daily counters
					saveMeters(x);										// save meter values to EEPROM
					}				
				errCount = 0 ;											// reset Json err counter
				updateDisplayFlag = true ;
				}
			else if (rotaryButton == ClickEncoder::Clicked) {
				updateDisplayFlag = true ;
				State = IDLE ;}	// click = return to Idle
			else if (now > idleTimer + idleTime){
				updateDisplayFlag = true ;
				State = IDLE ;}		// return to idle after expiration of delay
			break ;
		}
	

	// Update display every 1 second (IDLE) every loop (Other states)
	//if (((State == IDLE) && (now-lastUpdate > 100)) || State != IDLE ){
	if (now-lastDisplay > 1000){ 
		updateDisplayFlag = true ;
		lastDisplay = now;
		//Serial.print("State: ");
		//Serial.println(State);
		}		
	if (updateDisplayFlag) {
		LCD_local_display();
		updateDisplayFlag = false ;
		}
    
	// Every 10 seconds update one meter to controller to avoid traffic jams
 	if (now-lastSyncKWH > 10000){
		//printPulsemeter(updateMeter);
		sendPowerUpdate(updateMeter);        						// update the values for currentMeter
		updateMeter++ ;
		if (updateMeter >= NO_METERS){     							// increment and wrap current meter
			updateMeter = 0 ;}
		lastSyncKWH = now ;
		}
		
	// Update text sensor every 100 secs
	if (now-lastUpdate > 100000) {
		// get values to be displayed from controller
		request(LCD1_CHILD, V_TEXT);		 						// LCD text message
		lastUpdate = now;
	}
	
	// get readings from serial (sent every 10s)
	// format {"m":meter,"c":count,"r":rate, "cA":countAccum}
	if(readLineJSON(Serial.read(), json, JSON_LENGHT) > 0 ){   		//dummySerial(), Serial.read()
		Serial.println(json);
		storeMeterJSON(json);           							//store the meter reading
		calcMeterTotals();											// update totals
		}
}

// This is called when a new time value was received
void receiveTime(unsigned long controllerTime) {
    // Ok, set incoming time 
    //Serial.print(F("Time value received: "));
    //Serial.println(controllerTime);
    setTime(controllerTime); 										// set the clock to the time from controller
	timeReceived = true ;
}

// Handle incoming messages from the MySensors Gateway
void receive(const MyMessage &message) {  // Expect few types of messages from controller, V_VAR1 for messages
	if (message.type==V_TEXT) {
		// if message comes in, update the kWH reading for meter with value since last update
		// Write some debug info
		//Serial.print("Last reading for sensor: ");
		//Serial.print(message.sensor);                
		//Serial.print(", Message: ");
		//Serial.println(message.getString());
		if (message.sensor == LCD1_CHILD ) {
			strcpy(lastLCD1, message.getString());	// read payload in LCD string
		}
	}
}
// initialize Meters in EEPROM
/*
void initMetersEE(){
	const unsigned long initValues[NO_METERS] = {0, 2172760UL, 1311150UL, 2226290UL, 6978590UL, 3385840UL};
	for (int i = 0 ; i < NO_METERS ; i++ ){									// for all the meters
		pulseMeters[i].UsageWh = initValues[i] ; 							// convert to separate bytes via struct
		Serial.print("Meter: ") ; Serial.print(i) ; Serial.print(" : ") ; Serial.println(pulseMeters[i].UsageWh) ;
		saveMeters(i) ;														// save the meter values to EEPROM
		}
	}
*/

// save Meter to EEPROM when needed
void saveMeters(int8_t meterNo){
	kWhLong.kWhLongInt = pulseMeters[meterNo].UsageWh ; 					// convert to separate bytes via struct
	for (int y = 0; y < 4 ; y++){
		saveState(meterNo * 4 + y, kWhLong.kWhLongByte[y])  ;				// EEPROM position = meter number * 4 bytes
		}
	}

void sendPowerUpdate(int meterNo)
// Sends update to controller for current meter 
{
    send(powerMsg.setSensor(meterNo).set((long)pulseMeters[meterNo].PowerW));			// meterNo * 100 ));
    send(usageMsg.setSensor(meterNo).set((float)pulseMeters[meterNo].UsageWh/1000L ,3)); // send in kWh!
}

//  calculate the total values for from the meters
//  Total in and out
void calcMeterTotals(void)
{	
	totalMeterIn.UsageWh = 0 ;
	totalMeterIn.PowerW = 0 ;
	totalMeterIn.DayUsageWh = 0 ;
	totalMeterOut.UsageWh = 0 ;
	totalMeterOut.PowerW = 0 ;
	totalMeterOut.DayUsageWh = 0 ;
	for (int x = 0; x < NO_METERS; x++){ 					// add In and Out meters to totals
		if (pulseMeters[x].Type == meterIn){				
			totalMeterIn.UsageWh += pulseMeters[x].UsageWh;
			totalMeterIn.PowerW += pulseMeters[x].PowerW ;
			totalMeterIn.DayUsageWh += pulseMeters[x].DayUsageWh ;
		}
		else if (pulseMeters[x].Type == meterOut){
			totalMeterOut.UsageWh += pulseMeters[x].UsageWh;
			totalMeterOut.PowerW += pulseMeters[x].PowerW ;
			totalMeterOut.DayUsageWh += pulseMeters[x].DayUsageWh ;
		}
	} // else = neutral, do nothing 
	totalMeter.UsageWh = totalMeterOut.UsageWh - totalMeterIn.UsageWh ;		// calculate nett values
	totalMeter.PowerW = totalMeterOut.PowerW - totalMeterIn.PowerW ;
	totalMeter.DayUsageWh = totalMeterOut.DayUsageWh - totalMeterIn.DayUsageWh ;
}

void LCD_local_display(void)
// prints variables on LCD display with units, depending on current State
// IDLE = totals
// BROWSE = individual meters
// SET = individual meter & value
{
   	//long loopDelay = millis();									// Test loop time
	char buf[21]; 												// buffer for display
	char tempBuf[11];											// Temporary storage for float -> char conversion
	// always, time on first line
	snprintf(buf, sizeof buf, "%02d:%02d:%02d %02d-%02d %5s", hour(), minute(), second(), day(), month(), lastLCD1);
	lcd.setCursor(0,0);               							// LCD line 1
	lcd.print(buf);
	// State specific (line 2, 3, 4)
	switch (State) {
		case IDLE:											
			// display In meter total on line 2
			dtostrf((float)totalMeterOut.DayUsageWh /1000.0, 6, 2, tempBuf);
			snprintf(buf, sizeof buf, "Use: %6skW\x04%5dW  ", tempBuf, (int)totalMeterOut.PowerW );
			lcd.setCursor(0,1);               					// LCD line 2
			lcd.print(buf);
			// display Out meter total on line 3
			dtostrf(totalMeterIn.DayUsageWh /1000.0, 6, 2, tempBuf);
			snprintf(buf, sizeof buf, "Gen: %6skW\x04%5dW  ", tempBuf, (int)totalMeterIn.PowerW );
			lcd.setCursor(0,2);               					// LCD line 3
			lcd.print(buf);
			// display Net meter total on line 4
			dtostrf(totalMeter.DayUsageWh /1000.0, 6, 2, tempBuf);
			snprintf(buf, sizeof buf, "Net: %6skW\x04%5dW  ", tempBuf, (int)totalMeter.PowerW );
			lcd.setCursor(0,3);               					// LCD line 4
			lcd.print(buf);
			break;
		case BROWSE:
			// meter indicated by "currentMeter"
			dtostrf((float)pulseMeters[currentMeter].DayUsageWh /1000.0, 6, 2, tempBuf);
			snprintf(buf, sizeof buf, "%4s%6skW\x04\x07%5dW  ", pulseMeters[currentMeter].Name, tempBuf, (int)pulseMeters[currentMeter].PowerW );
			lcd.setCursor(0,1);               					// LCD line 2
			lcd.print(buf);
			// display total for meter line 3,
			dtostrf((float)pulseMeters[currentMeter].UsageWh /1000.0, 10, 2, tempBuf);
			snprintf(buf, sizeof buf, "tot:%10skW\x04          ",tempBuf);
			lcd.setCursor(0,2);               // LCD line 3
			lcd.print(buf);
			// TEST: display full message on line 4
			lcd.setCursor(0,3);               // LCD line 4
			snprintf(buf, sizeof buf, "%20s", lastLCD1);
			lcd.print(buf);
			break;
		case UPDATE:
			// meter indicated by "currentMeter"
			dtostrf((float)pulseMeters[currentMeter].DayUsageWh /1000.0, 6, 2, tempBuf);
			snprintf(buf, sizeof buf, "%4s%6skW\x04\x07%5dW  ", pulseMeters[currentMeter].Name, tempBuf, (int)pulseMeters[currentMeter].PowerW );
			lcd.setCursor(0,1);               // LCD line 2
			lcd.print(buf);
			// display current value on line 3
			lcd.setCursor(0,2);               // LCD line 3
			dtostrf((float)pulseMeters[currentMeter].UsageWh /1000.0, 10, 3, tempBuf);
			snprintf(buf, sizeof buf, "tot:%10skW\x04 \x7E      ",tempBuf);
			lcd.print(buf);
			// display update value on line 4
			lcd.setCursor(0,3);               // LCD line 4
			dtostrf((float)tempUsageWh /1000.0, 10, 3, tempBuf);
			snprintf(buf, sizeof buf, "tot:%10skW\x04          ",tempBuf);
			lcd.print(buf);
			break;
		case RST:
			//snprintf(buf, sizeof buf, "Use: %6skW\x04%5dW  ", tempBuf, (int)totalMeterOut.PowerW );
			lcd.clear();
			lcd.setCursor(0,0);               					// LCD line 2
			lcd.print("Doubleclick to reset");
			lcd.setCursor(0,2);
			snprintf(buf, sizeof buf, "JSON err count: %d",errCount);
			lcd.print(buf);
		default:												// default display
			break;
	}
}



//int storeMeterJSON(char *json)
/* convert JSON to values and store in corresponding meter (if used)
 input: JSON string (can be wrong formatted), with length
 output: changed meter record number or -1 if error
 use JsonParser 
*/
/*{
  StaticJsonBuffer<50> jsonBuffer; 							// 4 object  -> 4 + 4*10 = 44
  // char njson[] = "{\"m\":1,\"c\":12,\"r\":120000,\"cA\":12345}";
    JsonObject& root = jsonBuffer.parseObject(json);
  if (!root.success())
    {
        Serial.println(F("JsonParser.parse() failed"));
		errCount++ ;
        return -1;
    }
  int m = (long)root["m"];
  if (m > NO_METERS){                 						// meter value out of range for used meters (m starts at 1)
    return -1 ;
  } else {                     								// update meter values, Power is momentary, Usage is cumulative
	long newAccumWh = (long)root["cA"] ;
	long newWh = (long)root["c"] ;
	long diffAccumWh = newAccumWh - pulseMeters[m-1].UsageAccumWh; 	// check for missed pulses by comparing cA with last stored value
	//if (diffAccumWh >= newWh){								// no difference or missed pulses -> correct: add difference
	//	pulseMeters[m-1].UsageWh += diffAccumWh;
	//	pulseMeters[m-1].DayUsageWh += diffAccumWh;
	//	}
	//else {													// negative diff, out of sync -> add pulses only (can be out of range or restart)
		pulseMeters[m-1].UsageWh += newWh;
		pulseMeters[m-1].DayUsageWh += newWh;
	//	}
	pulseMeters[m-1].UsageAccumWh = newAccumWh; 			// always update sync counter (only for sync and error correction(serial))
    if ((long)root["r"] == 0){            					// calculate power from pulse rate (ms) and truncate to whole Watts
      pulseMeters[m-1].PowerW = 0;							// if overflow assume no Usage
    } else {
      pulseMeters[m-1].PowerW = long( 3600000000L / (long)root["r"]); // rate in microseconds
    }
    return m ;
  }
}
*/

int storeMeterJSON(char *json)
/* convert JSON to values and store in corresponding meter (if used)
 input: JSON string (can be wrong formatted), with length
 output: changed meter record number or -1 if error
 use simple parser strtok (input string will be  modified)
*/
{
	// int m ; 												// meternumber
	// long newWh ;											// current Wh
	// long newRate ; 										// current pulserate
	// long newAccumWh ;									// current accumulated Wh

	
	if (json[0] != '{'){									// check if first character is {
        Serial.println(F("Wrong input"));
		errCount++ ;
        return -1;
    }
	strtok(json, ":");										// strip "m" indicator
	int m = atoi(strtok(NULL,","));							// get first token = meter number
	if (m > NO_METERS){                 					// meter value out of range for used meters (m starts at 1)
		errCount++ ;
		return -1 ;
	} else {                     							// update meter values, Power is momentary, Usage is cumulative
		strtok(NULL, ":");									// strip "c" indicator = pulse count (Wh)
		long newWh = atol(strtok(NULL,","));
		pulseMeters[m-1].UsageWh += newWh;
		pulseMeters[m-1].DayUsageWh += newWh;
		strtok(NULL, ":");									// strip "r" indicator = pulse rate 
		long newRate  = atol(strtok(NULL,","));				// 
		if (newRate == 0){            						// calculate power from pulse rate (ms) and truncate to whole Watts
			pulseMeters[m-1].PowerW = 0;					// if overflow assume no Usage
		} else {
			pulseMeters[m-1].PowerW = long( 3600000000L / newRate); // rate in microseconds
		}
    return m ;
  }
}

int readLineJSON(int readch, char *buffer, int len)
/* checks for JSON and when started append char tot buffer and checks for line completion 
usage:
  static char buffer[80];
  if (readline(Serial.read(), buffer, 80) > 0) { // line complete}
  returns simple JSON
  */
{
  static int pos = 0;
  int rpos;

  if (readch > 0) {
    switch (readch) {
      case '\n':                // Ignore new-lines
        break;
      case '\r':                // Return on CR
        rpos = pos;
        pos = 0;                  // Reset position index ready for next time
        return rpos;
      default:
        if (pos < len-1) {
          buffer[pos++] = readch;
          buffer[pos] = 0;
        }
    }
  }
  // No end of line has been found, so return -1.
  return -1;
}


/*
void printPulsemeter(int meter)
// prints the Pulsemeter record to serial out
{
  Serial.print("m:");
  Serial.print(meter);
  Serial.print(", power: ");
  Serial.print(pulseMeters[meter].PowerW);
  Serial.print(", usage: ");
  Serial.print(pulseMeters[meter].UsageWh);
  Serial.print(", day usage: ");
  Serial.println(pulseMeters[meter].DayUsageWh );
//  Serial.print(", Ca: ");
//  Serial.println(pulseMeters[meter].UsageAccumWh);
}
*/
