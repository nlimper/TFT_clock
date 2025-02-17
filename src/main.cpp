#include "RTClib.h"
#include "Wire.h"
#include "time.h"
#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
#include <SD.h>
#include <SPI.h>
#include <TFT_eSPI.h>

#include "OpenFontRender.h"
#include "audio.h"
#include "common.h"
#include "config.h"
#include "display.h"
#include "interface.h"
#include "menutree.h"
#include "timefunctions.h"
#include "web.h"
#include "wifimanager.h"
#include <SimpleTimer.h>

int timerId = 0;

SimpleTimer timer;
Preferences prefs;
MenuState menustate;
WifiManager wm;

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite sprites[10] = {
    TFT_eSprite(&tft), TFT_eSprite(&tft), TFT_eSprite(&tft), TFT_eSprite(&tft), TFT_eSprite(&tft),
    TFT_eSprite(&tft), TFT_eSprite(&tft), TFT_eSprite(&tft), TFT_eSprite(&tft), TFT_eSprite(&tft)};
OpenFontRender truetype;

RTC_DS3231 rtc;

int currentFont = -1, currentColor = -1;
uint32_t lastmenuactive = 0;
volatile int prevMinute = -1, prevHour = -1;
volatile int d1 = 10, d2 = 10, d3 = 10;
int hour, minute;
int menulevel = 1;
int time_set[6];
uint16_t alarm_set[8] = {24 * 60, 24 * 60, 24 * 60, 24 * 60, 24 * 60, 24 * 60, 24 * 60, 24 * 60};
int hourlyChimeTrigger = false, currentChimeCount = 0;
int alarmActive = 0;
int timerAlarmFlashId, timerAlarmSoundId;
float lux = 75, avgLux = 75;
bool nightmode = false, manualNightmode = false, flipOrientation = false;
bool hasLightmeter = false, hasAccelerometer = false;
fs::FS *contentFS = nullptr;

void doChime() {
    if (hardware.solenoid) {
        digitalWrite(TING_PIN, HIGH);
        vTaskDelay(prefs.getUShort("chime_delay", 20) / portTICK_PERIOD_MS);
        digitalWrite(TING_PIN, LOW);
    } else if (alarmActive == 0) {
        audioStart("bell.mp3");
    }
}

void doCuckoo() {
    if (alarmActive == 0) {
        audioStart("cuckoo.mp3");
    }
}

void alarmFlash() {
    if (millis() % 1000 >= 500) {
        ledcWrite(1, 200);
    } else {
        ledcWrite(1, 4095);
    }
}

void alarmSound() {
    doChime();
}

void alarmAck() {
    if (alarmActive) {
        audioStop();
        alarmActive = 0;
        timer.deleteTimer(timerAlarmSoundId);
        timer.deleteTimer(timerAlarmFlashId);
        ledcWrite(1, perc2ledc(prefs.getUShort("brightness", 15)));
        d1 = 10;
        prevMinute = -1;
    }
}

void displayLux() {
    // Serial.println("Lux: " + String(lux) + " avg: " + String(avgLux));
}

void setup(void) {
    pinMode(TING_PIN, OUTPUT);
    digitalWrite(TING_PIN, LOW);

    Serial.begin(115200);
    Serial.setTxTimeoutMs(0);

    initTFT();

    debugTFT("File system starting");
    if (!LittleFS.begin()) {
        Serial.println("LittleFS initialisation failed!");
        haltError("LittleFS Mount Failed");
    } else {
        contentFS = &LittleFS;
    }

    debugTFT("Read configuration");
    initConfig();

    prefs.begin("clock", false);
    prefs.getBytes("alarm_set", alarm_set, sizeof(alarm_set));

    debugTFT("I2C starting");
    Wire.begin(PIN_SDA, PIN_SCL, 400000);

    initLightmeter();
    if (hasLightmeter) timer.setInterval(1000, displayLux);

    debugTFT("Real time clock init");
    if (!rtc.begin()) {
        Serial.println("Couldn't find RTC");
    }

    if (rtc.lostPower()) {
        Serial.println("RTC lost power, let's set the time!");
        rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }

    debugTFT("Accelerometer init");
    initAccelerometer();
    // int timerAccelerometerId = timer.setInterval(1000, accelerometerRun);

    if (prefs.getBool("enablewifi", false)) {
        debugTFT("WiFi init");
        init_web();
    } else {
        WiFi.mode(WIFI_OFF);
    }

    debugTFT("Audio init");
    initAudio();
    audioStart("bell.mp3");

    debugTFT("Init interfaces");

    String tz = timezones[prefs.getUShort("timezone", 1)].tzstring;
    setenv("TZ", tz.c_str(), 1);
    tzset();

    initInterface();
    initMenu();

    debugTFT("Set clock");
    setESP32RTCfromDS3231();

    debugTFT("Finished!");
}

