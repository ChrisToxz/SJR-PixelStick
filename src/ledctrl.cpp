#include "pixelstick.h"

#define DATA_PIN D1
#define LED_TYPE WS2812B
#define COLOUR_ORDER GRB
#define MILLI_AMPS 4000 // Maximum current available to drive the LEDs (4000 allows 3A from the converter at max white)

void writeConfig();
void doFixed();
void doPreset();
void doBitmap();
String update(char *cmd);
String getBMPInfoString(char *filename);
Switch getSwitch();
void writeFixPresets();

extern WiFiStatus wifistatus;
extern PresetInfo presetList[]; // Preset info is held in this array
extern Credentials creds;
extern WebSocketsServer ws;
extern FileInfo currentFile;
extern const uint8_t gamma8[];
extern bool browserInit;

bool saveCreds(char *newCreds);

CRGB leds[NUM_LEDS];

RGBColour colours[MAX_COLOURS]; // All colours default to [0, 0, 0] if no config data

FixPreset fixpresets[MAX_FIXPRESETS];

bool userChanges = false;

bool requestLedsOn = false;
bool requestLedsOff = false;
bool requestDrawBmp = false;
bool repeat = false;
bool looping = false;
bool fileopen = false;
File file;
uint8_t gHue = 0; // rotating "base color" used by many of the patterns

void initLeds()
{
    CRGB initColours[3] = {CRGB::Red, CRGB::Green, CRGB::Blue};

    FastLED.addLeds<WS2812B, DATA_PIN, COLOUR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
    FastLED.setDither(DISABLE_DITHER);                     // TODO: Disable dither only for bitmaps??
    FastLED.setBrightness(DEFAULT_BRIGHTNESS);             // Set brightness to default for startup
    FastLED.setMaxPowerInVoltsAndMilliamps(5, MILLI_AMPS); // Limit the power to the LEDs
    FastLED.clear(true);                                   // Make sure the LEDs are all off to begin with
    delay(1000);                                           // This function is the only place we use delay() as it is before the wifi is running
    for (int i = 0; i < 3; i++)                            // Flash the LEDs R/G/B to show we're awake
    {
        fill_solid(leds, NUM_LEDS, CRGB(initColours[i]));
        FastLED.show();
        delay(250);
        FastLED.clear(true);
        delay(250);
    }
}

void closeFile()
{
    if (fileopen)
    { // Make sure the file is closed
        file.close();
        fileopen = false;
        FastLED.clear(true); // We've closed the file so not looping - clear the LEDs
    }
}

bool sweepStatus()
{
    static int i = 0;
    static bool up = true;
    CRGB statusColour = CRGB::Red;

    // Check whether it's time to do an update
    static unsigned long prevMillis = millis();
    if (millis() - prevMillis < 8)
        return true;
    prevMillis = millis();

    // Sweep up and down - Green in AP mode, Blue in client mode, Red if WiFi failed
    if (!up)
        statusColour = CRGB::Black;
    else if (wifistatus != WIFI_FAILED)
        statusColour = wifistatus == WIFI_OK_AP ? CRGB::Green : CRGB::Blue;

    leds[i] = statusColour;
    FastLED.show();
    if (up)
    {
        if (i == NUM_LEDS - 1)
            up = false;
        else
            i++;
    }
    else // Down
    {
        i--;
    }
    if (i < 0) // We've finished the cycle, return false
        return false;
    return true;
}

