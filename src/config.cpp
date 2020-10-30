#include "pixelstick.h"

const char CONFIG_FILENAME[] PROGMEM = "/config.json";         // Configuration file
const char FIXPRESETS_FILENAME[] PROGMEM = "/fixpresets.json"; // Fixed preset colours file

// Keys for configuration JSON values
const char LEDSON_KEY[] = "ledson";
const char MODE_KEY[] = "mode";
const char DELAY_KEY[] = "delay";
const char BRIGHTNESS_KEY[] = "brightness";
const char COLOURSUSED_KEY[] = "coloursused";
const char GRADIENT_KEY[] = "gradient";
const char INTERLEAVE_KEY[] = "interleave";
const char COLOURS_KEY[] = "colours";
const char PRESETIDX_KEY[] = "presetidx";
const char PRESETS_KEY[] = "presets";
const char PALETTEIDX_KEY[] = "paletteidx";
const char PALETTES_KEY[] = "palettes";
const char NAME_KEY[] = "name";
const char PARMS_KEY[] = "parms";
const char VALUES_KEY[] = "values";
const char ROWTIME_KEY[] = "rowtime";
const char BMPFILE_KEY[] = "bmpfile";
const char APSSID_KEY[] = "apssid";
const char APPW_KEY[] = "appw";

// Initial values for configuration before the user values are set
#define DEFAULT_LEDSON false
#define DEFAULT_MODE MODE_FIXED
#define DEFAULT_DELAY 3
#define DEFAULT_COLOURSUSED 1
#define DEFAULT_GRADIENT false
#define DEFAULT_INTERLEAVE false
#define DEFAULT_PRESETIDX 0
#define DEFAULT_PALETTEIDX 0
#define DEFAULT_ROWTIME 20
#define DEFAULT_BMPFILE "/bmp/sjrps.bmp"
#define DEFAULT_APSSID "SJR-PixelStick"
#define DEFAULT_APPW "l3tm31nn0w" // WARNING: PW *must* be at least 8 characters otherwise setup fails
#define DEFAULT_FPNAME "Empty"

// Size of JSON document to hold config
#define CONFIG_JSON_SIZE 4096

void config2Json(JsonDocument &doc);
void loadConfig();
void writeConfig();
void loadFixPresets();
void writeFixPresets();
void fp2Json(JsonDocument &doc);

Config config; // Holds the current config values

extern const String paletteNames[];
extern const uint8_t paletteNum;
extern PresetInfo presetList[]; // Preset info is held in this array
extern const uint8_t presetNum; // Number of presets in the list
extern RGBColour colours[5];
extern FixPreset fixpresets[MAX_FIXPRESETS];

///
/// Load the configuration data and, if not saved in the filessystem, save it.
///
void initConfig()
{
    loadConfig();
    if (!LittleFS.exists(FPSTR(CONFIG_FILENAME)))
    {
        writeConfig();
    }
    loadFixPresets();
    if (!LittleFS.exists(FPSTR(FIXPRESETS_FILENAME)))
    {
        writeFixPresets();
    }
}

Config &getConfig()
{
    return config;
}

/// Load user variables for colours
void loadColours(JsonDocument &doc)
{
    byte i = 0;

    if (doc[COLOURS_KEY]) // Colours entry in the config, so load the user settings
    {
        JsonArray userColours = doc[COLOURS_KEY].as<JsonArray>();
        for (JsonArray userColour : userColours)
        {
            colours[i][0] = userColour[0];
            colours[i][1] = userColour[1];
            colours[i][2] = userColour[2];
            i++;
        }
    }
    else
    {
        Serial.println("No colours in config, using defaults");
    }
}

/// Load user variables for presets
void loadPresets(JsonDocument &doc)
{
    byte i = 0;

    // Serial.println("Checking presets.... ");

    if (doc[PRESETS_KEY]) // Presets entry in the config, so load the preset user settings
    {
        // Serial.println("Presets in config - loading user settings");
        JsonArray presets = doc[PRESETS_KEY].as<JsonArray>();
        for (JsonObject preset : presets)
        {
            // Serial.print("Preset: ");
            // Serial.println((const char *)preset[NAME_KEY]);
            if (preset[PARMS_KEY]) // This preset has parms, so get the user setting
            {
                JsonArray parms = preset[PARMS_KEY].as<JsonArray>();
                for (JsonObject parm : parms)
                {
                    // Serial.print("Parm: ");
                    // Serial.println((const char *)parm[NAME_KEY]);
                    JsonArray values = parm[VALUES_KEY].as<JsonArray>();
                    // Serial.printf("Updating user value (%d) for preset: %s\n", (int)values[2], (const char *)parm[NAME_KEY]);
                    presetList[i].parms->values[2] = values[2];
                }
                i++; // Bump to next preset
            }
        }
    }
    else
    {
        Serial.println("No presets in config, using parameter setting defaults");
    }
}

