#include "common.h"
#include "config.h"
#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <math.h>

int perc2ledc(int brightness) {
	avgLux = constrain(avgLux, 0, 1000);

	float exponent = 1.1;
	uint16_t min_output = prefs.getULong("minbrightness", 15);
	float max_output = 4095;

	float normalized_value = (float)brightness / 20.0;
	int output = constrain((float)min_output + (avgLux / config.luxfactor) * (int)((pow(normalized_value, exponent)) * (max_output - min_output)), 0, 4095);
	if (hardware.invertbacklight) output = 4095 - output;
	return output;
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
