/*
 PROJECT: MySensors / Philip HUE bridge
 PROGRAMMER: AWI
 DATE: december 9, 2016/ last update
 FILE: AWI_MyS_HUE_bridge.ino
 LICENSE: Public domain

 Hardware: ESP8266WiFi and MySensors 2.0
		
 Special:
	AWI_Color: color conversion lib (various sources)
	Philips HUE API:	https://developers.meethue.com/
	Fastled library with NeoPixel (great & fast RBG/HSV universal library) 			https://github.com/FastLED/FastLED
	JSON parsing: https://github.com/bblanchon/ArduinoJson
	
 SUMMARY:
	
	Connects Philips HUE bridge (groups) to MySensors
	
	Polls HUE bridge for changes in groups and sends to corresponding MySensors RGB lights
	
 Remarks:
	
 Change log:


/*
MySensors stuff
*/
// Enable debug prints to serial monitor
#define MY_DEBUG
#define MY_GATEWAY_ESP8266
#define MY_ESP8266_SSID "<Your netword>"							// enter your SSID
#define MY_ESP8266_PASSWORD "<Your password>"						// enter your password
#define MY_IP_ADDRESS 192,168,2,122									// enter you fixed IP & gateway
#define MY_IP_GATEWAY_ADDRESS 192,168,2,254							
#define MY_IP_SUBNET_ADDRESS 255,255,255,0
// The port to keep open on node server mode
#define MY_PORT 5003

#define NODE_TXT "AWI_ESP 122"									// Text to add to sensor name
/*
END -  MySensors stuff
*/
#include <ESP8266WiFi.h>
#include <MySensors.h>
#include <ArduinoJson.h>
#include "AWI_Color.h"											// color conversion library, includes FastLED

// helpers
#define LOCAL_DEBUG
#ifdef LOCAL_DEBUG
	#define Sprint(...) (Serial.print( __VA_ARGS__))			// macro's as substitute for print,println 
	#define Sprintln(...) (Serial.println( __VA_ARGS__))
#else
	#define Sprint(...)
	#define Sprintln(...)
#endif

// MySensors const & var
const int RGB_LightChild = 0 ;									// Child Id's, standard light child on/off/ dim
MyMessage lightRGBMsg(0, V_RGB);								// standard messages, light
MyMessage lightdimmerMsG(0 ,V_DIMMER);	
MyMessage lightOnOffMessage(0, V_STATUS);

AWI_Color ColorConv ;											// conversion routines


// Specific HUE settings, adapt to you need (needs some cleanup)
WiFiClient client;
// HUE syntax: http://<host:port>/api/<api_key>/<hueCommand>
const char* server = "<192.168.2.130>";  						// Philips HUE bridge / server's address
const char* resource = "/api/<xxxxxxxxxxxxxx>";  				// long number ..http resource (api) https://developers.meethue.com/
const unsigned long HTTP_TIMEOUT = 10;  						// max respone time from server
//HUE const & var
const uint8_t noHueGroups = 4 ;									// Number of Hue groups used, adapt to your own settings, groups start at 0 in HUE (caution for polling interval)
// const uint8_t hueGroups[noHueGroups] = {0, 1, 2} ;			// need to initialize the Hue groups with the group numbers (tbd for indirect addressing)
const uint16_t MAX_HUE_JSON = 512 ;								// JSON of a HUE group (this should fit the stack, so we need to poll them separate.....)				
const size_t MAX_CONTENT_SIZE = MAX_HUE_JSON + 256 ;			// max size of the HTTP response (JSON + HTTP header)
const unsigned long pollDelay = 250 ;							// time between polls in ms

// A few HUE command strings - These can be copied in the request (sprintf)
const char group_string[] = "/groups/%i" ;
const char group_action_string[] = "/groups/%i/action" ;
const char light_string[] = "/lights/%i" ;

