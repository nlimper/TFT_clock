#include "RTClib.h"
#include <Arduino.h>

#ifndef TIME_COMMON
#define TIME_COMMON

void setESP32RTCfromDS3231();
void setSystemTime();
uint16_t checkNextAlarm(struct tm timeinfo);
bool isNightMode(int hour);
void synchronizeNTP();

#endif