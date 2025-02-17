#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
#include <TFT_eSPI.h>
#include <map>

extern fs::FS *contentFS;
extern TFT_eSPI tft;

#define JSON_FILE_PATH "/system.json"

#ifndef CONFIGH
#define CONFIGH

struct Config {
    bool alarmclock;
    bool autobrightness;
    bool wifi;
    String fliporientation;
    float luxday;
    float luxnight;
    float luxfactor;
    uint8_t maxvolume;
    String deviceName;
    String firmwareName;
};

struct Hardware {
    bool ds3231;
    bool max98357;
    bool lis3d;
    bool bh1750;
    bool photodiode;
    bool solenoid;
    bool rotary;
    bool invertbacklight;
};

struct Font {
    String name;
    String file;
    int size;
    int posX;
    int posY;
};

struct Color {
    String name;
    uint8_t r, g, b;
};

struct Sound {
    String name;
    String filename;
};

struct TimeZone {
    String name;
    String tzstring;
    String tzcity;
};

struct Button {
    int pin;
    int lastState;
    bool pressed;
    unsigned long lastDebounceTime;
    unsigned long pressStartTime;
    unsigned long lastRepeatTime;
};

#endif

extern Config config;
extern Hardware hardware;
extern Font fonts[20];
extern Color colors[30];
extern Sound sounds[20];
extern Button buttons[4];
extern TimeZone timezones[20];
extern uint8_t fontCount;
extern uint8_t colorCount;
extern uint8_t soundCount;
extern uint8_t buttonCount;
extern uint8_t timeZoneCount;

extern uint16_t layoutNextalarmX;
extern uint16_t layoutNextalarmY;

void initConfig();
void haltError(String message);
String generateSerialWord();