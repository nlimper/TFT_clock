#include "common.h"
#include "config.h"
#include "esp_system.h"
#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <math.h>

int perc2ledc(int brightness) {
	avgLux = constrain(avgLux, 0, 1000);

	float exponent = 1.1;
	uint16_t min_output = prefs.getULong("minbrightness", 30);
	float max_output = 4095;

	float normalized_value = (float)brightness / 20.0;
	int output = constrain((float)min_output + (avgLux / config.luxfactor) * (int)((pow(normalized_value, exponent)) * (max_output - min_output)), 0, 4095);
	if (hardware.invertbacklight) output = 4095 - output;
	return output;
}

void fadeLEDC(int channel, int from, int to, int duration_ms) {
	const int steps = 50;
	const int delayTime = duration_ms / steps;
	for (int i = 0; i <= steps; i++) {
		float t = (float)i / steps;
		float easedT = t * t * (3 - 2 * t);
		int value = from + easedT * (to - from);
		ledcWrite(channel, value);
		vTaskDelay(delayTime / portTICK_PERIOD_MS);
	}
}

String formatTime(uint16_t x) {
	char buffer[10];
	uint8_t hourmode = prefs.getUShort("hourmode", 0);
	uint8_t hours = x / 60;
	uint8_t minutes = x % 60;

	if (x != 24 * 60) {
		switch (hourmode) {
		case 0:
			sprintf(buffer, "%d:%02d", hours, minutes);
			break;
		case 1:
			sprintf(buffer, "%02d:%02d", hours, minutes);
			break;
		case 2: {
			bool isPM = hours >= 12;
			hours = hours % 12;
			if (hours == 0) hours = 12;
			sprintf(buffer, "%d:%02d %s", hours, minutes, isPM ? "PM" : "AM");
			break;
		}
		default:
			sprintf(buffer, "%d:%02d", hours, minutes);
			break;
		}
		return String(buffer);
	} else {
		return "";
	}
}

void listDir(fs::FS &fs, const char *dirname, uint8_t levels) {
	Serial.printf("Listing directory: %s\r\n", dirname);

	File root = fs.open(dirname);
	if (!root) {
		Serial.println("- failed to open directory");
		return;
	}
	if (!root.isDirectory()) {
		Serial.println(" - not a directory");
		return;
	}

	File file = root.openNextFile();
	while (file) {
		if (file.isDirectory()) {
			Serial.print("  DIR : ");
			Serial.println(file.name());
			if (levels) {
				listDir(fs, file.path(), levels - 1);
			}
		} else {
			Serial.print("  FILE: ");
			Serial.print(file.name());
			Serial.print("\tSIZE: ");
			Serial.println(file.size());
		}
		file = root.openNextFile();
	}
}

const char vowels[] = "aeiouy";					   // 6 vowels
const char consonants[] = "bcdfghjklmnpqrstvwxyz"; // 20 consonants
const int BASE = 120;							   // 6 vowels * 20 consonants
const int WORD_LENGTH = 5;						   // Required for uniqueness with 32-bit input

String generateSerialWord() {

	uint8_t mac[6];
	esp_read_mac(mac, ESP_MAC_WIFI_STA);
	uint32_t macValue = 0;

	// Use only the last 4 bytes of the MAC address
	for (int i = 2; i < 6; i++) {
		macValue = (macValue << 8) | mac[i];
	}

	String word = "";

	// Convert to base-120 pairs (consonant + vowel)
	for (int i = 0; i < WORD_LENGTH; i++) {
		int index = macValue % BASE; // Get remainder
		macValue /= BASE;			 // Reduce value

		word += consonants[index % 20]; // Pick a consonant
		word += vowels[index / 20];		// Pick a vowel
	}

	return word;
}
