#include "wifimanager.h"
#include <Arduino.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

extern fs::FS *contentFS;
extern WifiManager wm;

void init_web();
