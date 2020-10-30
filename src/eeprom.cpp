#include "pixelstick.h"
#include "secrets.h"
#include <ESP_EEPROM.h> // Needs to follow pixelstick.h to include Arduino.h and avoid errors

Credentials creds;

void initEEPROM()
{
    EEPROM.begin(sizeof(creds));

    if (EEPROM.percentUsed() >= 0) // Retrieve data if it's there already
    {
        EEPROM.get(0, creds);
    }
    else // No valid data available, so initialise
    {
        Serial.println("EEPROM: No valid data found. EEPROM cleared and initialised");
        strlcpy(creds.clientssid, CLIENTSSID, sizeof(creds.clientssid));
        strlcpy(creds.clientpw, CLIENTPW, sizeof(creds.clientpw));
        EEPROM.put(0, creds);
        if (!EEPROM.commit())
            Serial.println("Error writing to EEPROM");
    }
    EEPROM.end();
}

// New credentials are passed as ssid:pw
bool saveCreds(char *newCreds)
{
    int i;

    for (i = 0; newCreds[i] != ':'; i++)
        ;
    newCreds[i] = 0; // Add terminator to SSID
    strlcpy(creds.clientssid, newCreds, sizeof(creds.clientssid));     // Copy the SSID
    strlcpy(creds.clientpw, newCreds + i + 1, sizeof(creds.clientpw)); // Copy the password
    EEPROM.begin(sizeof(creds));
    EEPROM.put(0, creds);
    if (!EEPROM.commit())
    {
        Serial.println("Error writing to EEPROM");
        return false;
    }
    Serial.print(EEPROM.percentUsed());
    Serial.println("% of EEPROM space currently used");
    EEPROM.end();
    return true;
}
