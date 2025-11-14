#include "wifimanager.h"
#include <Arduino.h>
#include <FS.h>

#ifndef DISABLE_WIFI
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#endif

extern fs::FS *contentFS;

#ifdef DISABLE_WIFI
// Stub implementation for when WiFi is disabled
inline void init_web() {}
#else
void init_web();
#endif
