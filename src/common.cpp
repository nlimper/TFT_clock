#include "common.h"
#include "Wire.h"
#include "config.h"
#include "display.h"
#include "esp_system.h"
#include <Arduino.h>
#include <BH1750.h>
#include <FS.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <WiFi.h>
#include <math.h>

BH1750 lightMeter(0x23); // 0x5C

int perc2ledc(int brightness) {
    avgLux = constrain(avgLux, 0, 1000);
    const float gamma = 2.2f;
    const uint16_t min_output = prefs.getULong("minbrightness", 40);
    const uint16_t max_output = 4095;

    // 1. Normalize brightness (0-40 → 0.0-2.0)
    float brightness_normalized = (float)brightness / 20.0f; // 20=100% (1.0), 40=200% (2.0)
    brightness_normalized = constrain(brightness_normalized, 0.0f, 2.0f);

    // 2. Process ambient light (linear ratio)
    float ambient_ratio = (float)avgLux / config.luxfactor;

    // 3. Perceptual adjustments
    float brightness_gamma = pow(brightness_normalized, gamma);
    float ambient_gamma = pow(ambient_ratio, 0.8f); // Square root for ambient perception

    // 4. Combine factors
    float output_ratio = brightness_gamma * ambient_gamma;

    // 5. Map to output range (automatically clamps at max_output)
    int output = min_output + output_ratio * (max_output - min_output);
    output = constrain(output, min_output, max_output);

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
        setBrightness(channel, value);
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

const char vowels[] = "aeiouy";
const char consonants[] = "bcdfghjklmnpqrstvwxyz";
const int BASE = 120;
const int WORD_LENGTH = 5;

String generateSerialWord() {

    uint8_t mac[6];
    WiFi.macAddress(mac);
    uint32_t macValue = 0;

    // Use only the last 4 bytes of the MAC address
    for (int i = 2; i < 6; i++) {
        macValue = (macValue << 8) | mac[i];
    }

    String word = "";

    // Convert to base-120 pairs (consonant + vowel)
    for (int i = 0; i < WORD_LENGTH; i++) {
        int index = macValue % BASE;
        macValue /= BASE;

        word += consonants[index % 20];
        word += vowels[index / 20];
    }

    return word;
}

String parseDate(const char *dateStr) {
    const char *months = "JanFebMarAprMayJunJulAugSepOctNovDec";
    char monthStr[4];
    int day, year, monthIndex;
    sscanf(dateStr, "%s %d %d", monthStr, &day, &year);
    monthIndex = (strstr(months, monthStr) - months) / 3 + 1;
    char formattedDate[7];
    snprintf(formattedDate, sizeof(formattedDate), "%02d%02d%02d", year % 100, monthIndex, day);
    return String(formattedDate);
}

void initLightmeter() {
    if (hardware.bh1750 && lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE_2)) {
        lightMeter.setMTreg(254);
        Serial.println(F("BH1750 found"));
        hasLightmeter = true;
    } else {
        Serial.println(F("BH1750 NOT found"));
        hardware.bh1750 = false;
    }
    if (hardware.photodiode) {
        analogReadResolution(12);
        analogSetPinAttenuation(PHOTODIODE_PIN, ADC_11db);

        pinMode(PHOTODIODE_PIN, INPUT_PULLDOWN);
        delay(5);
        uint16_t pulldownValue = analogReadMilliVolts(PHOTODIODE_PIN);
        pinMode(PHOTODIODE_PIN, INPUT_PULLUP);
        delay(5);
        uint16_t pullupValue = analogReadMilliVolts(PHOTODIODE_PIN);
        pinMode(PHOTODIODE_PIN, INPUT);
        if (pulldownValue < 2 && pullupValue > 4093) {
            Serial.println(F("Photodiode NOT found"));
            hardware.photodiode = false;
        } else {
            Serial.println(F("Photodiode found"));
            hasLightmeter = true;
        }
    }
}

// Function to read ADC with automatic attenuation and smoothing
struct AttenuationSetting {
    adc_attenuation_t atten;
    float vFullScale;
};

const AttenuationSetting attenuationLevels[] = {
    {ADC_0db, 0.75},
    {ADC_2_5db, 1.5},
    {ADC_6db, 2.2},
    {ADC_11db, 3.3}};

float readPhotodiode(uint8_t pin) {
    int rawValue;
    float vFullScale;

    // Loop through attenuation levels from lowest to highest
    for (int i = 0; i < 4; i++) {
        analogSetPinAttenuation(pin, attenuationLevels[i].atten);
        rawValue = analogReadMilliVolts(pin);
        vFullScale = attenuationLevels[i].vFullScale;

        if (rawValue < 3800) { // Stop if value is within range
            break;
        }
    }

    // Convert raw ADC reading to voltage
    return (rawValue / 4095.0) * vFullScale;
}

float lightsensorRun() {
    float lux;
    if (hasLightmeter && hardware.bh1750) {
        if (lightMeter.measurementReady()) lux = lightMeter.readLightLevel();
    } else if (hasLightmeter && hardware.photodiode) {
        lux = readPhotodiode(PHOTODIODE_PIN) * 100;
    } else {
        lux = nightmode ? 25 : 75;
    }
    return lux;
}