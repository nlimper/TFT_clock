#include "sound.h"
#include "audio.h"
#include "config.h"
#include <Arduino.h>
#include <Preferences.h>

// Reserve PSRAM globally before Audio is constructed
#define SPRITE_WIDTH 320
#define SPRITE_HEIGHT 240
#define COLOR_BYTES 2
#define NUM_SPRITES 11
void *reservedPSRAM = ps_malloc(SPRITE_WIDTH * SPRITE_HEIGHT * COLOR_BYTES * NUM_SPRITES + 20);

extern Preferences prefs;
Audio audio;
bool isStreaming;

void initAudio() {
    audio.setPinout(I2S_BCLK, I2S_WS, I2S_DATA);
    audio.setI2SCommFMT_LSB(true);
    audio.forceMono(true);
    uint16_t volumeVal = prefs.getUShort("volume", 50);
    audio.setVolumeSteps(100);
    audio.setVolume(static_cast<uint8_t>(((float)volumeVal) * (config.maxvolume / 100)));

    if (reservedPSRAM != NULL) {
        free(reservedPSRAM);
        reservedPSRAM = NULL;
        Serial.println("** Memory freed");
    }
}

void audioRun() {
    audio.loop();
}

void audioStart(String filename, int volume) {
    bool isStream = filename.startsWith("http");
    if (!isStream && audioStreaming()) return;
    if (audio.isRunning()) {
        audio.stopSong();
    }
    audioVolume(volume);
    if (filename != "") {
        if (isStream) {
            isStreaming = isStream;
            audio.connecttohost(filename.c_str());
        } else {
            filename = "/sounds/" + filename;
            audio.connecttoFS(*contentFS, filename.c_str());
        }
    }
    Serial.println("Playing " + filename);
}


void audio_eof_mp3(const char *info) { // end of file
    Serial.print("audio_eof_mp3: ");
    Serial.println(info);
}

void audio_eof_stream(const char *info) {
    Serial.print("audio_eof_stream: ");
    Serial.println(info);
}

void audio_info(const char *info) {
    // Serial.print("info        ");
    // Serial.println(info);
}

void audio_id3data(const char *info) {
    Serial.print("id3         ");
    Serial.println(info);
}

void audio_showstation(const char *info) {
    Serial.print("station    ");
    Serial.println(info);
}

void audio_showstreamtitle(const char *info) {
    Serial.print("title     ");
    Serial.println(info);
}

void audioStart(String filename) {
    uint16_t volume = prefs.getUShort("volume", 50);
    audioStart(filename, volume);
}

void audioStop() {
    audio.stopSong();
}

void audioVolume(int volumeVal, bool save) {
    float volFloat = ((float)volumeVal) * ((float)config.maxvolume / 100.0);
    audio.setVolume(static_cast<uint8_t>(volFloat));
    if (save) {
        prefs.putUShort("volume", volumeVal);
    }
}

bool audioRunning() {
    return audio.isRunning();
}

bool audioStreaming() {
    return isStreaming && audio.isRunning();
}

void alarmStart(uint8_t soundid) {
    if (sounds[soundid].filename == "*") {
        audioStart(prefs.getString("radiostation", "http://25353.live.streamtheworld.com:80/RADIO10.mp3"));
    } else {
        audioStart(sounds[soundid].filename);
    }
}
