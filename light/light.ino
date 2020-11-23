#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

/**
 * 
 */

/* PIN definition */
const int ledPIN   = D1; // PWM for LED
const int swPIN    = D8; // Reading state for switch
const int lamp1PIN = D2; // Lamp 1
const int lamp2PIN = D3; // Lamp 2
const int lamp3PIN = D4; // Lamp 3
const int irPIN    = D5; // Reading PIR sensor (movement)
const int tempPIN  = D6; // Reading I2C floor temperature

/* Used in timers */
unsigned long currentTime = millis();

/* State settings */
int stateMain = 0;  
// 0 - darkness, 
// 1 - night mode, somebody enter without clicking switch - 1% light, timeout or switch off
// 2 - light mode, somebody click switch - 100% light, switch off

const int nightTimeout = 1000 * 60 * 5; // 5 min
unsigned long nightPreviousTime = 0; // For calculate timeout for night mode


/* PWM LED settings */
const int freq = 200;
const int pwmResolution = 200;
int ledValue = 0;

int ledUp = 0; // Going up or down with brightness
unsigned long ledPreviousTime = 0;

const unsigned long stepTimeNormalMode = 15;
const unsigned long stepTimeNightMode = 60;
unsigned long ledStepTime = stepTimeNormalMode;

const int pwmNightModeMax = pwmResolution * 0.02;
int pwmMax = pwmResolution; // Maximum brightness in normal mode


/* PIR sensor settings */
int pirState = LOW;
int pirPreviousState = LOW; // for logic

/* Switch sensor settings */
int switchState = 0; // for detecting changes
int switchPreviousState = 0; //  for logic
int switchFlag = 0; // for logic - set to one only once
unsigned long switchPreviousTime = 0; 
unsigned long switchTimeout = 100; // changes are possible only once per 100ms


/* Temperature sensor DS18B20 */
// Zasilanie VCC - czerwony
// Masa GND - czarny lub zielony
// Dane 1-wire - niebieskilub żółty
//OneWire oneWire(tempPIN);
//DallasTemperature ds18(&oneWire);

/* Put your WiFi SSID & Password */
const char* ssid = "www.lac.gda.pl";  // Enter SSID here
const char* password = "0585338376";  //Enter Password here

/* Server settings */
ESP8266WebServer server(80);


void setup() {
  Serial.begin(115200);
  Serial.println("Start");

  pinMode(ledPIN, OUTPUT);
  pinMode(swPIN, INPUT);
  pinMode(lamp1PIN, OUTPUT);
  pinMode(lamp2PIN, OUTPUT);
  pinMode(lamp3PIN, OUTPUT);
  pinMode(irPIN, INPUT);

  analogWriteRange(pwmResolution);  // 0..8 - new range
  analogWriteFreq(freq);  // Default 1kHz
  analogWrite(ledPIN, ledValue); // 0 .. 1023 - default range

  Serial.println("Start - WiFi");
  //connect to your local wi-fi network
  WiFi.begin(ssid, password);
  //check wi-fi is connected to wi-fi network
  while (WiFi.status() != WL_CONNECTED) {
    Serial.println("Connecting Wifi..");
    delay(500);
  }
  Serial.println("");
  Serial.println("WiFi connected..!");
  Serial.print("Got IP: ");  
  Serial.println(WiFi.localIP());

  setupTemp();

  setupServer();
}


void setupTemp() {
  // TODO: Setup DS18B20
}

void setupServer() {
  server.on("/", handle_OnConnect);
  server.onNotFound(handle_NotFound);
  server.begin();  
}

void loop() {
  currentTime = millis();

  // Process PIR sensor
  pirHandle();

  // Process switch sensor
  switchHandle();

  // Process PWM LED
  ledLogic();
  ledHandle();

  // Process Main Lamps
  lampHandle();

  // Process all sensor flags, set mainState
  stateLogic();

  server.handleClient();  
}

/**
 * 
 * HTTP Server section
 * 
 */
void handle_OnConnect() {
  server.send(200, "application/json", SendHTML());
}

void handle_NotFound(){
  server.send(404, "application/json", "Not found");
}

String SendHTML(){
  String ptr = "";
  ptr +="{\n";
  ptr +="\"status\":\"OK\"\n";
  ptr +="\n}";
  return ptr;
}

/**
 *  PWM LED Handler
 */
