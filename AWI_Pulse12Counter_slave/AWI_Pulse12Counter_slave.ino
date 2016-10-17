//---------------------------------------------------------------------------------------------
// Arduino Pulse Counting Sketch for counting pulses from up to 12 pulse output meters.
// uses direct port manipulation to read from each register of 6 digital inputs simultaneously
//
// Licence: GNU GPL
// part of the openenergymonitor.org project
//
// Author: Trystan Lea
// AWI: adapted to produce JSON and error checking at client side.
//---------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------
// Pulse Counting Class - could be placed in seperate library...
//---------------------------------------------------------------------------------------------
class PulseOutput
{
public:                                 		 //AWI: access to all
  boolean pulse(int,int,unsigned long);                  //Detects pulses, in pulseLib.ino
  unsigned long rate( unsigned long );                   //Calculates rate 

  unsigned long count;                                   //pulse count accumulator
  unsigned long countAccum;                              //pulse count total accumulator for extended error checking (only resets at startup)
  unsigned long prate;                                   //pulse width in time 
  unsigned long prateAccum;                              //pulse rate accumulator for calculating mean.

private:
  boolean ld,d;                                          //used to determine pulse edge
  unsigned long lastTime,time;                           //used to calculate rate
};

//---------------------------------------------------------------------------------------------
// Variable declaration
//---------------------------------------------------------------------------------------------

//CHANGE THIS TO VARY RATE AT WHICH PULSE COUNTING ARDUINO SPITS OUT PULSE COUNT+RATE DATA
//time in seconds;
const unsigned long printTime = 1000000;	// delay between serial outputs in us (one meter at a time)  
const int lastMeter = 7 ; 	 	// is number of meters + 1

byte curMeter = 2 ;				// current meter for serial output, wraps from 2 to lastMeter

//---------------------------------------------------------------------------------------------
PulseOutput p[14];            //Pulse output objects

int a,b,la,lb;                //Input register variables

unsigned long ltime, time;    //time variables

void setup()
{
 // take care: pull-up inverses state! line 155
	//setup input pins here with pull_up, else (default) float
	pinMode( 2, INPUT_PULLUP);
	pinMode( 3, INPUT_PULLUP);
	pinMode( 4, INPUT_PULLUP);
	pinMode( 5, INPUT_PULLUP);
	pinMode( 6, INPUT_PULLUP);
	pinMode( 7, INPUT_PULLUP);
	pinMode( 8, INPUT_PULLUP);
	pinMode( 9, INPUT_PULLUP);
	pinMode(10, INPUT_PULLUP);
	pinMode(11, INPUT_PULLUP);
	pinMode(12, INPUT_PULLUP);
	pinMode(13, INPUT_PULLUP);
 
 
  Serial.begin(115200);       //standard serial
  DDRD = DDRD | B00000000;
  DDRB = DDRD | B00000000;
}

void loop()
{

  la = a;                    //last register a used to detect input change 
  lb = b;                    //last register b used to detect input change

  //--------------------------------------------------------------------
  // Read from input registers
  //--------------------------------------------------------------------
  a = PIND >> 2;             //read digital inputs 2 to 7 really fast
  b = PINB;                  //read digital inputs 8 to 13 really fast
  time = micros();
  if (la!=a || lb!=b)
  {


    //--------------------------------------------------------------------
    // Detect pulses from register A
    //--------------------------------------------------------------------
    p[2].pulse(0,a,time);                //digital input 2
    p[3].pulse(1,a,time);                //    ''        3
    p[4].pulse(2,a,time);                //    ''        etc
    p[5].pulse(3,a,time);
    p[6].pulse(4,a,time);
    p[7].pulse(5,a,time);

    //--------------------------------------------------------------------
    // Detect pulses from register B
    //--------------------------------------------------------------------
    p[8].pulse(0,b,time);                //digital input 8
    p[9].pulse(1,b,time);                //etc
    p[10].pulse(2,b,time);
    p[11].pulse(3,b,time);
    p[12].pulse(4,b,time);
    p[13].pulse(5,b,time);

  }

  //--------------------------------------------------------------------
  // Spit out data every printTime sec (time here is in microseconds)
  //--------------------------------------------------------------------
  // build JSON: for all counters print Count (W), Count Accum(W), Average ms
  // Format {"m":meter,"c":count,"r":rate, "cA":countAccum}
  if ((time-ltime)>(printTime))    
  {
    ltime = time;                          	//Print timer
    
    {
      Serial.print("{\"m\":"); 
      Serial.print(curMeter-1);           	//Print meter number
      Serial.print(",\"c\":"); 
      Serial.print(p[curMeter].count);    	//Print pulse count
      Serial.print(",\"r\":"); 
      Serial.print(p[curMeter].rate(time));	//Print pulse rate
	  p[curMeter].countAccum += p[curMeter].count;	//Increment and print count accumulator to allow for error checking at client side;
	  Serial.print(",\"cA\":"); 
      Serial.print(p[curMeter].countAccum); 
      Serial.println("}");
      p[curMeter].count = 0;                //Reset count (we just send count increment)
      p[curMeter].prateAccum = 0;       	//Reset accum so that we can calculate a new average
    }
	curMeter++ ;							
	if (curMeter > lastMeter){				// wrap a around if passed last meter
		curMeter = 2;} 
  }
}

// library for pulse, originally in separate file 

//-----------------------------------------------------------------------------------
//Gets a particular input state from the register binary value
// A typical register binary may look like this:
// B00100100
// in this case if the right most bit is digital pin 0
// digital 2 and 5 are high
// The method below extracts this from the binary value
//-----------------------------------------------------------------------------------
#define BIT_TST(REG, bit, val)( ( (REG & (1UL << (bit) ) ) == ( (val) << (bit) ) ) )

//-----------------------------------------------------------------------------------
// Method detects a pulse, counts it, finds its rate, Class: PulseOutput
//-----------------------------------------------------------------------------------
boolean PulseOutput::pulse(int pin, int a, unsigned long timeIn)
{
   ld = d;                                    //last digital state = digital state
   
   if (BIT_TST(a,pin,1)) d = 1; else d = 0;   //Get current digital state from pin number
   
   // if (ld==0 && d==1)                      // no internal pull_up if state changed from 0 to 1: internal pull-up inverts state
	if (ld==1 && d==0)                         //pull_up f state changed from 0 to 1: internal pull-up inverts state
   {
     count++;                                 //count the pulse
     
     // Rate calculation
     lastTime = time;           
     time = timeIn ;						// correction to allow for processing
     prate = (time-lastTime);// - 400;          //rate based on last 2 pulses
                                                //-190 is an offset that may not be needed...??
     prateAccum += prate - 2000;                     //accumulate rate for average calculation
     
     return 1;
   }
   return 0;
}


//-----------------------------------------------------------------------------------
// Method calculates the average rate based on multiple pulses (if there are 2 or more pulses)
//-----------------------------------------------------------------------------------
unsigned long PulseOutput::rate(unsigned long timeIn)
{
 if (count > 1)
 {
   prate = prateAccum / count;                          //Calculate average
 } else 
 {
 
 if ((timeIn - lastTime)>(prate*2)) prate = 0;}         //Decrease rate if no pulses are received
                                                        //in the expected time based on the last 
                                                        //pulse width.
 return prate; 
}

 




