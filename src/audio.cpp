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
I2SStream i2sStream;
MP3DecoderHelix decoder;
AudioPlayer audioPlayer(source, i2sStream, decoder);
extern Preferences prefs;

void audioTask(void *pvParameters) {
	while (true) {
		if (xSemaphoreTake(audioMutex, portMAX_DELAY)) {
			audioPlayer.copy();
			xSemaphoreGive(audioMutex);
		}
		vTaskDelay(10 / portTICK_PERIOD_MS);
	}
}

void audioTask() {
	audioPlayer.copy();
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
		audioTask,		 // Task function
		"Audiotask",	 // Name of the task
		20000,			 // Stack size (in words)
		NULL,			 // Task input parameter
		1,				 // Priority of the task
		&audioTaskHandle // Task handle
	);
}

void audioStart(String filename, int volume) {
	audioVolume(volume);
	if (xSemaphoreTake(audioMutex, portMAX_DELAY)) {
		if (audioPlayer.isActive()) audioPlayer.end();
		audioPlayer.begin(-1, false);
		audioPlayer.setAutoNext(false);
		if (filename != "") audioPlayer.setPath(filename.c_str());
		audioPlayer.play();
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
	return audioPlayer.isActive();
}