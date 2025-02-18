#include "config.h"
#include "common.h"
#include "display.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <TFT_eSPI.h>
#include <map>

Config config;
Hardware hardware;
Font fonts[20];
Color colors[30];
Sound sounds[20];
Button buttons[4];
TimeZone timezones[20];

uint8_t fontCount;
uint8_t colorCount;
uint8_t soundCount;
uint8_t buttonCount;
uint8_t timeZoneCount;

uint16_t layoutNextalarmX = 0;
uint16_t layoutNextalarmY = 0;

bool readConfig() {
    File configFile = contentFS->open(JSON_FILE_PATH, "r");
    if (!configFile) {
        Serial.println("Failed to open config file");
        return false;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, configFile);

    if (error) {
        Serial.print("Failed to parse config file: ");
        Serial.println(error.f_str());
        return false;
    }

    JsonObject configObj = doc["config"];
    config.alarmclock = configObj["alarmclock"];
    config.autobrightness = configObj["autobrightness"];
    config.wifi = configObj["wifi"];
    config.fliporientation = configObj["fliporientation"].as<String>();
    config.luxday = configObj["luxday"];
    config.luxnight = configObj["luxnight"];
    config.luxfactor = configObj["luxfactor"];
    config.maxvolume = configObj["maxvolume"];

    JsonObject identityObj = doc["identity"];
    config.deviceName = identityObj["deviceName"].as<String>();
    config.firmwareName = identityObj["firmwareName"].as<String>();

    JsonObject hardwareObj = doc["hardware"];
    hardware.ds3231 = hardwareObj["ds3231"];
    hardware.max98357 = hardwareObj["max98357"];
    hardware.lis3d = hardwareObj["lis3d"];
    hardware.bh1750 = hardwareObj["bh1750"];
    hardware.photodiode = hardwareObj["photodiode"];
    hardware.solenoid = hardwareObj["solenoid"];
    hardware.rotary = hardwareObj["rotary"];
    hardware.invertbacklight = hardwareObj["invertbacklight"];

    JsonArray buttonPinsArray = hardwareObj["buttons"];
    buttonCount = buttonPinsArray.size();
    for (int i = 0; i < buttonCount; i++) {
        buttons[i].pin = buttonPinsArray[i];
    }

    // Parse 'fonts'
    JsonArray fontsArray = doc["fonts"];
    fontCount = fontsArray.size();
    for (int i = 0; i < fontCount; i++) {
        JsonArray font = fontsArray[i];
        fonts[i].name = font[0].as<String>();
        fonts[i].file = font[1].as<String>();
        fonts[i].size = font[2].as<int>();
        fonts[i].posX = font[3].as<int>();
        fonts[i].posY = font[4].as<int>();
    }
    if (fontCount == 0) {
        Serial.println("No fonts found in config file");
        return false;
    }

    // Parse 'colors'
    JsonArray colorsArray = doc["colors"];
    colorCount = colorsArray.size();
    for (int i = 0; i < colorCount; i++) {
        JsonArray color = colorsArray[i];
        colors[i].name = color[0].as<String>();
        colors[i].r = color[1];
        colors[i].g = color[2];
        colors[i].b = color[3];
    }

    // Parse 'sounds'
    JsonArray soundsArray = doc["sounds"];
    soundCount = soundsArray.size();
    for (int i = 0; i < soundCount; i++) {
        if (!prefs.getBool("enablewifi", false) && sounds[i].filename == "*") continue;
        JsonArray sound = soundsArray[i];
        sounds[i].name = sound[0].as<String>();
        sounds[i].filename = sound[1].as<String>();
    }

    // Parse 'timezone'
    JsonArray timezoneArray = doc["timezone"];
    timeZoneCount = timezoneArray.size();

    if (timeZoneCount < 2) {
        timeZoneCount = 2;
        timezones[0].name = "UTC";
        timezones[0].tzstring = "UTC0";
        timezones[0].tzcity = "UTC";
        timezones[1].name = "CET";
        timezones[1].tzstring = "CET-1CEST,M3.5.0,M10.5.0/3";
        timezones[1].tzcity = "Berlin";
    }

    for (int i = 0; i < timeZoneCount; i++) {
        JsonArray tz = timezoneArray[i];
        timezones[i].name = tz[0].as<String>();
        timezones[i].tzstring = tz[1].as<String>();
        timezones[i].tzcity = tz[2].as<String>();
    }

    // Parse 'layout'
    JsonObject layoutObject = doc["layout"];
    if (!layoutObject.isNull()) {
        layoutNextalarmX = layoutObject["nextalarm"][0];
        layoutNextalarmY = layoutObject["nextalarm"][1];
    } else {
        Serial.println("Invalid layout object");
    }
    return true;
}

void initConfig() {
    if (!readConfig()) {
        haltError("Failed to read config file");
    }
}

void haltError(String message) {
    Serial.println(message);
    digitalWrite(DIGIT1, LOW);
    tft.fillScreen(TFT_RED);
    tft.setTextColor(TFT_YELLOW, TFT_RED);
    tft.setCursor(50, 50, 2);
    tft.println(message);
    digitalWrite(DIGIT1, HIGH);
    ledcWrite(1, 2048);
    while (1) {
        delay(5000);
        Serial.println(message);
    }
}