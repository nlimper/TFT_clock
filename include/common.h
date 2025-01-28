#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <TFT_eSPI.h>

extern Preferences prefs;
extern fs::FS *contentFS;

extern float avgLux;

int perc2ledc(int brightness);
String formatTime(uint16_t x);
void listDir(fs::FS &fs, const char *dirname, uint8_t levels);
