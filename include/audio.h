#include <Arduino.h>

void audioTask(void *pvParameters);
void initAudio();
void audioStart(String filename);
void audioStart(String filename, int volume);
void audioStop();
void audioVolume(int volume);
bool audioRunning();
bool audioStreaming();
void alarmStart(uint8_t soundid);