void serviceLeds()
{
    static bool startup = true;
    static bool switchDelay = false;
    static unsigned long switchTimer;

    // At startup, give  the user an indication of the WiFi status
    if (startup)
    {
        startup = sweepStatus();
        if (!startup)
        {
            // Get the bitmap info in case the user presses the switch to draw before connecting a browser
            char temp[34] = "F";
            strlcpy(temp + 1, getConfig().bmpFile, sizeof(Config::bmpFile));
            update(temp);
        }
        return;
    }

    if (getSwitch() == SWITCH_CHANGED_ON)
    {                         // User has pressed the switch
        char power[3] = "U1"; // Assume we're switching the LEDs on
        if (getConfig().ledsOn && getConfig().mode != MODE_BITMAP)
        {
            power[1] = '0';      // If they were on, we're switching them off unless we're in bitmap mode
            switchDelay = false; // Set false so we ensure we get full delay if the user presses again before timeout
        }
        update(&power[1]);      // Set the states based on LEDs on/off
        ws.broadcastTXT(power); // .... and tell the browser
        if (getConfig().mode == MODE_BITMAP)
        { // In bitmap mode, set status to display the bitmap (LEDs are not turned off by the switch in bitmap mode)
            requestDrawBmp = true;
            // If not in repeat mode, the bitmap is displayed once
            if (repeat) // If repeat is set, toggle looping
            {           // If looping is true, the bitmap will repeat, if false, it will terminate at the end of the current cycle
                looping = !looping;
            }
        }
        // If the user has pressed the switch to start the display, then start the delay
        // Except: In bitmap mode, don't start the delay if in repeat mode but we're stopping looping
        // Note that this means if you press the button again while displaying an image, the display will freeze before continuing
        if (!switchDelay && (power[1] == '1' && !(getConfig().mode == MODE_BITMAP && repeat && !looping)))
        {
            switchDelay = true;
            switchTimer = millis();
        }
    }
    if (requestLedsOn)
    { // If requested to turn LEDS on, then do it
        getConfig().ledsOn = true;
        requestLedsOn = false;
    }
    if (requestLedsOff)
    { // If requested to turn LEDS off, then do it
        getConfig().ledsOn = false;
        requestLedsOff = false;
        FastLED.clear(true); // Switch the LEDs off
        closeFile();         // Make sure BMP file is closed if open
    }

    if (!(getConfig().ledsOn))
        return; // LEDs are off so nothing else to do

    if (switchDelay)
    { // User has asked to switch on, so check delay has expired
        if (millis() - switchTimer > getConfig().delay * 1000)
        {
            switchDelay = false;
        }
        else
            return;
    }

    // Check whether it's time to do an update
    static unsigned long prevMillis = millis();
    if (millis() - prevMillis < getConfig().rowDisplayTime)
        return;
    prevMillis = millis();
    // The LEDs are on so process as required
    switch (getConfig().mode)
    {
    case MODE_FIXED:
        doFixed();
        break;
    case MODE_PRESET:
        doPreset();
        break;
    case MODE_BITMAP:
        doBitmap();
    }
}

