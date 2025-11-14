#include <Arduino.h>

#ifndef DISABLE_AUDIO
// Audio functions - full implementation
void initAudio();
void audioRun();
void audioStart(String filename);
void audioStart(String filename, int volume);
void audioStop();
void audioVolume(int volume, bool save = false);
bool audioRunning();
bool audioStreaming();
void alarmStart(uint8_t soundid);
#else
// Audio disabled - stub functions
inline void initAudio() {}
inline void audioRun() {}
inline void audioStart(String filename) { (void)filename; }
inline void audioStart(String filename, int volume) { (void)filename; (void)volume; }
inline void audioStop() {}
inline void audioVolume(int volume, bool save = false) { (void)volume; (void)save; }
inline bool audioRunning() { return false; }
inline bool audioStreaming() { return false; }
inline void alarmStart(uint8_t soundid) { (void)soundid; }
#endif
