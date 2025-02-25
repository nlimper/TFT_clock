#include <Arduino.h>

void initAudio();
void audioRun();
void audioStart(String filename);
void audioStart(String filename, int volume);
void audioStop();
void audioVolume(int volume, bool save = false);
bool audioRunning();
bool audioStreaming();
void alarmStart(uint8_t soundid);

