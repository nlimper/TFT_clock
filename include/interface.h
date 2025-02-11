#include "config.h"
#include <Arduino.h>

extern bool hasAccelerometer;

void interfaceRun();
void initInterface();
void initAccelerometer();
void accelerometerRun(bool active = true);

