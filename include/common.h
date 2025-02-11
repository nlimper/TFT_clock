#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <TFT_eSPI.h>

extern Preferences prefs;
extern fs::FS *contentFS;

extern float lux, avgLux;
extern bool hasLightmeter, nightmode;

int perc2ledc(int brightness);
void fadeLEDC(int channel, int from, int to, int duration_ms = 500);
String formatTime(uint16_t x);
void listDir(fs::FS &fs, const char *dirname, uint8_t levels);
String generateSerialWord();
void initLightmeter();
float lightsensorRun();