void loadConfig()
{
    Serial.println("Loading config data");
    DynamicJsonDocument doc(CONFIG_JSON_SIZE);
    File file = LittleFS.open(FPSTR(CONFIG_FILENAME), "r");
    // Deserialize the JSON document
    DeserializationError error = deserializeJson(doc, file);
    if (error)
    {
        Serial.print(F("Failed to read config file, using default configuration: "));
        Serial.println(error.c_str());
    }
    // Copy values from the JsonDocument to the Config or use the defaults
    config.ledsOn = doc[LEDSON_KEY] | DEFAULT_LEDSON;
    config.mode = doc[MODE_KEY] | DEFAULT_MODE;
    config.brightness = doc[BRIGHTNESS_KEY] | DEFAULT_BRIGHTNESS;
    config.delay = doc[DELAY_KEY] | DEFAULT_DELAY;
    config.coloursUsed = doc[COLOURSUSED_KEY] | DEFAULT_COLOURSUSED;
    config.gradient = doc[GRADIENT_KEY] | DEFAULT_GRADIENT;
    config.interleave = doc[INTERLEAVE_KEY] | DEFAULT_INTERLEAVE;
    loadColours(doc);
    config.presetIndex = doc[PRESETIDX_KEY] | DEFAULT_PRESETIDX;
    config.rowDisplayTime = doc[ROWTIME_KEY] | DEFAULT_ROWTIME;
    strlcpy(config.bmpFile, doc[BMPFILE_KEY] | DEFAULT_BMPFILE, sizeof(config.bmpFile));
    strlcpy(config.apssid, doc[APSSID_KEY] | DEFAULT_APSSID, sizeof(config.apssid));
    strlcpy(config.appw, doc[APPW_KEY] | DEFAULT_APPW, sizeof(config.appw));
    loadPresets(doc); // Get the user settings for preset parameters
    file.close();
}

void loadFixPresets()
{
    DynamicJsonDocument doc(CONFIG_JSON_SIZE);
    JsonArray presetArray;
    File file = LittleFS.open(FPSTR(FIXPRESETS_FILENAME), "r");
    // Deserialize the JSON document
    DeserializationError error = deserializeJson(doc, file);
    if (error)
    {
        Serial.print(F("Failed to read colour presets file, using default values: "));
        Serial.println(error.c_str());
    }
    else
    {
        Serial.println("Reading preset file");
        presetArray = doc.as<JsonArray>();
    }
    // Copy values from the JsonDocument to the Config or use the defaults
    for (int i = 0; i < MAX_FIXPRESETS; i++)
    {
        strlcpy(fixpresets[i].name, presetArray[i][NAME_KEY] | DEFAULT_FPNAME, sizeof(FixPreset::name));
        fixpresets[i].coloursUsed = presetArray[i][COLOURSUSED_KEY] | DEFAULT_COLOURSUSED;
        fixpresets[i].gradient = presetArray[i][GRADIENT_KEY] | DEFAULT_GRADIENT;
        fixpresets[i].interleave = presetArray[i][INTERLEAVE_KEY] | DEFAULT_INTERLEAVE;
        for (int j = 0; j < MAX_COLOURS; j++) // Max colours rather than colours used to ensure fully populated
        {
            for (int k = 0; k < 3; k++)
                fixpresets[i].colours[j][k] = presetArray[i][COLOURS_KEY][j][k] | 0;
        }
    }
    file.close();
}

void writeConfig()
{
    Serial.println("Writing config");
    File file = LittleFS.open(FPSTR(CONFIG_FILENAME), "w");
    if (!file)
    {
        Serial.print(F("Failed to open config file for write: "));
        Serial.println(FPSTR(CONFIG_FILENAME));
        return;
    }
    DynamicJsonDocument doc(CONFIG_JSON_SIZE); // Allocate a temporary JsonDocument
    config2Json(doc);
    doc[LEDSON_KEY] = false;       // We always want the default state to be off
    if (!serializeJson(doc, file)) // Write the JSON to file
    {
        Serial.print(F("Failed to write config file: "));
        Serial.println(FPSTR(CONFIG_FILENAME));
    }
    file.close();
}