const char set_on_off_string[] = "{\"on\":%s}";
const char set_color_string[] = "{\"hue\":%i,\"sat\":%i}";
//const char set_color_coord_string[] = "{\"xy\": [%d.%03d, %d.%03d], \"bri\": %i}"; // trick to print floats (tbd)
const char set_color_coord_string[] = "{\"xy\": [%s, %s], \"bri\": %i}"; 
const char set_bri_string[] = "{\"bri\":%i}";
const char set_alert_string[] = "{\"alert\":\"%s\"}";


// Struct contains the Hue data for each group to sync with the bridge
struct HueData {
	bool on ;													// true = on ; false = off
	uint8_t  bri ;												// brightness 0..254
	uint16_t hue ;												// hue  0..65535
	uint8_t sat ; 												// saturation 0..254
    float xy[2] ;												// color xy coords 0..1
	uint16_t ct ;												// color temperature (mired 153..500)
	char alert[8] ;												// alert mode (none, select, lselect)
	char colormode[3] ;											// color mode xy (coord space) / ct (color temp)/ hs (hue/sat)
	char name[16] ;												// name of group
};

HueData hueData, lastHueData[noHueGroups] ;						// global storage for hue group data
CRGB convRGB;													// FastLED type for RGB conversion CHSV(hue, sat, bri255)
int groupCounter ; 												// loop counter for group polling in loop

// entry
void setup() {
	groupCounter = 0 ;											// start with first group
}

void presentation(){
// MySensors present hue group names read from HUE bridge to controller
	sendSketchInfo("AWI ESP HUE " NODE_TXT, "2.0"); wait(50) ;
	char tmpCommand[10] = ""; 									// temporary char store 
	for(int i = 0 ; i < noHueGroups ; i++){						// poll groups for data and present to controller
		sprintf(tmpCommand, group_string , i) 		;			// construct command
		if (connect(server)) {
			if (sendRequest(server, resource, tmpCommand) && skipResponseHeaders()) {
				char response[MAX_CONTENT_SIZE];
				readReponseContent(response, sizeof(response));
				if (parseHueData(response, &hueData)){
					wait(5000) ;
					present( i, S_RGB_LIGHT, hueData.name); 	// present the sensor with the hue 
				}
			disconnect();
			}
		}
	}
}

// Loop polls each group, reads content and acts accordingly
void loop() {
	char tmpCommand[20] = ""; 									// temporary char store 
	sprintf(tmpCommand, group_string ,groupCounter) ;			// construct command
	if (connect(server)) {
		if (sendRequest(server, resource, tmpCommand) && skipResponseHeaders()) {
			char response[MAX_CONTENT_SIZE];
			readReponseContent(response, sizeof(response));
			if (parseHueData(response, &hueData)){
				Sprintln(); Sprintln("new HUE data:") ;
				printHueData(hueData) ;
				//Sprintln(); Sprintln("last HUE data:") ;
				//printHueData(lastHueData[groupCounter]) ;
				compareAndSend(groupCounter, hueData);	// check if changes and act
				lastHueData[groupCounter] = hueData ;
			}
		disconnect();
		}
	}
	groupCounter = ++groupCounter % noHueGroups ;				// increment and wrap
	wait(pollDelay);
}

// Open connection to the HTTP server
bool connect(const char* hostName) {
	Sprint("Connect to ");
	Sprintln(hostName);
	bool ok = client.connect(hostName, 80);
	Sprintln(ok ? "Connected" : "Connection Failed!");
	return ok;
}

// Send the HTTP GET request to the server
// after request the (JSON) information is available through "readReponseContent". 
// a JSON object or array can be returned (first char = '[' or '{'), default is Object if no error
bool sendRequest(const char* host, const char* resource, const char* command) {
	Sprint("GET ");	Sprint(resource); Sprintln(command) ;
	// construct HTTP header
	client.print("GET "); client.print(resource); client.println(command) ;
	client.println("HTTP/1.1");
	client.print("Host: "); client.println(host);
	client.println("Connection: close");
	client.println();
	return true ;
}