void loop() {

    timer.run();
    interfaceRun();

    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    int timevalue = timeinfo.tm_hour * 60 + timeinfo.tm_min;
    static uint16_t oldNextAlarm = 24 * 60;
    uint16_t nextAlarm = checkNextAlarm(timeinfo);

    if (menustate == OFF) {
        if (prefs.getBool("enablewifi", false)) wm.poll();
        accelerometerRun();
        lux = lightsensorRun();
        avgLux = 0.98 * avgLux + 0.02 * lux;
        int ledc = perc2ledc(manualNightmode ? 0 : prefs.getUShort("brightness", 15));
        if (alarmActive == 0) ledcWrite(1, ledc);

        int nightTo = prefs.getUShort("night_to", 9);
        if (manualNightmode && prevHour == (nightTo - 1 + 24) % 24 && timeinfo.tm_hour == nightTo) {
            Serial.println("Manual nightmode off");
            manualNightmode = false;
            nightmode = false;
            initSprites(true);
        }
        prevHour = timeinfo.tm_hour;

        bool alarmPassed = (timevalue >= alarm_set[timeinfo.tm_wday] || timevalue >= alarm_set[7]);
        if (isNightMode(timeinfo.tm_hour) && !nightmode && !alarmPassed) {
            Serial.println("Nightmode on");
            nightmode = true;
            initSprites(true);
        }
        if ((!isNightMode(timeinfo.tm_hour) || alarmPassed) && nightmode) {
            Serial.println("Nightmode off");
            nightmode = false;
            initSprites(true);
        }
    }

    if (millis() - lastmenuactive > 60000 && menustate != OFF) {
        exitmenu();
    }

    if (menustate == PREVIEW) {
        hour = timeinfo.tm_hour;
        minute = timeinfo.tm_sec;
    } else {
        if (prefs.getUShort("hourmode", 0) == 2) {
            hour = timeinfo.tm_hour % 12;
            if (hour == 0) hour = 12;
        } else {
            hour = timeinfo.tm_hour;
        }
        minute = timeinfo.tm_min;
    }

    if ((minute != prevMinute || ((d1 == 10 || d2 == 10) && menustate == OFF) || d3 == 10 || nextAlarm != oldNextAlarm) && menustate != MENU) {

        uint16_t alarmStarted = 24 * 60;
        if (alarmActive == 0 && timevalue == nextAlarm - 1) {
            alarmActive = 1;
        }
        if (alarmActive == 1 && timevalue == nextAlarm) {
            alarmActive = 2;
            alarmStarted = timevalue;
            timerAlarmFlashId = timer.setInterval(500, alarmFlash);
            uint16_t soundid = prefs.getUShort("alarmsound", 0);
            if (nightmode) {
                manualNightmode = false;
                nightmode = false;
                initSprites(true);
            }
            audioStart(sounds[soundid].filename);
        }
        if (alarmActive == 2 && audioRunning() == false) {
            alarmActive = 3;
            doChime();
            timerAlarmSoundId = timer.setInterval(5000, alarmSound);
        }
        if (alarmActive == 3 && timevalue > alarmStarted + 15) {
            timer.deleteTimer(timerAlarmSoundId);
            timerAlarmSoundId = timer.setInterval(1000, alarmSound);
            alarmActive = 4;
        }
        if (alarmActive == 4 && timevalue > alarmStarted + 20) {
            alarmAck();
        }

        if (prefs.getUShort("minutesound", 0) > 0 && alarmActive == 0 && !isNightMode(timeinfo.tm_hour) && prevMinute != -1) {
            uint16_t volume = prefs.getUShort("volume", 5);
            audioStart("flip.mp3", prefs.getUShort("minutesound", 0) == 1 ? volume / 1.5 : volume);
        }

        // hourly chime
        if (timeinfo.tm_min > 0) hourlyChimeTrigger = false;
        if (timeinfo.tm_min == 0 && !hourlyChimeTrigger &&
            !isNightMode(timeinfo.tm_hour) &&
            (nextAlarm == 24 * 60 || timevalue > alarm_set[timeinfo.tm_wday])) {

            hourlyChimeTrigger = true;
            currentChimeCount = (timeinfo.tm_hour - 1) % 12;

            switch (prefs.getUShort("hoursound", 0)) {
            case 1: // "once"
                doChime();
                break;

            case 2: // "count"
                doChime();
                if (currentChimeCount > 0) {
                    timer.setTimer(1200, doChime, currentChimeCount);
                }
                break;

            case 3: // "cuckoo"
                doCuckoo();
                if (currentChimeCount > 0) {
                    timer.setTimer(1200, doCuckoo, currentChimeCount);
                }
                break;
            }
        }

        if ((currentFont != prefs.getUShort("font", 0) || currentColor != prefs.getUShort("color", 0)) && (menustate == OFF)) {
            initSprites(true);
        }

        if (oldNextAlarm != nextAlarm && nextAlarm == 24 * 60) d1 = 10;

        if (d1 != int(hour / 10) && menustate == OFF) {
            d1 = int(hour / 10);
            if (d1 > 0 || prefs.getUShort("hourmode", 0) == 1) {
                selectScreen(1);
                drawDigit(d1);
            } else {
                clearScreen(1);
            }
            deselectScreen(1);
            oldNextAlarm = 24 * 60;
        }

        if (oldNextAlarm != nextAlarm && menustate == OFF) {
            if (nextAlarm != 24 * 60) {
                selectScreen(1);
                showAlarmIcon(nextAlarm);
                deselectScreen(1);
            }
            oldNextAlarm = nextAlarm;
        }

        if (d2 != hour % 10 && (menustate == OFF)) {
            d2 = hour % 10;
            selectScreen(2);
            drawDigit(d2);
            deselectScreen(2);
        }

        if (menustate == OFF || menustate == PREVIEW) {
            if (d3 != int(minute / 10)) {
                d3 = int(minute / 10);
                selectScreen(3);
                drawDigit(d3, menustate == PREVIEW ? false : true);
                deselectScreen(3);
            }

            selectScreen(4);
            drawDigit(minute % 10, menustate == PREVIEW ? false : true);
            deselectScreen(4);
        }

        prevMinute = minute;
    }

    if (menustate == DEBUG) {
        uint8_t side = accelerometerRun(false);
        lux = lightsensorRun();
        avgLux = 0.95 * avgLux + 0.05 * lux;
        lastmenuactive = millis();

        selectScreen(4);
        tft.loadFont("/dejavusanscond15", *contentFS);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.fillRect(80 + 30, 40 + 17, 50, 16, TFT_BLACK);
        tft.setCursor(80 + 30, 40 + 17);
        tft.println(hasLightmeter ? String(avgLux) : "N/A");
        tft.fillRect(80 + 30, 40 + 2 * 17, 50, 16, TFT_BLACK);
        tft.setCursor(80 + 30, 40 + 2 * 17);
        tft.println(side);
        tft.unloadFont();
        deselectScreen(4);
    }
}
