#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266mDNS.h>
#include "pixelstick.h"
#include "secrets.h"

void initEEPROM();
void setSwitch(Switch);
extern Credentials creds;

ESP8266WiFiMulti wifiMulti; // Create an instance of the ESP8266WiFiMulti class, called 'wifiMulti'

// Status of WiFi so we can tell the user at startup - assume failure to begin with
WiFiStatus wifistatus = WIFI_FAILED;

bool initClient()
{
    WiFi.disconnect(true);
    WiFi.softAPdisconnect(true); // Switch off any existing AP
    initEEPROM();                // Start up the EEPROM to get the credentials

    wifiMulti.addAP(creds.clientssid, creds.clientpw); // Add AP credentials
    // WiFi.begin(creds.clientssid, creds.clientpw);  // Add AP credentials

    Serial.println(F("Connecting ..."));
    int i = 0;
    while (wifiMulti.run() != WL_CONNECTED)
    { // Wait for the Wi-Fi to connect
        delay(500);
        Serial.print(++i);
        Serial.print(' ');
        if (i > 10)
        {
            Serial.println(F("\nFailed to Connect: Timeout "));
            return false;
        }
    }
    Serial.print(F("Connected to "));
    Serial.println(WiFi.SSID()); // Tell us what network we're connected to
    Serial.print(F("IP address: "));
    Serial.println(WiFi.localIP()); // Send the IP address of the ESP8266 to the computer
    wifistatus = WIFI_OK_CLIENT;
    return true;
}

void initAP()
{
    WiFi.disconnect(true);                                            // Make sure station mode is off
    bool success = WiFi.softAP(getConfig().apssid, getConfig().appw); // Start the access point
    if (success)
    {
        Serial.print(F("Access Point \""));
        Serial.print(getConfig().apssid);
        Serial.println(F("\" started"));
        Serial.print(F("IP address:\t"));
        Serial.println(WiFi.softAPIP());
        wifistatus = WIFI_OK_AP;
    }
    else
        Serial.println("Error creating Access Point - is the AP pw (" + String(getConfig().appw) + ") at least 8 characters long?");
}

void initWifi()
{
    WiFi.setSleepMode(WIFI_NONE_SLEEP); // Don't let the WiFi go to sleep

    // Set up the WiFi access point unless the user switch is closed.
    // If the user switch is closed, attempt to connect to an existing network (set credentials in secrets.h)
    // but revert to setting up the AP if that fails
    if (!digitalRead(USER_SWITCH)) // We don't debounce here as the user will close the switch before power-on
    {
        if (!initClient())
            initAP();
        setSwitch(SWITCH_ON);
    }
    else
    {
        initAP();
        setSwitch(SWITCH_OFF);
    }
    if (!MDNS.begin("sjrps"))
    { // Start the mDNS responder for sjrps.local
        Serial.println("Error setting up MDNS responder!");
    }
    Serial.println("mDNS responder started");
}
