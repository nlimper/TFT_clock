#include "config.h"
#include <Arduino.h>

extern bool hasAccelerometer;

void interfaceRun();
void initInterface();
void initAccelerometer();
uint8_t accelerometerRun(bool active = true);

