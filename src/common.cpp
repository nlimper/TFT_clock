#include "common.h"
#include "Wire.h"
#include "config.h"
#include "esp_system.h"
#include <Arduino.h>
#include <BH1750.h>
#include <FS.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <esp_adc_cal.h>
#include <math.h>

BH1750 lightMeter(0x23); // 0x5C

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

const char vowels[] = "aeiouy";
const char consonants[] = "bcdfghjklmnpqrstvwxyz";
const int BASE = 120;
const int WORD_LENGTH = 5;

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
        int index = macValue % BASE;
        macValue /= BASE;

        word += consonants[index % 20];
        word += vowels[index / 20];
    }

    return word;
}


#define PHOTODIODE_ADC ((adc1_channel_t)(PHOTODIODE_PIN - 1))

void initLightmeter() {
    if (hardware.bh1750 && lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE_2)) {
        lightMeter.setMTreg(254);
        Serial.println(F("BH1750 found"));
        hasLightmeter = true;
    } else {
        Serial.println(F("BH1750 NOT found"));
    }
    if (hardware.photodiode) {
        adc1_config_width(ADC_WIDTH_BIT_12);
        adc1_config_channel_atten(PHOTODIODE_ADC, ADC_ATTEN_DB_12);

        pinMode(PHOTODIODE_PIN, INPUT_PULLDOWN);
        delay(10);
        uint16_t pulldownValue = adc1_get_raw(PHOTODIODE_ADC);
        pinMode(PHOTODIODE_PIN, INPUT_PULLUP);
        delay(10);
        uint16_t pullupValue = adc1_get_raw(PHOTODIODE_ADC);
        if (pulldownValue < 5 && pullupValue > 4090) {
            Serial.println(F("Photodiode NOT found"));
            pinMode(PHOTODIODE_PIN, INPUT);
        } else {
            Serial.println(F("Photodiode found"));
            pinMode(PHOTODIODE_PIN, INPUT_PULLDOWN);
            hasLightmeter = true;
        }
    }
}

// Function to read ADC with automatic attenuation and smoothing
struct AttenuationSetting {
    adc_atten_t atten;
    float vFullScale;
};

const AttenuationSetting attenuationLevels[] = {
    {ADC_ATTEN_DB_0, 0.75},
    {ADC_ATTEN_DB_2_5, 1.5},
    {ADC_ATTEN_DB_6, 2.2},
    {ADC_ATTEN_DB_12, 3.3}};

float readPhotodiode(adc1_channel_t channel) {
    int rawValue;
    float vFullScale;

    // Loop through attenuation levels from lowest to highest
    for (int i = 0; i < 4; i++) {
        adc1_config_channel_atten(channel, attenuationLevels[i].atten);
        rawValue = adc1_get_raw(channel);
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
        lux = readPhotodiode(PHOTODIODE_ADC) / 0.2115;  // approximation volt to lux
    } else {
        lux = nightmode ? 25 : 75;
    }
    return lux;
}