#include "RTClib.h"
#include "Wire.h"
#include "time.h"
#include <Arduino.h>
#include <BH1750.h>
#include <FS.h>
#include <SD.h>
#include <LittleFS.h>
#include <SPI.h>
#include <TFT_eSPI.h>

#ifdef ENABLE_WIFI
#include "AsyncUDP.h"
#include <ESPmDNS.h>
#include <WiFi.h>
#include <WiFiUdp.h>
// #include <NTPtimeESP32.h>
#endif

#include "OpenFontRender.h"
#include "audio.h"
#include "common.h"
#include "config.h"
#include "display.h"
#include "interface.h"
#include "menutree.h"
#include "timefunctions.h"
#include <SimpleTimer.h>

int timerId = 0;

#ifdef ENABLE_WIFI
const char *ssid = "****";	   // Set you WiFi SSID
const char *password = "****"; // Set you WiFi password
#endif

SimpleTimer timer;
Preferences prefs;
MenuState menustate;

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite sprites[10] = {
	TFT_eSprite(&tft), TFT_eSprite(&tft), TFT_eSprite(&tft), TFT_eSprite(&tft), TFT_eSprite(&tft),
	TFT_eSprite(&tft), TFT_eSprite(&tft), TFT_eSprite(&tft), TFT_eSprite(&tft), TFT_eSprite(&tft)};
OpenFontRender truetype;

BH1750 lightMeter(0x23); // 0x5C
RTC_DS3231 rtc;

int currentFont = -1, currentColor = -1;
uint32_t lastmenuactive = 0;
int prevMinute = 61, prevHour = 61;
int d1 = 10, d2 = 10, d3 = 10;
int hour, minute;
int menulevel = 1;
int time_set[6];
uint16_t alarm_set[8] = {24 * 60, 24 * 60, 24 * 60, 24 * 60, 24 * 60, 24 * 60, 24 * 60, 0};
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
		audioStart("/sounds/bell.mp3");
	}
}

void doCuckoo() {
	if (alarmActive == 0) {
		audioStart("/sounds/cuckoo.mp3");
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
	Serial.println("Lux: " + String(lux) + " avg: " + String(avgLux));
}

void setup(void) {
	pinMode(TING_PIN, OUTPUT);
	digitalWrite(TING_PIN, LOW);

	Serial.begin(115200);
	Serial.setDebugOutput(true);

	initTFT();

	if (!LittleFS.begin()) {
		Serial.println("LittleFS initialisation failed!");
		haltError("LittleFS Mount Failed");
	} else {
		contentFS = &LittleFS;
	}

	initConfig();

	prefs.begin("clock", false);
	prefs.getBytes("alarm_set", alarm_set, sizeof(alarm_set));

	Wire.begin(PIN_SDA, PIN_SCL, 400000);

	if (hardware.bh1750 && lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE_2)) {
		lightMeter.setMTreg(254);
		Serial.println(F("BH1750 found"));
		hasLightmeter = true;
		timer.setInterval(1000, displayLux);
	} else {
		Serial.println(F("BH1750 not found"));
	}

	if (!rtc.begin()) {
		Serial.println("Couldn't find RTC");
	}

	if (rtc.lostPower()) {
		Serial.println("RTC lost power, let's set the time!");
		rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
	}

	initAccelerometer();
	// int timerAccelerometerId = timer.setInterval(1000, accelerometerRun);

#ifdef ENABLE_WIFI
	WiFi.mode(WIFI_STA);
	WiFi.begin(ssid, password);
	while (WiFi.status() != WL_CONNECTED) {
		Serial.print(".");
		delay(500);
	}
	Serial.println("WiFi connected");
#endif

	initAudio();
	audioStart("/sounds/bell.mp3");

	String tz = timezones[prefs.getUShort("timezone", 1)].tzstring;
	setenv("TZ", tz.c_str(), 1);
	tzset();

	initInterface();
	initMenu();
	initSprites(false);
	setESP32RTCfromDS3231();
}

void loop() {

	timer.run();
	interfaceRun();

	time_t now;
	struct tm timeinfo;
	time(&now);
	localtime_r(&now, &timeinfo);
	int timevalue = timeinfo.tm_hour * 60 + timeinfo.tm_min;
	uint16_t nextAlarm = checkNextAlarm(timeinfo);

	if (menustate == OFF) {
		accelerometerRun();
		if (hasLightmeter) {
			if (lightMeter.measurementReady()) lux = lightMeter.readLightLevel();
		} else {
			lux = nightmode ? 25 : 75;
		}
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

		if (isNightMode(timeinfo.tm_hour) && !nightmode) {
			Serial.println("Nightmode on");
			nightmode = true;
			initSprites(true);
		}
		if (!isNightMode(timeinfo.tm_hour) && nightmode && (nextAlarm == 24 * 60 || timevalue >= alarm_set[timeinfo.tm_wday])) { 
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

	if (minute != prevMinute && menustate != MENU) {

		if (alarmActive == 0 && timevalue == alarm_set[timeinfo.tm_wday] - 1) {
			alarmActive = 1;
		}
		if (alarmActive == 1 && timevalue == alarm_set[timeinfo.tm_wday]) {
			alarmActive = 2;
			timerAlarmFlashId = timer.setInterval(500, alarmFlash);
			uint16_t soundid = prefs.getUShort("alarmsound", 0);
			if (nightmode) {
				manualNightmode = false;
				nightmode = false;
				initSprites(true);
			}
			audioStart("/sounds/" + sounds[soundid].filename);
		}
		if (alarmActive == 2 && audioRunning() == false) {
			alarmActive = 3;
			doChime();
			timerAlarmSoundId = timer.setInterval(5000, alarmSound);
		}
		if (alarmActive == 3 && timevalue > alarm_set[timeinfo.tm_wday] + 15) {
			timer.deleteTimer(timerAlarmSoundId);
			timerAlarmSoundId = timer.setInterval(1000, alarmSound);
			alarmActive = 4;
		}
		if (alarmActive == 4 && timevalue > alarm_set[timeinfo.tm_wday] + 20) {
			alarmAck();
		}

		if (prefs.getUShort("minutesound", 0) > 0 && alarmActive == 0 && !isNightMode(timeinfo.tm_hour) && prevMinute != -1) {
			uint16_t volume = prefs.getUShort("volume", 5);
			audioStart("/sounds/flip.mp3", prefs.getUShort("minutesound", 0) == 1 ? volume / 1.5 : volume);
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

		if (d1 != int(hour / 10) && (menustate == OFF)) {
			d1 = int(hour / 10);
			if (d1 > 0 || prefs.getUShort("hourmode", 0) == 1) {
				selectScreen(1);
				drawDigit(d1);
			} else {
				clearScreen(1);
			}

			if (nextAlarm != 24 * 60) {
				selectScreen(1);
				showAlarmIcon(nextAlarm);
			}

			deselectScreen(1);
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
}
