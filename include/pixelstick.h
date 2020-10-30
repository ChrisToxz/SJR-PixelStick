#ifndef PIXELSTICK_H
#define PIXELSTICK_H

#define FASTLED_ALLOW_INTERRUPTS 0
#include <FastLED.h>
#include <Arduino.h>
#include <LittleFS.h> // Include the LittleFS library
#include <ArduinoJson.h>
#include <WebSocketsServer.h>

#define USER_SWITCH D2
#define NUM_LEDS 144

#define MAX_COLOURS 5    // Maximum number of colours in fixed colour mode
#define MAX_FIXPRESETS 8 // If bigger than this, need to increase CONFIG_JSON_SIZE
#define MAX_PARMS 3      // Maximum number of user adjustable parameters for motion presets

#define DEFAULT_BRIGHTNESS 36

typedef unsigned char RGBColour[3];

/// Structure to hold configuration data for the running code
/// These are stored in a file in LittleFS and may be updated by the user
/// The lists of presets and bitmap files are not stored but built dynamically to send to the web page
struct Config
{
  bool ledsOn;               // Current state of the LEDs
  unsigned char mode;        // Fixed colour(s), presets or bitmap
  unsigned char delay;       // Delay before display starts after user switch is pressed
  unsigned char brightness;  // Brightness
  unsigned char coloursUsed; // Number of colours used in fixed mode (1-5)
  bool gradient;             // Bands of colour or gradients
  bool interleave;           // Display fixed colours in blocks or interleave them
  // Colour specific information goes here in the JSON version of the configuration
  unsigned char presetIndex; // Current preset
  // Preset specific data goes here in the JSON version of the configuration
  // Palette specific data goes here in the JSON version of the configuration
  unsigned int rowDisplayTime; // Delay before updating the LEDs with the next row of the bitmap
  char bmpFile[32];            // Current bitmap
  char apssid[32];             // ESP8266 Access point SSID
  char appw[16];               // Wifi password for access point (8 characters minimum)
};

/// Structure to hold the fixed colour preset information
struct FixPreset
{
  char name[16];                  // Preset name
  unsigned char coloursUsed;      // Number of colours used (1-5)
  bool gradient;                  // Bands of colour or gradients
  bool interleave;                // Display fixed colours in blocks or interleave them
  RGBColour colours[MAX_COLOURS]; // The colours for this preset
};

/// Structure to hold information about the current BMP file
/// The file is opened to extract this information from the
/// BMP headerbut is then closed.
struct FileInfo
{
  char path[32];           // Full path to the file
  uint32_t fileSize;       // File size
  uint32_t bmpImageoffset; // Start address of image data in file
  uint16_t bmpWidth;       // Image width in pixels
  uint16_t bmpHeight;      // Image height in pixels
  uint16_t planeCount;     // No. of planes in the image
  uint16_t bitDepth;       // Bit depth of the image
  uint16_t compMethod;     // Compression method
  uint16_t status;         // Bitmap file status
};

struct Credentials
{
  char clientssid[32];
  char clientpw[64];
};

// Typedefs and structures for preset data
typedef void (*Preset)(); // Pointer to preset function

struct PresetParm // Structure for parameters user can change
{
  char name[16];
  int values[3]; // This holds min/max/current in that order
};

struct PresetInfo
{
  String name;                 // Preset name
  Preset presetfn;             // Preset function
  PresetParm parms[MAX_PARMS]; // Allow each preset up to three user parameters
  signed char paletteIndex;    // -1 if the preset doesn't use palettes
};

// struct Palette
// {
//   String name;
//   CRGBPalette16 palette;
// };

// Display modes
#define MODE_FIXED 0
#define MODE_PRESET 1
#define MODE_BITMAP 2

// Switch status codes
enum Switch
{
  SWITCH_ON,
  SWITCH_OFF,
  SWITCH_CHANGED_ON,
  SWITCH_CHANGED_OFF
};

// WiFi status after start up
enum WiFiStatus
{
  WIFI_FAILED,
  WIFI_OK_CLIENT,
  WIFI_OK_AP
};

// Bitmap file status conditions
// This should be done with an enum really, but I'm no C++ coder...
#define VALID 0x00
#define BAD_PLANES 0x01
#define BAD_BITDEPTH 0x02
#define BAD_COMPRESSION 0x04
#define BAD_SIGNATURE 0x08
#define OPEN_ERROR 0x10

Config &
getConfig();

#endif // PIXELSTICK_H