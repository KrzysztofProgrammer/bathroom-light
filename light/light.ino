#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

/**

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

/* Lamp settings */
int lampUp1 = LOW;
int lampUp2 = LOW;
int lampUp3 = LOW;
int lampSwitchingOn = 0; // Progress switching on lamps 0 off, 1 - switching on in progress, 2 - switched on
unsigned long lampPreviousTime = 0;
const unsigned long lampDelayOnTime = 1 * 1000; // 2 sec
const unsigned long lampStepTime = 400; // [ms] switch lamp one by one

/* PWM LED settings */
const int freq = 200; //Hz PWM frequency is in the range of 1 – 1000Khz.
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
unsigned long switchTimeout = 200; // changes are possible only once per 1500ms (ok)


/* Temperature sensor DS18B20 */
// Zasilanie VCC - czerwony
// Masa GND - czarny lub zielony
// Dane 1-wire - niebieskilub żółty
//OneWire oneWire(tempPIN);
//DallasTemperature ds18(&oneWire);

/* Put your WiFi SSID & Password */
const char* WIFI_ssid = "IoT";  // Enter SSID here
const char* WIFI_password = "ZqFWBlZN6Vg5q52cLHPw";  //Enter Password here
const char* WIFI_hostname = "bathroom";

/* Server settings */
ESP8266WebServer server(80);

String WebLog;

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
  wifi_station_set_hostname(WIFI_hostname);
  //WiFi.hostname(WIFI_hostname);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_ssid, WIFI_password);


  //check wi-fi is connected to wi-fi network
  while (WiFi.status() != WL_CONNECTED) {
    Serial.println("Connecting Wifi..");
    delay(250);
    digitalWrite(lamp1PIN, HIGH);
    delay(250);
    digitalWrite(lamp1PIN, LOW);
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
  lampLogic();
  lampHandle();

  // Process all sensor flags, set mainState
  stateLogic();

  server.handleClient();
}

/**

   HTTP Server section

*/
void setupServer() {
  server.on("/", handle_OnConnect);
  server.on("/reset", handle_OnReset);
  server.on("/off",handle_Off);
  server.onNotFound(handle_NotFound);
  server.begin();
}

void handle_OnConnect() {
  server.send(200, "application/json", WebLog);
}

void handle_OnReset() {
  WebLog = "";
  server.send(200, "application/json", SendHTML());
}

void handle_NotFound() {
  server.send(404, "application/json", "Not found");
}

void handle_Off() {
  stateMain = 0;
  ledValue = 0;
  analogWrite(ledPIN, ledValue);
  server.send(200, "application/json", SendHTML());
}

String SendHTML() {
  String ptr = "";
  ptr += "{\n";
  ptr += "\"status\":\"OK\"";
  ptr += "\n}";
  return ptr;
}

/**
    PWM LED Handler
*/
void ledHandle() {
  if (
    ( ledUp == 1 ) &&
    ( ledValue < pwmMax ) &&
    ( (currentTime - ledPreviousTime) >= ledStepTime  )
  ) {
    ledPreviousTime = currentTime;
    ledValue++;
    analogWrite(ledPIN, ledValue);
    if ( ledValue >= pwmMax ) {
      WebLog += " LED-FULL-ON ";
      Serial.println("LED-FULL-ON");
      Serial.println(ledValue);
    }
  }

  if (
    ( ledUp == 0 ) &&
    ( ledValue > 0 ) &&
    ( (currentTime - ledPreviousTime) >= ledStepTime  )
  ) {
    ledPreviousTime = currentTime;
    ledValue--;
    analogWrite(ledPIN, ledValue);
    if ( ledValue == 0 ) {
      WebLog += " LED-FULL-OFF ";
      Serial.println("LED FULL OFF");
    }
  }

}

/**
    PWM LED Logic
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
  if ( (stateMain != 0) && (ledUp != 1) ) {
    ledPreviousTime = currentTime;
    ledUp = 1;
  }

  // Dark mode set, but leds are not switched off yet
  if ( (stateMain == 0) && (ledUp != 0) ) {
    ledPreviousTime = currentTime;
    ledUp = 0;
  }
}

/**
   Handle switching on and off main lamps
*/
void lampLogic() {

  if (stateMain == 0) { // Dark Mode
    lampSwitchingOn = 0;
    lampUp1 = LOW;
    lampUp2 = LOW;
    lampUp3 = LOW;
    return;
  }

  if (stateMain == 1) {  // Night Mode - only LEDS
    lampSwitchingOn = 0;
    lampUp1 = LOW;
    lampUp2 = LOW;
    lampUp3 = LOW;
    return;
  }

  if (stateMain == 2) {  // Light mode - all lamps on one by one
    if ( lampSwitchingOn == 2 ) { // Already on
      return;
    }
    
    if ( lampSwitchingOn == 0 ) {
      lampSwitchingOn = 1;
      lampPreviousTime = currentTime;
      return;
    }

    // lampSwitchingOn == 1

    // Take delay before switching on
    if ( (currentTime - lampPreviousTime) <= lampDelayOnTime  ) {
      return;
    }

    // After delay - switch on first lamp
    lampUp1 = HIGH;

    // After next small delay - switch on second lamp
    if ( (currentTime - lampPreviousTime - lampDelayOnTime) >= lampStepTime  ) {
      lampUp2 = HIGH;
    }
    
    // After next small delay - switch on second lamp
    if ( (currentTime - lampPreviousTime - lampDelayOnTime) >= ( 2 * lampStepTime ) ) {
      lampUp3 = HIGH;
      lampSwitchingOn = 2;
    }

  }
}

void lampHandle() {
  digitalWrite(lamp1PIN, lampUp1);
  digitalWrite(lamp2PIN, lampUp2);
  digitalWrite(lamp3PIN, lampUp3);
}

/**
   Handle PIR state machine
*/
void pirHandle() {
  int newPirState = digitalRead(irPIN);
  if ( pirState != newPirState ) {
    pirState = newPirState;
    if (pirState == HIGH) {
      WebLog += " PIR-ON ";
      Serial.println("PIR ON");
    } else {
      WebLog += " PIR-OFF ";
      Serial.println("PIR OFF");
      pirPreviousState = LOW; // Reset previous state, trigger only up state
    }
  }
}

/**
   Handle switch state machine with little logic
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
      WebLog += " SW-ON ";
      Serial.println("Switch ON");
    } else {
      switchPreviousState = 0;  // Reset previous state, trigger only up state
      WebLog += " SW-OFF ";
      Serial.println("Switch OFF");
    }
  }
}

/**
   Main State Logic
   setting state as connection with sensors states
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
    ( ( currentTime - nightPreviousTime ) >= nightTimeout ) // Timeout occured
  ) {
    stateMain = 0;
  }


}
