#include "RTClib.h"
#include <Arduino.h>

#ifndef TIME_COMMON
#define TIME_COMMON

extern volatile int d1, d2, d3, prevMinute;

void setESP32RTCfromDS3231();
void setSystemTime();
uint16_t checkNextAlarm(struct tm timeinfo);
bool isNightMode(int hour);
void synchronizeNTP();
void setDailyAlarm(uint16_t alarmTime);
uint16_t getDailyAlarm();

#endif