void doFixedBands()
{
    switch (getConfig().coloursUsed)
    {
    case 1:
        fill_solid(leds, NUM_LEDS, CRGB(colours[0][0], colours[0][1], colours[0][2]));
        break;
    case 2:
        if (getConfig().gradient)
        {
            fill_gradient_RGB(leds, NUM_LEDS,
                              CRGB(colours[0][0], colours[0][1], colours[0][2]),
                              CRGB(colours[1][0], colours[1][1], colours[1][2]));
        }
        else
        {
            CRGB colour0 = CRGB(colours[0][0], colours[0][1], colours[0][2]);
            CRGB colour1 = CRGB(colours[1][0], colours[1][1], colours[1][2]);
            for (int i = 0; i < NUM_LEDS / 2; i++)
            {
                leds[i] = colour0;
                leds[i + NUM_LEDS / 2] = colour1;
            }
        }
        break;
    case 3:
        if (getConfig().gradient)
        {
            fill_gradient_RGB(leds, NUM_LEDS,
                              CRGB(colours[0][0], colours[0][1], colours[0][2]),
                              CRGB(colours[1][0], colours[1][1], colours[1][2]),
                              CRGB(colours[2][0], colours[2][1], colours[2][2]));
        }
        else
        {
            CRGB colour0 = CRGB(colours[0][0], colours[0][1], colours[0][2]);
            CRGB colour1 = CRGB(colours[1][0], colours[1][1], colours[1][2]);
            CRGB colour2 = CRGB(colours[2][0], colours[2][1], colours[2][2]);
            for (int i = 0; i < NUM_LEDS / 3; i++)
            {
                leds[i] = colour0;
                leds[i + NUM_LEDS / 3] = colour1;
                leds[i + 2 * NUM_LEDS / 3] = colour2;
            }
        }
        break;
    case 4:
        if (getConfig().gradient)
        {
            fill_gradient_RGB(leds, NUM_LEDS,
                              CRGB(colours[0][0], colours[0][1], colours[0][2]),
                              CRGB(colours[1][0], colours[1][1], colours[1][2]),
                              CRGB(colours[2][0], colours[2][1], colours[2][2]),
                              CRGB(colours[3][0], colours[3][1], colours[3][2]));
        }
        else
        {
            CRGB colour0 = CRGB(colours[0][0], colours[0][1], colours[0][2]);
            CRGB colour1 = CRGB(colours[1][0], colours[1][1], colours[1][2]);
            CRGB colour2 = CRGB(colours[2][0], colours[2][1], colours[2][2]);
            CRGB colour3 = CRGB(colours[3][0], colours[3][1], colours[3][2]);
            for (int i = 0; i < NUM_LEDS / 4; i++)
            {
                leds[i] = colour0;
                leds[i + NUM_LEDS / 4] = colour1;
                leds[i + NUM_LEDS / 2] = colour2;
                leds[i + 3 * NUM_LEDS / 4] = colour3;
            }
        }
        break;
    case 5:
        if (getConfig().gradient)
        {
            fill_gradient_RGB(leds, 0, CRGB(colours[0][0], colours[0][1], colours[0][2]),
                              NUM_LEDS / 4, CRGB(colours[1][0], colours[1][1], colours[1][2]));
            fill_gradient_RGB(leds, NUM_LEDS / 4, CRGB(colours[1][0], colours[1][1], colours[1][2]),
                              NUM_LEDS / 2, CRGB(colours[2][0], colours[2][1], colours[2][2]));
            fill_gradient_RGB(leds, NUM_LEDS / 2, CRGB(colours[2][0], colours[2][1], colours[2][2]),
                              NUM_LEDS * 3 / 4, CRGB(colours[3][0], colours[3][1], colours[3][2]));
            fill_gradient_RGB(leds, NUM_LEDS * 3 / 4, CRGB(colours[3][0], colours[3][1], colours[3][2]),
                              NUM_LEDS - 1, CRGB(colours[4][0], colours[4][1], colours[4][2]));
        }
        else
        {
            CRGB colour0 = CRGB(colours[0][0], colours[0][1], colours[0][2]);
            CRGB colour1 = CRGB(colours[1][0], colours[1][1], colours[1][2]);
            CRGB colour2 = CRGB(colours[2][0], colours[2][1], colours[2][2]);
            CRGB colour3 = CRGB(colours[3][0], colours[3][1], colours[3][2]);
            CRGB colour4 = CRGB(colours[4][0], colours[4][1], colours[4][2]);
            for (int i = 0; i <= NUM_LEDS / 5; i++) // <= as 5 is not a factor of 144
            {
                leds[i] = colour0;
                leds[i + NUM_LEDS / 5] = colour1;
                leds[i + 2 * NUM_LEDS / 5] = colour2;
                leds[i + 3 * NUM_LEDS / 5] = colour3;
                leds[i + 4 * NUM_LEDS / 5] = colour4;
            }
        }
        break;
    }
}

void doFixedInterleave()
{
    CRGB userColours[] = {CRGB(colours[0][0], colours[0][1], colours[0][2]),
                          CRGB(colours[1][0], colours[1][1], colours[1][2]),
                          CRGB(colours[2][0], colours[2][1], colours[2][2]),
                          CRGB(colours[3][0], colours[3][1], colours[3][2]),
                          CRGB(colours[4][0], colours[4][1], colours[4][2])};
    int coloursUsed = getConfig().coloursUsed;

    for (int i = 0; i < NUM_LEDS; i++)
    {
        leds[i] = userColours[i % coloursUsed];
    }
}

void doFixed()
{
    FastLED.setBrightness(getConfig().brightness);
    if (getConfig().interleave)
        doFixedInterleave();
    else
        doFixedBands();
    FastLED.show();
}

