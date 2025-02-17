#include "audio.h"
#include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"
#include "AudioTools/AudioLibs/AudioSourceLittleFS.h"
#include <Arduino.h>
#include <Preferences.h>

SemaphoreHandle_t audioMutex;
TaskHandle_t audioTaskHandle = NULL;

I2SStream i2sStream;
MP3DecoderHelix decoder;

AudioSourceLittleFS source("/", "mp3");
URLStream streamSource;

EncodedAudioStream dec(&i2sStream, &decoder);
StreamCopy streamCopier(dec, streamSource);

AudioPlayer audioPlayer(source, i2sStream, decoder);

extern Preferences prefs;

void onStreamStarted() {
    Serial.println("Network stream started");
    // Add your stream start logic here
}

void onStreamStopped() {
    Serial.println("Network stream stopped");
    // Add your stream stop logic here
}

void onPlaybackStarted() {
    Serial.println("Local playback started");
    // Add your local file start logic here
}

void onPlaybackStopped() {
    Serial.println("Local playback stopped");
    // Add your local file stop logic here
}

void audioTask(void *pvParameters) {
    bool last_player_state = false;
    bool last_streamcopier_state = false;

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
        vTaskDelay(3 / portTICK_PERIOD_MS);
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
        10000,           // Stack size (in words)
        NULL,            // Task input parameter
        1,               // Priority of the task
        &audioTaskHandle // Task handle
    );
}

void audioStart(String filename, int volume) {
    bool isStreaming = filename.startsWith("http");
    if (!isStreaming && audioStreaming()) return;
    audioVolume(volume);
    if (xSemaphoreTake(audioMutex, portMAX_DELAY)) {
        if (audioPlayer.isActive()) audioPlayer.end();
        audioPlayer.begin(-1, false);
        audioPlayer.setAutoNext(false);
        if (filename != "") {
            if (isStreaming) {
                audioPlayer.end();
                streamSource.setReadBufferSize(8192);
                streamSource.setTimeout(4000);
                streamSource.begin(filename.c_str(), "audio/mp3");
            } else {
                filename = "/sounds/" + filename;
                streamSource.end();
                audioPlayer.end();
                audioPlayer.setPath(filename.c_str());
                audioPlayer.play();
            }
            Serial.println("Playing " + filename);
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
        streamSource.end();
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
    return streamSource.httpRequest().connected() || audioPlayer.isActive();
}

bool audioStreaming() {
    return streamSource.httpRequest().connected();
}