// Send the HTTP PUT command to the server
// after command the (JSON) information is available through "readReponseContent"
// a JSON array will be returned (first char == '[')
// http://www.esp8266.com/viewtopic.php?f=24&t=3632&sid=12439a0535f00bb1688f986b21d5b7a8&start=4 trick
/*        "PUT /api/username/lights/4/state",
        "HTTP/1.1","Host: " .. hostIP,
        "Connection: close","Accept: text/plain",
        "Content-Type: text/plain;charset=UTF-8",
        "Content-Length: " .. #postData, '', postData
     } ,"\r\n")
*/
bool sendCommand(const char* host, const char* resource, const char* command, const char* commandJSON) {
	Sprint("PUT "); Sprintln(resource); Sprintln(command) ;
	Sprintln("HTTP/1.1");
	Sprint("Host: ");	Sprintln(host); 
	Sprintln("Connection: close");
	Sprintln("Accept: text/plain");
	Sprintln("Content-Type: text/plain;charset=UTF-8");
	Sprint("Content-Length: ") ; Sprintln(strlen(commandJSON));
	Sprintln();
	Sprintln(commandJSON) ;
	// construct HTTP header
	client.print("PUT "); client.print(resource); client.println(command) ;
	client.println("HTTP/1.1");
	client.print("Host: ");	client.println(host);
	client.println("Connection: close");
	client.println("Accept: text/plain");
	client.println("Content-Type: text/plain;charset=UTF-8");
	client.print("Content-Length: ") ; client.println(strlen(commandJSON));
	client.println();
	client.println(commandJSON) ;
	client.println();
	/*
	// can be used to get response from sendCommand, not used here
	if (sendCommand(server, resource, "/groups/1/action", turn_on_test) && skipResponseHeaders()) {
		char response[MAX_CONTENT_SIZE];
		readReponseContent(response, sizeof(response));
		if (parseUserDataArray(response, &hueData)){
		}
	}

	wait(5000) ;
	*/
}

// Skip HTTP headers so that we are at the beginning of the response's body
bool skipResponseHeaders() {
	// HTTP headers end with an empty line
	char endOfHeaders[] = "\r\n\r\n";
	client.setTimeout(HTTP_TIMEOUT);
	bool ok = client.find(endOfHeaders);
	if (!ok) {
		Sprintln("No response or invalid response!");
	}
	return ok;
}

// Read the body of the response from the HTTP server
void readReponseContent(char* content, size_t maxSize) {
  size_t length = client.readBytes(content, maxSize);
  content[length] = 0;
  Sprintln(content);
}


// Parse the JSON string containing the HUE values, should be a JSON object '{'
bool parseHueData(char* content, struct HueData* hueData) { 
	char tmpKey[10] = ""; 														// temporary key store (only action of state)
	const size_t JSON_BUFFER_SIZE = MAX_CONTENT_SIZE ; 	// for now
	StaticJsonBuffer<JSON_BUFFER_SIZE> jsonBuffer;
	JsonObject& root = jsonBuffer.parseObject(content);							// assumes object starting with "{"
	if (!root.success()) {
		Sprintln("JSON parsing failed!");
		return false;
	}
	// check for all values and copy to HUE data structure
	if (containsNestedKey(root, "on")){
		hueData->on = root["action"]["on"] ; Sprintln("contains key on");
	}
	if (containsNestedKey(root, "bri")){
		hueData->bri = root["action"]["bri"] ; Sprintln("contains key bri");
	}
	if (containsNestedKey(root, "hue")){
		hueData->hue = root["action"]["hue"] ; Sprintln("contains key hue");
	}
	if (containsNestedKey(root, "sat")){
		hueData->sat = root["action"]["sat"] ; Sprintln("contains key sat");
	}
	if (containsNestedKey(root, "xy")){
		hueData->xy[0] = root["action"]["xy"][0] ; 
		hueData->xy[1] = root["action"]["xy"][1] ; 
		Sprintln("contains key xy");
	}
	if (containsNestedKey(root, "ct")){
		hueData->ct = root["action"]["ct"] ; Sprintln("contains key ct");
	}
	if (containsNestedKey(root, "alert")){
		strcpy(hueData->alert, root["action"]["alert"]) ; Sprintln("contains key alert");
	}
	if (containsNestedKey(root, "colormode")){
		strcpy(hueData->colormode, root["action"]["colormode"]) ; Sprintln("contains key colormode");
	}
	if (containsNestedKey(root, "name")){
		strcpy(hueData->name, root["name"]) ; Sprintln("contains name");
	}
	// printHueData(hueData) ;
	return true;
}