void ledHandle() {
  if (
    ( ledUp == 1 )&&
    ( ledValue < pwmMax )&&
    ( (currentTime - ledPreviousTime) >= ledStepTime  )
  ) {
    ledPreviousTime = currentTime;
    ledValue++;
    analogWrite(ledPIN, ledValue);
    if ( ledValue >= pwmMax ) {
      Serial.println("LED FULL ON");
      Serial.println(ledValue);
    }
  }

  if (
    ( ledUp == 0 )&&
    ( ledValue > 0 )&&
    ( (currentTime - ledPreviousTime) >= ledStepTime  )
  ) {
    ledPreviousTime = currentTime;
    ledValue--;
    analogWrite(ledPIN, ledValue);
    if ( ledValue == 0 ) {
      Serial.println("LED FULL OFF");
    }
  }

}

/** 
 *  PWM LED Logic
 */
void ledLogic() {

  // Slow in night mode, fast in dark or light mode
  if (stateMain == 1) { // night mode
    ledStepTime = 100;
    pwmMax = pwmNightModeMax;
  } else {
    ledStepTime = 10;
    pwmMax = pwmResolution;
  }
  
  // Night mode or Light mode, but leds are not switched on yet
  if ( (stateMain != 0)&&(ledUp != 1) ) {
    ledPreviousTime = currentTime;
    ledUp = 1;
  }

  // Dark mode set, but leds are not switched off yet
  if ( (stateMain == 0)&&(ledUp != 0) ) {
    ledPreviousTime = currentTime;
    ledUp = 0;
  }
}

/**
 * Handle switching on and off main lamps
 */
void lampHandle() {
  if (stateMain == 0) { // Dark Mode
    digitalWrite(lamp1PIN, LOW);
    digitalWrite(lamp2PIN, LOW);
    digitalWrite(lamp3PIN, LOW);
  }
  if (stateMain == 1) {  // Night Mode - only LEDS
    digitalWrite(lamp1PIN, LOW);
    digitalWrite(lamp2PIN, LOW);
    digitalWrite(lamp3PIN, LOW);
  }
  if (stateMain == 2) {  // Light mode - all lamps
    digitalWrite(lamp1PIN, HIGH);
    digitalWrite(lamp2PIN, HIGH);
    digitalWrite(lamp3PIN, HIGH);
  }
}

/**
 * Handle PIR state machine
 */
void pirHandle() {
  int newPirState = digitalRead(irPIN);
  if ( pirState != newPirState ) {
    pirState = newPirState;
    if (pirState == HIGH) {
      Serial.println("PIR ON");
    } else {
      Serial.println("PIR OFF");
      pirPreviousState = LOW; // Reset previous state, trigger only up state
    }
  }
}

/**
 * Handle switch state machine with little logic
 */
void switchHandle() {
  int newSwitchState = digitalRead(swPIN);
  if (
      ( newSwitchState != switchState ) && 
      ( (currentTime - switchPreviousTime) >= switchTimeout  )
     ) {
    switchState = newSwitchState;
    switchPreviousTime = currentTime;
    if (switchState == HIGH) {
      Serial.println("Switch ON");
    } else {
      switchPreviousState = 0;  // Reset previous state, trigger only up state
      Serial.println("Switch OFF");
    }
  }
}

/**
 * Main State Logic
 * setting state as connection with sensors states
 */
void stateLogic() {
  // Light mode on from Dark mode, switch goes ON
  if (
      ( stateMain  == 0 ) &&
      ( switchState == 1 ) && 
      ( switchPreviousState == 0 )
    ) {
    stateMain = 2; // Light mode
    switchPreviousState = 1;
  }

  // Night mode on from Dark mode, IR goes ON, switch is off
  if (
    ( stateMain == 0 ) &&
    ( switchState == 0) &&
    ( pirState == HIGH ) &&
    ( pirPreviousState == LOW )
  ) {
    stateMain = 1;
    pirPreviousState = HIGH;
    nightPreviousTime = currentTime;
  }

  // Light mode off, Night mode off
  if ( 
    ( stateMain != 0 ) && 
    ( switchState == 1 ) && 
    ( switchPreviousState == 0 )
    ) {
      stateMain = 0;
      switchPreviousState = 1;
    }

  // Prolong delay after each pir trigger
  if (
    ( stateMain == 1 ) && // Night mode
    ( pirState == HIGH ) && 
    ( pirPreviousState == LOW )
    ) {
      nightPreviousTime = currentTime;
      pirPreviousState = HIGH;
  }

  // After timeout on night mode switch it off
  if (
    ( stateMain ==  1 ) && // night Mode
    ( ( currentTime - nightPreviousTime ) >= nightPreviousTime ) // Timeout occured
    ) {
      stateMain = 0;
  }
  

}
