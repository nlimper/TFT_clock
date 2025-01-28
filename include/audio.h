#include <Arduino.h>

void audioTask(void *pvParameters);
void initAudio();
void audioStart(String filename);
void audioStart(String filename, int volume);
void audioStop();
void audioVolume(int volume);
void audioTask();
bool audioRunning();
