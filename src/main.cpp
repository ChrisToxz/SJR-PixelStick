#include "pixelstick.h"

void initConfig();
void initLeds();
void initWifi();
// void checkWifi();
void initWebserver();
void serviceClient();
void initWebSocket();
void serviceSocket();
void initOTA();
void serviceOTA();
void serviceLeds();
void checkBattery();
void checkSwitch();

void setup()
{
  Serial.begin(115200);
  while (!Serial) // wait for serial port to connect.
    ;
  // delay(3000);
  // Serial.setDebugOutput(true); // Get debug info from wifi library (also enables printf())
  Serial.println(F("Starting setup"));
  pinMode(USER_SWITCH, INPUT_PULLUP);
  LittleFS.begin();
  initConfig();    // Get the config data
  initLeds();      // Set the LEDs up and tell the user we're alive
  initWifi();      // Start the WiFi - default is AP mode, press user switch during boot for client mode
  initOTA();       // Set up OTA
  initWebSocket(); // Start the WebSocket server
  initWebserver(); // Start the web server
}

void loop()
{
  checkSwitch();
  checkBattery();
  serviceClient();
  serviceSocket();
  serviceOTA();
  serviceLeds();
}