void doPreset()
{
    static unsigned long prevMillis = millis();
    if (millis() - prevMillis > 40)
    {
        prevMillis = millis();
        gHue++;
    }
    FastLED.setBrightness(getConfig().brightness);
    presetList[getConfig().presetIndex].presetfn();
    FastLED.show();
}

void drawNextRow()
{
    static unsigned int rowSize;
    static uint32_t offset;
    static uint8_t rowBuffer[NUM_LEDS * 3];

    if (!fileopen)
    {
        if (currentFile.status != VALID)
        {
            Serial.print(F("Bitmap file error:"));
            Serial.println(currentFile.status);
            return;
        }
        // Check the image isn't too wide
        if (currentFile.bmpWidth > NUM_LEDS)
        {
            Serial.println(F("Error: Image is bigger than the number of LEDs"));
            return;
        }
        // Check file exists and open it
        if (!(file = LittleFS.open(currentFile.path, "r")))
        {
            Serial.print(F("File not found"));
            Serial.println(currentFile.path);
            return;
        }
        fileopen = true;
        rowSize = (currentFile.bmpWidth * 3 + 3) & ~3;
        offset = currentFile.bmpImageoffset;     // Set the offset pointer to the first (bottom) row of the image
        memset(rowBuffer, 0, sizeof(rowBuffer)); // Clear the LED array
        FastLED.clear(true);                     // Switch the LEDs off to start
    }

    // Seek to the next row if there's some padding
    if (file.position() != offset)
    {
        file.seek(offset, SeekSet);
    }
    file.read(rowBuffer, currentFile.bmpWidth * 3); // Read  in the row of data
    // Set the LED colours for this row
    uint16_t pixelPtr = 0;
    for (int16_t i = 0; i < currentFile.bmpWidth; i++)
    {
        leds[i].setRGB(gamma8[rowBuffer[pixelPtr + 2]], gamma8[rowBuffer[pixelPtr + 1]], gamma8[rowBuffer[pixelPtr]]);
        pixelPtr += 3;
    }
    FastLED.setBrightness(getConfig().brightness);
    FastLED.show();
    offset += rowSize; // Bump the offset to the next row

    if (offset >= currentFile.bmpImageoffset + currentFile.bmpHeight * rowSize)
    {
        if (looping)
        {                                        // Show the BMP file again
            offset = currentFile.bmpImageoffset; // Reset the offset to the start of the image
        }
        else
        {
            requestDrawBmp = false;
            closeFile();
        }
    }
}

void doBitmap()
{
    if (requestDrawBmp)
    {
        drawNextRow();
    }
}

void setFixPreset(int index)
{
    getConfig().coloursUsed = fixpresets[index].coloursUsed;
    getConfig().gradient = fixpresets[index].gradient;
    getConfig().interleave = fixpresets[index].interleave;
    for (int i = 0; i < fixpresets[index].coloursUsed; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            colours[i][j] = fixpresets[index].colours[i][j];
        }
    }
}

void saveFixPreset(char *name, int index)
{
    strlcpy(fixpresets[index].name, name, sizeof(FixPreset::name));
    fixpresets[index].coloursUsed = getConfig().coloursUsed;
    fixpresets[index].gradient = getConfig().gradient;
    fixpresets[index].interleave = getConfig().interleave;
    for (int i = 0; i < fixpresets[index].coloursUsed; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            fixpresets[index].colours[i][j] = colours[i][j];
        }
    }
    writeFixPresets();
}

