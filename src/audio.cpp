#include "audio.h"
#include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"
#include "AudioTools/AudioLibs/AudioSourceLittleFS.h"
#include <Arduino.h>
#include <Preferences.h>

SemaphoreHandle_t audioMutex;
TaskHandle_t audioTaskHandle = NULL;

const char *startFilePath = "/";
const char *ext = "mp3";
AudioSourceLittleFS source(startFilePath, ext);
URLStream streamSource;
I2SStream i2sStream;
MP3DecoderHelix decoder;
EncodedAudioStream dec(&i2sStream, &decoder);
StreamCopy streamCopier(dec, streamSource);
AudioPlayer audioPlayer(source, i2sStream, decoder);
extern Preferences prefs;
bool isStreaming = false;

void audioTask(void *pvParameters) {
    while (true) {
        if (xSemaphoreTake(audioMutex, portMAX_DELAY)) {
            if (isStreaming) {
                streamCopier.copy();
            } else {
                audioPlayer.copy();
            }
            xSemaphoreGive(audioMutex);
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

void initAudio() {
    AudioLogger::instance().begin(Serial, AudioLogger::Error);
    auto config = i2sStream.defaultConfig(TX_MODE);
    config.pin_bck = AUDIO_BCLK;
    config.pin_ws = AUDIO_WS;
    config.pin_data = AUDIO_DATA;
    i2sStream.begin(config);
    uint16_t volume = prefs.getUShort("volume", 5);
    audioPlayer.setVolume((float)volume / 10);

    audioMutex = xSemaphoreCreateMutex();
    xTaskCreate(
        audioTask,       // Task function
        "Audiotask",     // Name of the task
        20000,           // Stack size (in words)
        NULL,            // Task input parameter
        1,               // Priority of the task
        &audioTaskHandle // Task handle
    );
}

void audioStart(String filename, int volume) {
    audioVolume(volume);
    if (xSemaphoreTake(audioMutex, portMAX_DELAY)) {
        if (audioPlayer.isActive()) audioPlayer.end();
        audioPlayer.begin(-1, false);
        audioPlayer.setAutoNext(false);
        if (filename != "") {
            Serial.println("Playing " + filename);
            isStreaming = filename.startsWith("http");
            if (isStreaming) {
                streamSource.setReadBufferSize(2048);
                streamSource.setTimeout(5000);
                streamSource.begin(filename.c_str(), "audio/mp3");
            } else {
                filename = "/sounds/" + filename;
                audioPlayer.setPath(filename.c_str());
                audioPlayer.play();
            }
        }

        xSemaphoreGive(audioMutex);
    }
}

void audioStart(String filename) {
    uint16_t volume = prefs.getUShort("volume", 5);
    audioStart(filename, volume);
}

void audioStop() {
    if (xSemaphoreTake(audioMutex, portMAX_DELAY)) {
        audioPlayer.end();
        xSemaphoreGive(audioMutex);
    }
}

void audioVolume(int volume) {
    if (xSemaphoreTake(audioMutex, portMAX_DELAY)) {
        audioPlayer.setVolume((float)volume / 10);
        xSemaphoreGive(audioMutex);
    }
}

bool audioRunning() {
    return isStreaming || audioPlayer.isActive();
}

bool audioStreaming() {
    return isStreaming;
}