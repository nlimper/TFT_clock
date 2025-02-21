#include "OpenFontRender.h"
#include "config.h"
#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <SimpleTimer.h>
#include <TFT_eSPI.h>

extern TFT_eSprite spr;
extern fs::FS *contentFS;
extern TFT_eSPI tft;
extern OpenFontRender truetype;
extern TFT_eSprite sprites[10];
extern Preferences prefs;
extern SimpleTimer timer;

extern int currentFont, currentColor;
extern bool nightmode, flipOrientation;
extern volatile int d1, d2, d3, prevMinute;

extern uint8_t digits[];
extern uint8_t backlight[];
extern Font fonts[20];
extern Color colors[30];

void initTFT();
void initTruetype();
void loadTruetype(String fontFile, int fontSize);

void initSprites(bool reInit);
void clearScreen(uint8_t menuLevel, bool enableBacklight = false);
void selectScreen(uint8_t digitId, bool enableBacklight = true);
void deselectScreen(uint8_t digitId);
void showRaster();
void showColors();
void drawDigit(uint8_t digit, bool useSprite = true);
void showAlarmIcon(uint16_t nextAlarm);
void showNotification(String text);
void resetNotification();
void debugTFT(String message);
void setBrightness(uint8_t channel, uint32_t value);
void addPWM(uint8_t pin, uint8_t channel);
void removePWM(uint8_t pin);

FT_FILE *OFR_fopen(const char *filename, const char *mode);
void OFR_fclose(FT_FILE *stream);
size_t OFR_fread(void *ptr, size_t size, size_t nmemb, FT_FILE *stream);
int OFR_fseek(FT_FILE *stream, long int offset, int whence);
long int OFR_ftell(FT_FILE *stream);
