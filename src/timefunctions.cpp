#include "RTClib.h"
#include "config.h"
#include "esp_sntp.h"
#include <Arduino.h>
#include <Preferences.h>
#include <functional>
#include <timefunctions.h>

extern RTC_DS3231 rtc;
const char *ntpServer = "pool.ntp.org";
extern const char *timezone;
extern uint16_t time_set[];
extern uint16_t alarm_set[8];
extern float avgLux;
extern Preferences prefs;
extern bool nightmode, manualNightmode, hasLightmeter;

void setESP32RTCfromDS3231() {
    DateTime now = rtc.now();
    time_t epochTime = now.unixtime();
    struct timeval tv = {epochTime, 0};
    settimeofday(&tv, NULL);
    Serial.printf("ESP32 RTC set from DS3231. Epoch time: %ld\n", epochTime);
}

void setSystemTime() {
    struct tm timeinfo;
    timeinfo.tm_year = time_set[0] - 1900; // Year since 1900
    timeinfo.tm_mon = time_set[1] - 1;     // Month (0-11)
    timeinfo.tm_mday = time_set[2];        // Day of the month
    timeinfo.tm_hour = time_set[3];        // Hours
    timeinfo.tm_min = time_set[4];         // Minutes
    timeinfo.tm_sec = 0;                   // Seconds (optional)
    timeinfo.tm_isdst = -1;                // Daylight Saving Time flag (auto-detect)

    time_t epoch_time = mktime(&timeinfo);
    if (epoch_time == -1) {
        Serial.println("Error: invalid time structure.");
        return;
    }

    Serial.printf("Epoch time: %ld\n", epoch_time);

    struct timeval tv = {epoch_time, 0};
    settimeofday(&tv, NULL);
    Serial.println("ESP32 system time updated.");
    Serial.println(epoch_time);

    rtc.adjust(DateTime(epoch_time));
    Serial.println("DS3231 RTC updated.");
}

volatile bool sntpCallbackTriggered = false;
extern "C" void time_sync_notification_cb(struct timeval *tv) {
    sntpCallbackTriggered = true;
}

void synchronizeNTP() {
    static bool NTPsynced = false;
    if (!NTPsynced) {
        NTPsynced = true;
        sntpCallbackTriggered = false;

        sntp_set_time_sync_notification_cb(time_sync_notification_cb);

        String tz = timezones[prefs.getUShort("timezone", 1)].tzstring;
        setenv("TZ", tz.c_str(), 1);
        tzset();
        configTime(0, 0, ntpServer);
        xTaskCreate([](void *param) {
            while (true) {
                if (sntpCallbackTriggered) {
                    Serial.println("NTP sync successful");
                    time_t now;
                    time(&now);
                    rtc.adjust(DateTime(now));

                    sntpCallbackTriggered = false;
                }
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
        },
                    "NTP Sync Task", 4096, nullptr, 1, nullptr);
    }
}

uint16_t checkNextAlarm(struct tm timeinfo) {
    uint8_t dow = timeinfo.tm_wday;
    uint16_t timevalue = timeinfo.tm_hour * 60 + timeinfo.tm_min;
    uint16_t nextAlarm = 24 * 60; 

    if (alarm_set[dow] != 24 * 60 && alarm_set[dow] >= timevalue) {
        nextAlarm = alarm_set[dow];
    }
    else if (timevalue >= 18 * 60 && alarm_set[(dow + 1) % 7] != 24 * 60 && alarm_set[(dow + 1) % 7] < timevalue) {
        nextAlarm = alarm_set[(dow + 1) % 7];
    }

    if (alarm_set[7] != 24 * 60 && alarm_set[7] < nextAlarm) {
        nextAlarm = alarm_set[7];
    }

    return nextAlarm;
}

bool isNightMode(int hour) {
    if (manualNightmode) {
        return true;
    }
    if (hasLightmeter) {
        if (avgLux <= config.luxnight || (nightmode == true && avgLux < config.luxday)) {
            return true;
        }
    } else {
        int nightFrom = prefs.getUShort("night_from", 22);
        int nightTo = prefs.getUShort("night_to", 9);

        if ((nightFrom <= nightTo && (hour >= nightFrom && hour < nightTo)) ||
            (nightFrom > nightTo && (hour >= nightFrom || hour < nightTo))) {
            return true;
        }
    }
    return false;
}

void setDailyAlarm(uint16_t alarmTime) {
    Serial.println("Setting daily alarm to " + String(alarmTime));
    alarm_set[7] = alarmTime;
    prefs.putBytes("alarm_set", alarm_set, sizeof(alarm_set));
}

uint16_t getDailyAlarm() {
    Serial.println("Getting daily alarm: " + String(alarm_set[7]));
    return alarm_set[7];
}
