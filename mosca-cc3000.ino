/***************************************************
  Adafruit MQTT Library CC3000 Example

  Designed specifically to work with the Adafruit WiFi products:
  ----> https://www.adafruit.com/products/1469

  Adafruit invests time and resources providing this open source code,
  please support Adafruit and open-source hardware by purchasing
  products from Adafruit!

  Written by Limor Fried/Ladyada for Adafruit Industries.
  MIT license, all text above must be included in any redistribution
 ****************************************************/
#include <Adafruit_SleepyDog.h>
#include <Adafruit_CC3000.h>
#include <SPI.h>
#include "utility/debug.h"
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_CC3000.h"

/*************************** CC3000 Pins ***********************************/
#define ADAFRUIT_CC3000_IRQ   3  // MUST be an interrupt pin!
#define ADAFRUIT_CC3000_VBAT  5  // VBAT & CS can be any digital pins.
#define ADAFRUIT_CC3000_CS    10
// Use hardware SPI for the remaining pins
// On an UNO, SCK = 13, MISO = 12, and MOSI = 11

/************************* WiFi Access Point *********************************/

#define WLAN_SSID       "INFINITUM0004"  // can't be longer than 32 characters!
#define WLAN_PASS       ""
#define WLAN_SECURITY   WLAN_SEC_WPA2  // Can be: WLAN_SEC_UNSEC, WLAN_SEC_WEP,
                                       //         WLAN_SEC_WPA or WLAN_SEC_WPA2

/************************* Adafruit.io Setup *********************************/

#define AIO_SERVER      ""
#define AIO_SERVERPORT  1883
#define AIO_USERNAME    "josleugim"
#define AIO_KEY         ""

/************ *********/
#define LAMP_ONE       8  //   
#define SWITCH_ONE     9 // switch for the lamp 1
#define LAMP_TWO       6 // input for the switch
#define SWITCH_TWO     7 // switch fot the lamp 2
//#define LAMP_THREE     2 // input for the switch
//#define SWITCH_THREE   4 // switch fot the lamp 2

int lastButtonStateOne;       
int actualButtonStateOne;
int lastButtonStateTwo;       
int actualButtonStateTwo;
//int lastButtonStateThree;
//int actualButtonStateThree;

/************ Global State (you don't need to change this!) ******************/

// Setup the main CC3000 class, just like a normal CC3000 sketch.
Adafruit_CC3000 cc3000 = Adafruit_CC3000(ADAFRUIT_CC3000_CS, ADAFRUIT_CC3000_IRQ, ADAFRUIT_CC3000_VBAT);

// Store the MQTT server, username, and password in flash memory.
// This is required for using the Adafruit MQTT library.
const char MQTT_CLIENTID[] PROGMEM  = __TIME__ AIO_USERNAME;
const char MQTT_SERVER[] PROGMEM    = AIO_SERVER;
const char MQTT_USERNAME[] PROGMEM  = AIO_USERNAME;
const char MQTT_PASSWORD[] PROGMEM  = AIO_KEY;

// Setup the CC3000 MQTT class by passing in the CC3000 class and MQTT server and login details.
Adafruit_MQTT_CC3000 mqtt(&cc3000, MQTT_SERVER, AIO_SERVERPORT, MQTT_CLIENTID, MQTT_USERNAME, MQTT_PASSWORD);

// You don't need to change anything below this line!
#define halt(s) { Serial.println(F( s )); while(1);  }

// CC3000connect is a helper function that sets up the CC3000 and connects to
// the WiFi network. See the cc3000helper.cpp tab above for the source!
boolean CC3000connect(const char* wlan_ssid, const char* wlan_pass, uint8_t wlan_security);

/****************************** Feeds ***************************************/

// Setup a feed called 'photocell' for publishing.
// Notice MQTT paths for AIO follow the form: <username>/feeds/<feedname>
const char STREETLAMP1STATE[] PROGMEM = AIO_USERNAME "/groundfloor/frontyard/lamp1state";
Adafruit_MQTT_Publish lamp1State = Adafruit_MQTT_Publish(&mqtt, STREETLAMP1STATE);

const char STREETLAMP1[] PROGMEM = AIO_USERNAME "/groundfloor/frontyard/lamp1";
Adafruit_MQTT_Subscribe lamp1 = Adafruit_MQTT_Subscribe(&mqtt, STREETLAMP1);

const char STREETLAMP2STATE[] PROGMEM = AIO_USERNAME "/groundfloor/frontyard/lamp2state";
Adafruit_MQTT_Publish lamp2State = Adafruit_MQTT_Publish(&mqtt, STREETLAMP2STATE);

const char STREETLAMP2[] PROGMEM = AIO_USERNAME "/groundfloor/frontyard/lamp2";
Adafruit_MQTT_Subscribe lamp2 = Adafruit_MQTT_Subscribe(&mqtt, STREETLAMP2);

/*const char LAMP3STATE[] PROGMEM = AIO_USERNAME "/groundfloor/frontyard/lamp3state";
Adafruit_MQTT_Publish lamp3State = Adafruit_MQTT_Publish(&mqtt, LAMP3STATE);

const char STREETLAMP3[] PROGMEM = AIO_USERNAME "/groundfloor/frontyard/lamp3";
Adafruit_MQTT_Subscribe lamp3 = Adafruit_MQTT_Subscribe(&mqtt, STREETLAMP3);*/