// Parse the JSON string containing other values returned, should be a JSON array '['
bool parseUserDataArray(char* content, struct HueData* hueData){
	const size_t JSON_BUFFER_SIZE = MAX_CONTENT_SIZE ; 	// for now
	StaticJsonBuffer<JSON_BUFFER_SIZE> jsonBuffer;
	JsonArray& root = jsonBuffer.parseArray(content);
	if (!root.success()) {
		Sprintln("JSON parsing failed!");
		return false;
	}
	// Here were copy the strings we're interested in
	Sprint("Return JSON:  ") ;
	root[0].prettyPrintTo(Serial) ;
	Sprintln();
	return true;
}

// Print the HUE data extracted from the JSON
void printHueData(const struct HueData& hueData) {
	Sprint("on:\t"); Sprintln( hueData.on?"true":"false") ;					// true = on ; false = off
	Sprint("bri:\t") ; Sprintln( hueData.bri);									// brightness 0..254
	Sprint("hue:\t") ; Sprintln( hueData.hue);									// hue  0..65535
	Sprint("sat:\t") ; Sprintln( hueData.sat) ; 								// saturation 0..254
    Sprint("x:\t") ; Sprint( hueData.xy[0]) ;									// color xy coords 0..1
	Sprint("\t y:\t") ; Sprintln( hueData.xy[1]) ;
	Sprint("ct:\t") ; Sprintln( hueData.ct) ;									// color temperature (mired 153..500)
	Sprint("alert:\t") ; Sprintln( hueData.alert) ;							// alert mode (none, select, lselect)
	Sprint("color m:\t") ; Sprintln( hueData.colormode);						// color mode xy (coord space) / ct (color temp)/ hs (hue/sat)
	Sprint("name:\t") ; Sprintln( hueData.name);								// color mode xy (coord space) / ct (color temp)/ hs (hue/sat)
}


// Close the connection with the HTTP server
void disconnect() {
	Sprintln("Disconnect");
	client.stop();
}

// Incoming messages from MySensors
void receive(const MyMessage &message) {
	char tmpCommand[30] = ""; 													// temporary string store
	char tmpCommandJson[40] = "";
	int ID = message.sensor;
	sprintf(tmpCommand, group_action_string , ID ) ;							// construct command /groups/%i/action
	Sprint("Sensor: "); Sprintln(ID);
	if(message.type == V_STATUS){												// if on/off type, toggle 
		sprintf(tmpCommandJson, set_on_off_string, message.getInt()==0?"false":"true" ) ;		// construct command
		Sprint("tmpCommandJson") ; Sprintln(tmpCommandJson) ;
	} else if (message.type == V_PERCENTAGE){
		sprintf(tmpCommandJson, set_bri_string, map(message.getInt(),0,100,0,254 )) ;									// construct command
	} else if (message.type == V_RGB){											// contained in char array
		CRGB tmpRGB = strtol( message.getString(), NULL, 16) ;
		double cx, cy, bri ;													// send color in xy space. (be aware: tbd, ambient lights only accept color temp in mired)
		ColorConv.getXYfromRGB(cx, cy, bri, tmpRGB ) ;							// set color coordinates for HUE
		char xTmp[6], yTmp[6] ;													// temp storage and float to char conversion
		dtostrf(cx, 5, 3, xTmp) ;
		dtostrf(cy, 5, 3, yTmp) ;
		sprintf(tmpCommandJson, set_color_coord_string, xTmp, yTmp,
				map(int(bri*1000.0), 0, 1000, 0, 254)) ; 						// brightness from 0..1 to 0.254
//		sprintf(tmpCommandJson, set_color_coord_string, (int)cx, (int)(cx*1000)%1000, // construct command (trick to print float)
//														(int)cy, (int)(cy*1000)%1000,
//														map(int(bri*1000.0), 0, 1000, 0, 254)) ; // brightness from 0..1 to 0.254
		Sprint("constructed color command:  ") ; Sprintln(tmpCommandJson) ;
	}	
	if (connect(server)) {
		sendCommand(server, resource, tmpCommand, tmpCommandJson) ;
		disconnect();
	}
}