String update(char *cmd)
{
    String r, s;
    int i;

    // Serial.print("Command: ");
    // Serial.println(cmd);
    switch (cmd[0])
    {
    case '0': // LEDs off
        requestLedsOff = true;
        requestDrawBmp = false;
        s = "0";
        break;
    case '1': // LEDs on
        requestLedsOn = true;
        s = "1";
        break;
    case 'A': // Update AP SSID and password
        s = cmd;
        for (i = 1; s[i] != ':'; i++)
            ;
        cmd[i] = 0;
        strlcpy(getConfig().apssid, cmd + 1, sizeof(Config::apssid));
        strlcpy(getConfig().appw, cmd + i + 1, sizeof(Config::appw));
        userChanges = true;
        break;
    case 'B': // Set [B]lue
        i = atoi(cmd + 2);
        colours[cmd[1] - '0'][2] = i;
        s = cmd;
        userChanges = true;
        break;
    case 'C': // Save client credentials
        s = 'C';
        if (!saveCreds(cmd + 1))
            s = "?Error saving credentials";
        break;
    case 'D':                 // [D]raw the bitmap
        requestLedsOn = true; // Make sure the LEDs are on
        requestDrawBmp = true;
        looping = cmd[1] - '0';
        s = 'D';
        break;
    case 'E':
        repeat = cmd[1] - '0';
        break;
    case 'F': // Change bitmap [F]ile
        s = "F";
        r = getBMPInfoString(cmd + 1); // Skip the "F" and point to file name
        strlcpy(getConfig().bmpFile, cmd + 1, sizeof(Config::bmpFile));
        if (r.startsWith("?")) // Check for an error
            s = r;             // Return the error message
        else
        {
            s += r;           // Good file data returned
            if (!browserInit) // If this is called during browser init, we're not actually changing the file
            {
                userChanges = true;
            }
        }
        break;
    case 'G': // Set [G]reen
        i = atoi(cmd + 2);
        colours[cmd[1] - '0'][1] = i;
        s = cmd;
        userChanges = true;
        break;
    case 'H': // Set Fixed colour preset index
        setFixPreset(cmd[1] - '0');
        s = cmd;
        userChanges = true;
        break;
    case 'I': // Set brightness ([I]ntensity)
        i = atoi(cmd + 1);
        getConfig().brightness = i;
        s = cmd;
        userChanges = true;
        break;
    case 'J': // Set number of colours used in fixed mode
        getConfig().coloursUsed = cmd[1] - '0';
        s = cmd;
        userChanges = true;
        break;
    case 'K': // Set gradient
        getConfig().gradient = cmd[1] - '0';
        s = cmd;
        userChanges = true;
        break;
    case 'L': // Set switch de[L]ay
        i = atoi(cmd + 1);
        getConfig().delay = i;
        s = cmd;
        userChanges = true;
        break;
    case 'M': // Change [M]ode
        getConfig().mode = cmd[1] - '0';
        closeFile();
        s = cmd; // sends back "M0"/"M1"/"M2"
        // userChanges = true; // We don't count this as a user change unless something else has changed
        break;
    case 'N': // Set interleave
        getConfig().interleave = cmd[1] - '0';
        s = cmd;
        userChanges = true;
        break;
    case 'O':
        s = cmd;
        saveFixPreset(cmd + 2, cmd[1] - '0');
        break;
    case 'P': // Set [P]reset index
        getConfig().presetIndex = atoi(cmd + 1);
        s = cmd;
        userChanges = true;
        break;
    case 'Q': // Set Palette index for the [Q]urrent preset
        presetList[getConfig().presetIndex].paletteIndex = atoi(cmd + 1);
        s = cmd;
        userChanges = true;
        break;
    case 'R': // Set  [R}ed
        i = atoi(cmd + 2);
        colours[cmd[1] - '0'][0] = i;
        s = cmd;
        userChanges = true;
        break;
    case 'S': // [S]ave settings
        writeConfig();
        s = 'S';
        userChanges = false;
        break;
    case 'T': // Set  row display [T]ime
        i = atoi(cmd + 1);
        getConfig().rowDisplayTime = i;
        s = cmd;
        userChanges = true;
        break;
    case 'U': // [U]pdate parameter value
        presetList[getConfig().presetIndex].parms[cmd[1] - '0'].values[2] = atoi(cmd + 2);
        s = cmd;
        userChanges = true;
        break;
    case 'X':        // Delete file
        s = cmd + 1; // Path for file being deleted
        closeFile(); // Should be closed anyway, but just in case
        if (LittleFS.remove(s))
            s = 'X';
        else
            s = "?Error deleting file";
        break;
    default:
        Serial.print(F("Unexpected websocket command: "));
        Serial.println(cmd);
    }
    return s;
}