/*************************** Sketch Code ************************************/

void setup() {
  Serial.begin(115200);

  // define the pin modes
  pinMode(LAMP_ONE, OUTPUT);
  pinMode(SWITCH_ONE, INPUT);
  pinMode(LAMP_TWO, OUTPUT);
  pinMode(SWITCH_TWO, INPUT);
  /*pinMode(LAMP_THREE, OUTPUT);
  pinMode(SWITCH_THREE, INPUT);*/

  Serial.println(F("\nAdafruit MQTT"));
  Serial.print(F("\nFree RAM: ")); Serial.println(getFreeRam(), DEC);

  // initialize the lamps
  digitalWrite(LAMP_ONE, LOW);
  digitalWrite(LAMP_TWO, LOW);
  //digitalWrite(LAMP_THREE, LOW);

  // Initialise the CC3000 module
  Serial.print(F("\nInit the CC3000..."));
  if (!cc3000.begin())
      halt("Failed");

  // do the subscribe
  mqtt.subscribe(&lamp1);
  mqtt.subscribe(&lamp2);
  //mqtt.subscribe(&lamp3);
  
  // attempt wifi connection
  while (! CC3000connect(WLAN_SSID, WLAN_PASS, WLAN_SECURITY)) {
    Serial.println(F("Retrying WiFi"));
    while(1);
  }
}

void loop() {
  // Make sure to reset watchdog every loop iteration!
  Watchdog.reset();

  // Ensure the connection to the MQTT server is alive (this will make the first
  // connection and automatically reconnect when disconnected).  See the MQTT_connect
  // function definition further below.
  MQTT_connect();
  
  // Wait for the push button
  StreetLampOne(digitalRead(SWITCH_ONE));
  StreetLampTwo(digitalRead(SWITCH_TWO));
  //LampThree(digitalRead(SWITCH_THREE));

  // this is our 'wait for incoming subscription packets' busy subloop
  Adafruit_MQTT_Subscribe *subscription;
  while ((subscription = mqtt.readSubscription(500))) {
    if(subscription == &lamp1) {
      char *value = (char*)lamp1.lastread;
      StreetLampOne(atoi(value));
    }
    if(subscription == &lamp2) {
      char *value = (char*)lamp2.lastread;
      StreetLampTwo(atoi(value));
    }
    /*if(subscription == &lamp3) {
      char *value = (char*)lamp3.lastread;
      LampThree(atoi(value));
    }*/
  }

  // ping the server to keep the mqtt connection alive
  if(! mqtt.ping()) {
    Serial.println(F("\nMQTT Ping failed."));
  }
}

// Function to connect and reconnect as necessary to the MQTT server.
// Should be called in the loop function and it will take care if connecting.
void MQTT_connect() {
  int8_t ret;

  // Stop if already connected.
  if (mqtt.connected()) {
    return;
  }

  Serial.print("\nConnecting to MQTT... ");

  while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
       Serial.println(mqtt.connectErrorString(ret));
       if (ret < 0)
            CC3000connect(WLAN_SSID, WLAN_PASS, WLAN_SECURITY);  // y0w, lets connect to wifi again
       Serial.println("Retrying MQTT connection in 5 seconds...");
       mqtt.disconnect();
       delay(5000);  // wait 5 seconds
  }
  Serial.println("MQTT Connected!");
}

void StreetLampOne(int lastSwitchState) {
  if(lastSwitchState == HIGH) {
    Serial.print("\nSwitch one pressed: ");
    // Cambiar el estado del foco
      if(digitalRead(LAMP_ONE) == 1) {
        digitalWrite(LAMP_ONE, LOW);
        lamp1State.publish("0");
      }
      else {
        digitalWrite(LAMP_ONE, HIGH);
        lamp1State.publish("1");
      }
  }
}

void StreetLampTwo(int lastSwitchState) {
  lastButtonStateTwo = lastSwitchState;
  if (lastButtonStateTwo != actualButtonStateTwo) {    // Valor del switch ha cambiado!
    if (lastButtonStateTwo == HIGH) {               // Verificar si el boton se ha presionado
      // Cambiar el estado del foco
      if(digitalRead(LAMP_TWO) == 1) {
        digitalWrite(LAMP_TWO, LOW);
        lamp2State.publish("0");
      }
      else {
        digitalWrite(LAMP_TWO, HIGH);
        lamp2State.publish("1");
      }
    }
  }
  actualButtonStateTwo = lastButtonStateTwo;
}

/*void LampThree(int lastSwitchState) {
  lastButtonStateThree = lastSwitchState;
  if (lastButtonStateThree != actualButtonStateThree) {    // Valor del switch ha cambiado!
    if (lastButtonStateThree == HIGH) {               // Verificar si el boton se ha presionado
      // Cambiar el estado del foco
      if(digitalRead(LAMP_THREE) == 1) {
        digitalWrite(LAMP_THREE, LOW);
        lamp3State.publish("0");
      }
      else {
        digitalWrite(LAMP_THREE, HIGH);
        lamp3State.publish("1");
      }
    }
  }
  actualButtonStateThree = lastButtonStateThree;
}*/
