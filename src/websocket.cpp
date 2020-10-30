#include "pixelstick.h"

String update(char *cmd);
String getConfigJson();
String getFixPresetJson();
String getBMPList();
String getSystemInfo();
void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length);

extern bool userChanges;

WebSocketsServer ws(81); // Create a websocket server on port 81
bool browserInit;

void initWebSocket()
{                               // Start a WebSocket server
    ws.begin();                 // start the websocket server
    ws.onEvent(webSocketEvent); // if there's an incomming websocket message, go to function 'webSocketEvent'
    Serial.println("WebSocket server started.");
}

void serviceSocket()
{
    ws.loop(); // constantly check for websocket events
}

// Invoked when a WebSocket message is received
void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length)
{
    IPAddress ip;
    String s;

    switch (type)
    {
    case WStype_DISCONNECTED: // if the websocket is disconnected
        Serial.printf("[%u] Disconnected!\n", num);
        break;
    case WStype_CONNECTED: // if a new websocket connection is established
        ip = ws.remoteIP(num);
        Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
        break;
    case WStype_TEXT: // if new text data is received
        // Serial.printf("[%u] Received: %s\n", num, payload);
        switch (payload[0])
        {
        case 'U': // [U]pdate a value
            s = "U";
            s += update((char *)(payload + 1));
            break;
        case 'B': // Request a list of [B]itmap files (in /bmp/)
            s = "B";
            s += getBMPList();
            break;
        case 'S': // Request [S]ystem info
            s = "S";
            s += getSystemInfo();
            break;
        case 'C': // Request [C]onfig data to initialise the web interface
            s = "C";
            browserInit = true; // 'C' only used at browser init
            s += getConfigJson();
            break;
        case 'F': // Request [C]onfig data to initialise the web interface
            s = "F";
            s += getFixPresetJson();
            break;
        case 'I': // Browser [I}nit complete, return user change status
            s = "I";
            browserInit = false;
            s += userChanges ? "1" : "0";
            break;
        default:
            s = "XInvalid websocket request: " + (String)((char *)payload);
            Serial.println("Invalid websocket request: " + (String)((char *)payload));
        }
        ws.sendTXT(num, s);
        // Serial.print("Sending: ");
        // Serial.println(s);
        break;
    case WStype_PING: // if the websocket is disconnected
        Serial.printf("[%u] Ping received\n", num);
        // Should send a Pong here....
        break;
    case WStype_PONG: // if the websocket is disconnected
        Serial.printf("[%u] Pong received\n", num);
        break;
    case WStype_BIN:
    case WStype_FRAGMENT_TEXT_START:
    case WStype_FRAGMENT_BIN_START:
    case WStype_FRAGMENT:
    case WStype_FRAGMENT_FIN:
    default:
        Serial.printf("Unexpected type: %d\n", type);
    }
}

void checkBattery()
{
    static unsigned long prevMillis = 0;
    static byte count = 0;
    static unsigned int voltage = 0;

    char s[6]; // Maximum of 'V' + 4 digits + 0x00
    // Original plan was to  take a reading every 5 seconds but
    // the ADC gave inconsistent results. Forum discussions suggest that
    // ADC performance suffers when WiFi is active and that taking the average
    // of several readings can mitigate this, so now averaging 32  readings.
    // Take a reading every 150ms
    if (millis() - prevMillis < 150)
        return;

    prevMillis = millis();
    // read the value from the sensor and bump the counter
    voltage += analogRead(A0);
    count++;
    // Every 32 readings (4.8s), output the average
    if (count == 32)
    {
        sprintf(s, "V%d", voltage >> 5);
        ws.broadcastTXT(s);
        count = 0;
        voltage = 0;
    }
}