void writeFixPresets()
{
    Serial.println("Writing fixed presets");
    File file = LittleFS.open(FPSTR(FIXPRESETS_FILENAME), "w");
    if (!file)
    {
        Serial.print(F("Failed to open fixed preset file for write: "));
        Serial.println(FPSTR(CONFIG_FILENAME));
        return;
    }
    DynamicJsonDocument doc(CONFIG_JSON_SIZE); // Allocate a temporary JsonDocument
    fp2Json(doc);
    if (!serializeJson(doc, file)) // Write the JSON to file
    {
        Serial.print(F("Failed to write fix presets file: "));
        Serial.println(FPSTR(FIXPRESETS_FILENAME));
    }
    file.close();
}

/// Add the colours to ths JSON document
void getColours(JsonDocument &doc)
{
    JsonArray cols = doc.createNestedArray(COLOURS_KEY);

    for (byte i = 0; i < MAX_COLOURS; i++)
    {
        JsonArray rgbvals = cols.createNestedArray();
        rgbvals[0] = colours[i][0];
        rgbvals[1] = colours[i][1];
        rgbvals[2] = colours[i][2];
    }
}

/// Add the palettes to ths JSON document
void getPalettes(JsonDocument &doc)
{
    JsonArray pals = doc.createNestedArray(PALETTES_KEY);
    for (byte i = 0; i < paletteNum; i++)
    {
        pals.add(paletteNames[i]);
    }
}

/// Add the presets to ths JSON document
void getPresets(JsonDocument &doc)
{
    JsonArray presets = doc.createNestedArray(PRESETS_KEY);
    for (byte i = 0; i < presetNum; i++)
    {
        JsonObject preset = presets.createNestedObject();
        preset[NAME_KEY] = presetList[i].name;
        if (presetList[i].parms[0].name[0]) // Check if this preset has parms or not
        {
            JsonArray parms = preset.createNestedArray(PARMS_KEY);
            for (byte j = 0; j < MAX_PARMS; j++)
            {
                if (presetList[i].parms[j].name[0]) // Check if this parm exists
                {
                    JsonObject parm = parms.createNestedObject();
                    parm[NAME_KEY] = presetList[i].parms[j].name;
                    JsonArray values = parm.createNestedArray(VALUES_KEY);
                    values.add(presetList[i].parms[j].values[0]);
                    values.add(presetList[i].parms[j].values[1]);
                    values.add(presetList[i].parms[j].values[2]);
                }
            }
        }
        if (presetList[i].paletteIndex >= 0)
            preset[PALETTEIDX_KEY] = presetList[i].paletteIndex;
    }
}

/// Convert the config data into a JSON document
void config2Json(JsonDocument &doc)
{
    doc[LEDSON_KEY] = config.ledsOn; // Set the values in the document
    doc[MODE_KEY] = config.mode;
    doc[BRIGHTNESS_KEY] = config.brightness;
    doc[DELAY_KEY] = config.delay;
    doc[COLOURSUSED_KEY] = config.coloursUsed;
    doc[GRADIENT_KEY] = config.gradient;
    doc[INTERLEAVE_KEY] = config.interleave;
    getColours(doc);
    doc[PRESETIDX_KEY] = config.presetIndex;
    getPresets(doc);
    getPalettes(doc);
    doc[ROWTIME_KEY] = config.rowDisplayTime;
    doc[BMPFILE_KEY] = config.bmpFile;
    doc[APSSID_KEY] = config.apssid;
    doc[APPW_KEY] = config.appw;
}

/// Returns the config as a JSON string
String getConfigJson()
{
    DynamicJsonDocument doc(CONFIG_JSON_SIZE);
    String s = "";

    config2Json(doc); // Get the config data as a JSON document
    serializeJson(doc, s);
    return s;
}

/// Convert the fixed presets data into a JSON document
void fp2Json(JsonDocument &doc)
{
    JsonArray presetArray = doc.to<JsonArray>();

    for (int i = 0; i < MAX_FIXPRESETS; i++)
    {
        JsonObject preset = presetArray.createNestedObject();
        preset[NAME_KEY] = fixpresets[i].name;
        preset[COLOURSUSED_KEY] = fixpresets[i].coloursUsed;
        preset[GRADIENT_KEY] = fixpresets[i].gradient;
        preset[INTERLEAVE_KEY] = fixpresets[i].interleave;
        JsonArray colourArray = preset.createNestedArray(COLOURS_KEY);
        for (int j = 0; j < MAX_COLOURS; j++)
        {
            JsonArray colourEntry = colourArray.createNestedArray();
            for (int k = 0; k < 3; k++)
            {
                colourEntry.add(fixpresets[i].colours[j][k]);
            }
        }
    }
}

/// Returns the fixed presets as a JSON string
String getFixPresetJson()
{
    DynamicJsonDocument doc(CONFIG_JSON_SIZE);
    String s = "";

    fp2Json(doc); // Get the config data as a JSON document
    serializeJson(doc, s);
    return s;
}
