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
            // Get current states using library methods
            bool current_player_state = audioPlayer.isActive();
            bool current_streamcopier_state = streamCopier.isActive();

            // Detect local file playback changes
            if (current_player_state != last_player_state) {
                if (current_player_state) {
                    onPlaybackStarted(); // Local file started
                } else {
                    onPlaybackStopped(); // Local file ended
                }
                last_player_state = current_player_state;
            }

            // Detect stream changes
            if (current_streamcopier_state != last_streamcopier_state) {
                if (current_streamcopier_state) {
                    onStreamStarted(); // Network stream started
                } else {
                    onStreamStopped(); // Network stream ended
                }
                last_streamcopier_state = current_streamcopier_state;
            }

            // Let the library handle what to copy based on its internal state
            if (current_streamcopier_state) { // If stream source is active
                streamCopier.copy();
            }
            if (current_player_state) {
                if (audioPlayer.copy() == 0) {
                    audioPlayer.end();
                }
            }

            xSemaphoreGive(audioMutex);
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

void initAudio() {
    AudioLogger::instance().begin(Serial, AudioLogger::Warning);
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
                audioPlayer.end();
                streamSource.setReadBufferSize(4096);
                streamSource.setTimeout(4000);
                streamSource.begin(filename.c_str(), "audio/mp3");
            } else {
                filename = "/sounds/" + filename;
                streamSource.end();
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
    return isStreaming || audioPlayer.isActive();
}

bool audioStreaming() {
    return isStreaming;
}