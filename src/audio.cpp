#include "audio.h"
#include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"
#include "AudioTools/AudioLibs/AudioSourceLittleFS.h"
#include <Arduino.h>
#include <Preferences.h>
#include "config.h"

SemaphoreHandle_t audioMutex;
TaskHandle_t audioTaskHandle = NULL;

I2SStream i2sStream;
MP3DecoderHelix decoder;
VolumeStream volume(i2sStream);

AudioSourceLittleFS source("/", "mp3");
URLStream streamSource;

EncodedAudioStream dec(&volume, &decoder);
StreamCopy streamCopier(dec, streamSource, 8192);

AudioPlayer audioPlayer(source, volume, decoder);

extern Preferences prefs;

void audioTask(void *pvParameters) {
    while (true) {
        if (xSemaphoreTake(audioMutex, portMAX_DELAY)) {
            String(streamCopier.copy());
            if (audioPlayer.isActive()) {
                if (audioPlayer.copy() == 0) {
                    audioPlayer.end();
                }
            }
            xSemaphoreGive(audioMutex);
        }
        vTaskDelay(1 / portTICK_PERIOD_MS);
    }
}

void initAudio() {
    AudioLogger::instance().begin(Serial, AudioLogger::Error);
    auto i2sconfig = i2sStream.defaultConfig(TX_MODE);
    i2sconfig.pin_bck = AUDIO_BCLK;
    i2sconfig.pin_ws = AUDIO_WS;
    i2sconfig.pin_data = AUDIO_DATA;
    i2sconfig.i2s_format = I2S_LSB_FORMAT;
    i2sconfig.channels = 1;

    auto vcfg = volume.defaultConfig();
    vcfg.copyFrom(i2sconfig);
    volume.begin(vcfg); 

    i2sStream.begin(i2sconfig);
    uint16_t volumeVal = prefs.getUShort("volume", 50);
    audioPlayer.begin(-1, false);
    audioPlayer.setAutoNext(false);
    audioPlayer.setVolume(1.0f);
    volume.setVolume(((float)volumeVal / 100) * (config.maxvolume / 100));

    audioMutex = xSemaphoreCreateMutex();
    xTaskCreatePinnedToCore(
        audioTask,        // Task function
        "Audiotask",      // Name of the task
        10000,            // Stack size (in words)
        NULL,             // Task input parameter
        1,                // Priority of the task
        &audioTaskHandle, // Task handle
        1                 // Core ID (0 or 1)
    );
}

void audioStart(String filename, int volume) {
    audioVolume(volume);
    bool isStreaming = filename.startsWith("http");
    if (!isStreaming && audioStreaming()) return;
    if (xSemaphoreTake(audioMutex, portMAX_DELAY)) {
        if (audioPlayer.isActive()) {
            audioPlayer.stop();
            audioPlayer.end();
        }
        if (filename != "") {
            if (isStreaming) {
                audioPlayer.setBufferSize(8192);
                streamSource.setReadBufferSize(16384);
                streamSource.setTimeout(4000);
                streamSource.begin(filename.c_str(), "audio/mp3");
            } else {
                filename = "/sounds/" + filename;
                streamSource.end();
                audioPlayer.setBufferSize(512);
                audioPlayer.setPath(filename.c_str());
                audioPlayer.play();
            }
        }
        xSemaphoreGive(audioMutex);
        Serial.println("Playing " + filename);
    }
}

void audioStart(String filename) {
    uint16_t volume = prefs.getUShort("volume", 50);
    audioStart(filename, volume);
}

void audioStop() {
    if (xSemaphoreTake(audioMutex, portMAX_DELAY)) {
        audioPlayer.stop();
        streamSource.end();
        xSemaphoreGive(audioMutex);
    }
}

void audioVolume(int volumeVal) {
    float volFloat = ((float)volumeVal / 100.0) * ((float)config.maxvolume / 100.0);
    //if (xSemaphoreTake(audioMutex, portMAX_DELAY)) {
        volume.setVolume(volFloat);
        //xSemaphoreGive(audioMutex);
    //}
}

bool audioRunning() {
    return streamSource.httpRequest().connected() || audioPlayer.isActive();
}

bool audioStreaming() {
    return streamSource.httpRequest().connected();
}

void alarmStart(uint8_t soundid) {
    if (sounds[soundid].filename == "*") {
        audioStart(prefs.getString("radiostation", "http://25353.live.streamtheworld.com:80/RADIO10.mp3"));
    } else {
        audioStart(sounds[soundid].filename);
    }
}