// compare huedata with last huedata and act on changes
// takes global lastHueData and input
void compareAndSend(int currentGroup, struct HueData hueData ){					// check if changes and act
	if (lastHueData[currentGroup].on != hueData.on){							// something changed, so need to update sensor
		// change on/ off
		send(lightOnOffMessage.setSensor(currentGroup).set(hueData.on?1:0)) ; 
		Sprint("on/off changed to: "); Sprintln( hueData.on?"true":"false") ;
	}
	if (lastHueData[currentGroup].bri != hueData.bri){
		// change brightness
		send(lightdimmerMsG.setSensor(currentGroup).set((int)map(hueData.bri, 0, 255, 0, 100))) ; 
		Sprint("brightness changed to"); Sprintln( (int)map(hueData.bri, 0, 255, 0, 100)) ;
	}
	bool sendRGBflag = false ;	
	if (strcmp(hueData.colormode, "xy" ) == 0){ // xy, look if any change
		Sprint("xy x: "); Sprint( hueData.xy[0]) ; Sprint("  y: "); Sprintln( hueData.xy[1]) ;
		if ((fabs(lastHueData[currentGroup].xy[0] - hueData.xy[0]) > 0.001) || (fabs(lastHueData[currentGroup].xy[1] - hueData.xy[1]) > 0.001)){	// precedence for xy this will also reflect changes in HSV
			ColorConv.getRGBfromXY(convRGB, hueData.xy[0], hueData.xy[1], hueData.bri) ;
			sendRGBflag = true ;
		}
	} else if (strcmp(hueData.colormode, "hv" ) == 0){ // hsv look if any change
		Sprint("hv hue: "); Sprint( hueData.hue) ; Sprint("  sat: "); Sprintln( hueData.sat) ;
		if ((lastHueData[currentGroup].hue != hueData.hue) || (lastHueData[currentGroup].sat != hueData.sat)){
			CHSV tmpHSV ;
			tmpHSV.h = hueData.hue / 255 ; 										// hue in HUE is 16bit
			tmpHSV.s = hueData.sat ;
			tmpHSV.v = hueData.bri ;
			ColorConv.getRGBfromHSV(convRGB, tmpHSV ) ;
			sendRGBflag = true ;
		}
	} else if (strcmp(hueData.colormode, "ct" ) == 0){ // color temperature look if any change
		Sprint("ct: "); Sprintln( hueData.ct) ;
		if (lastHueData[currentGroup].ct != hueData.ct){						// change in color temperature needs to be handled separately (HUE design)
			ColorConv.getRGBfromTemperature(convRGB, hueData.ct ) ;
			sendRGBflag = true ;
		}
	}
	if (sendRGBflag){															// send the color commands
		char tempChar[8] ;														// temporary store for alphanumeric RGB (hex)
		sprintf(tempChar, "%02x%02x%02x", convRGB.r, convRGB.g, convRGB.b) ;
		Sprint("converted RGB: ") ; Sprintln(tempChar) ;
		send(lightRGBMsg.setSensor(currentGroup).set(tempChar)) ; 
	}
}

// helper to check for nested keys in JSON object
bool containsNestedKey(const JsonObject& obj, const char* key) {
    for (const JsonPair& pair : obj) {
        if (!strcmp(pair.key, key))
            return true;

        if (containsNestedKey(pair.value.as<JsonObject>(), key)) 
            return true;
    }
    return false;
}
