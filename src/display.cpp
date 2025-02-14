#include "display.h"
#include "OpenFontRender.h"
#include "common.h"
#include <Arduino.h>
#include <algorithm>
#include <TFT_eSPI.h>
#include <TJpg_Decoder.h>

fs::File ttfFile;
TFT_eSprite spr = TFT_eSprite(&tft);

uint8_t digits[] = {DIGIT1, DIGIT2, DIGIT3, DIGIT4};
uint8_t backlight[] = {BACKLIGHT1, BACKLIGHT2, BACKLIGHT3, BACKLIGHT4};

bool jpgDraw(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap) {
    spr.pushImage(x, y, w, h, bitmap);
    return true;
}

void initTFT() {

    for (int pin : {DIGIT1, DIGIT2, DIGIT3, DIGIT4}) {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, HIGH);
    }

    pinMode(TFT_RST, OUTPUT);
    digitalWrite(TFT_RST, LOW);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    digitalWrite(TFT_RST, HIGH);
    vTaskDelay(200 / portTICK_PERIOD_MS);

    for (int pin : {DIGIT1, DIGIT2, DIGIT3, DIGIT4}) {
        digitalWrite(pin, LOW);
    }
    vTaskDelay(1 / portTICK_PERIOD_MS);
    tft.init();
    vTaskDelay(1 / portTICK_PERIOD_MS);
    tft.initDMA();
    tft.setRotation(0);
    tft.fillScreen(TFT_DARKGREEN);
    for (int pin : {DIGIT1, DIGIT2, DIGIT3, DIGIT4}) {
        digitalWrite(pin, HIGH);
    }

    ledcSetup(1, 9700, 12);
    ledcWrite(1, 2048);
    ledcSetup(2, 9700, 12);
    ledcWrite(2, 2048);
    vTaskDelay(1 / portTICK_PERIOD_MS);

    for (int i = 0; i < 4; i++) {
        selectScreen(i + 1, true);
        tft.fillScreen(TFT_RED);

        tft.setTextColor(TFT_YELLOW, TFT_RED);

        tft.setCursor(50, 50, 4);
        tft.println("display " + String(i + 1));
        tft.setRotation(2);
        tft.setCursor(50, 50, 4);
        tft.println("display " + String(i + 1));
        tft.setRotation(0);

        deselectScreen(i + 1);
        vTaskDelay(300 / portTICK_PERIOD_MS);
    }
}

void initTruetype() {
    truetype.setSerial(Serial);
    truetype.setDebugLevel(OFR_DEBUG_LEVEL::OFR_INFO);
    truetype.setDrawer(tft);
}

void loadTruetype(String fontFile, int fontSize) {
    static String currentFont = "";
    if (currentFont != fontFile) {
        time_t start = millis();
        truetype.unloadFont();
        String fontPath = "/truetype/" + fontFile;
        if (truetype.loadFont(fontPath.c_str())) {
            Serial.println("Truetype initialize error " + fontPath);
        }
        currentFont = fontFile;
        Serial.println("Font loaded in " + String(millis() - start) + "ms");
    }
    truetype.setFontSize(fontSize);
}

void initSprites(bool reInit) {
    d1 = 10;
    d2 = 10;
    d3 = 10;
    prevMinute = -1;

    if (TFT_WIDTH * TFT_HEIGHT > 320 * 240) return;

    currentFont = prefs.getUShort("font", 0);
    Serial.println("font: " + String(currentFont) + " " + String(fonts[currentFont].file));

    if (String(fonts[currentFont].file).endsWith(".ttf")) {

        currentColor = prefs.getUShort("color", 0);
        int fontr = nightmode ? 255 : colors[currentColor].r;
        int fontg = nightmode ? 0 : colors[currentColor].g;
        int fontb = nightmode ? 0 : colors[currentColor].b;

        loadTruetype(fonts[currentFont].file, fonts[currentFont].size);
        truetype.setFontColor(fontr, fontg, fontb);
        truetype.setAlignment(Align::Center);

        int posx = TFT_WIDTH / 2 + fonts[currentFont].posX;
        int posy = TFT_HEIGHT / 2 + fonts[currentFont].posY - fonts[currentFont].size / 2;

        for (int i = 0; i < 10; i++) {
            sprites[i].setColorDepth(16);
            sprites[i].createSprite(TFT_WIDTH, TFT_HEIGHT);
            if (sprites[i].created()) {
                sprites[i].fillSprite(TFT_BLACK);
                truetype.setDrawer(sprites[i]);
                truetype.setCursor(posx, posy);
                truetype.printf("%d", i);

            } else {
                Serial.println("Failed to create sprite " + String(i));
            }
        }
    } else {

        TJpgDec.setJpgScale(1);
        TJpgDec.setSwapBytes(true);
        TJpgDec.setCallback(jpgDraw);
        spr.setColorDepth(16);
        spr.createSprite(TFT_WIDTH, TFT_HEIGHT);
        if (spr.getPointer() == nullptr) {
            Serial.println("Failed to create sprite in initSprites");
            return;
        }
        for (int i = 0; i < 10; i++) {
            sprites[i].setColorDepth(16);
            sprites[i].createSprite(TFT_WIDTH, TFT_HEIGHT);
            if (sprites[i].created()) {
                sprites[i].fillSprite(TFT_BLACK);
                TJpgDec.drawFsJpg(0, 0, fonts[currentFont].file + String(i) + ".jpg", *contentFS);
                memcpy(sprites[i].getPointer(), spr.getPointer(), spr.width() * spr.height() * 2);
            } else {
                Serial.println("Failed to create sprite " + String(i));
            }
        }
        spr.deleteSprite();
    }
}

void clearScreen(uint8_t digitId, bool enableBacklight) {
    uint8_t screenId = flipOrientation ? 3 - (digitId - 1) : digitId - 1;
    if (enableBacklight) {
        ledcAttachPin(backlight[screenId], 1);
    } else {
        ledcDetachPin(backlight[screenId]);
        digitalWrite(backlight[screenId], hardware.invertbacklight ? HIGH : LOW);
    }
    digitalWrite(digits[screenId], LOW);
    vTaskDelay(1 / portTICK_PERIOD_MS);
    tft.fillScreen(TFT_BLACK);
    digitalWrite(digits[screenId], HIGH);
};

void selectScreen(uint8_t digitId, bool enableBacklight) {
    uint8_t screenId = flipOrientation ? 3 - (digitId - 1) : digitId - 1;
    if (enableBacklight) {
        ledcAttachPin(backlight[screenId], 1);
    } else {
        ledcDetachPin(backlight[screenId]);
        digitalWrite(backlight[screenId], hardware.invertbacklight ? HIGH : LOW);
    }
    digitalWrite(digits[screenId], LOW);
    vTaskDelay(1 / portTICK_PERIOD_MS);
    tft.setRotation(flipOrientation ? 2 : 0);
};

void deselectScreen(uint8_t digitId) {
    uint8_t screenId = flipOrientation ? 3 - (digitId - 1) : digitId - 1;
    digitalWrite(digits[screenId], HIGH);
};

void showRaster() {
    // debug screen position
    for (int j = 0; j < TFT_HEIGHT / 10; j++) {
        tft.drawLine(0, j * 10, TFT_WIDTH, j * 10, TFT_DARKGREY);
    }
    for (int j = 0; j < TFT_WIDTH / 10; j++) {
        tft.drawLine(j * 10, 0, j * 10, TFT_HEIGHT, TFT_DARKGREY);
    }
}

uint16_t rgb332ToTFT(uint8_t r, uint8_t g, uint8_t b) {
    // Scale RGB332 to 5 bits for red, 6 bits for green, and 5 bits for blue
    uint16_t tftColor = (r >> 3) << 11 | (g >> 2) << 5 | (b >> 3);
    return tftColor;
}

void showColors() {
    uint8_t squareSize = 16; // Size of each square (16x16)

    // Loop through all 256 RGB332 colors
    int colorIndex = 0;
    for (int i = 0; i < 8; i++) {         // 8 levels of red
        for (int j = 0; j < 8; j++) {     // 8 levels of green
            for (int k = 0; k < 4; k++) { // 4 levels of blue
                // Calculate the RGB332 color
                uint8_t r = i * 36; // Red levels
                uint8_t g = j * 36; // Green levels
                uint8_t b = k * 85; // Blue levels

                // Get the TFT color equivalent
                uint16_t color = rgb332ToTFT(r, g, b);

                // Calculate position for the square
                int x = i * squareSize;                             // 20 squares per row
                int y = j * squareSize + k * (8 * squareSize + 10); // 30 squares per column

                // Draw the square with the calculated color
                tft.fillRect(x, y, squareSize, squareSize, color);

                colorIndex++; // Move to the next color
            }
        }
    }
}

void drawDigit(uint8_t digit, bool useSprite) {
    if (sprites[digit].created() && useSprite) {
        sprites[digit].pushSprite(0, 0);
    } else {
        auto &c = colors[prefs.getUShort("color", 0)];
        int fontr = c.r, fontg = c.g, fontb = c.b;

        if (nightmode && useSprite) {
            fontr = 255;
            fontg = 0;
            fontb = 0;
        }

        int fonttemp = prefs.getUShort("font", 0);
        if (String(fonts[fonttemp].file).endsWith(".ttf")) {

            int posx = TFT_WIDTH / 2 + fonts[fonttemp].posX;
            int posy = TFT_HEIGHT / 2 + fonts[fonttemp].posY - fonts[fonttemp].size / 2;
            TFT_eSprite digitspr = TFT_eSprite(&tft);
            digitspr.setColorDepth(16);
            digitspr.createSprite(TFT_WIDTH, TFT_HEIGHT);
            if (digitspr.created()) {
                digitspr.fillSprite(TFT_BLACK);
                truetype.setDrawer(digitspr);
            } else {
                tft.fillScreen(TFT_BLACK);
                truetype.setDrawer(tft);
            }
            loadTruetype(fonts[fonttemp].file, fonts[fonttemp].size);
            truetype.setFontColor(fontr, fontg, fontb);
            truetype.setAlignment(Align::Center);
            truetype.setCursor(posx, posy);
            truetype.printf("%d", digit);
            if (digitspr.created()) {
                digitspr.pushSprite(0, 0);
                digitspr.deleteSprite();
            }

        } else {

            TJpgDec.setJpgScale(1);
            TJpgDec.setSwapBytes(true);
            TJpgDec.setCallback(jpgDraw);

            spr.setColorDepth(16);
            spr.createSprite(TFT_WIDTH, TFT_HEIGHT);
            if (spr.getPointer() == nullptr) {
                Serial.println("Failed to create sprite in drawDigit");
            } else {
                TJpgDec.drawFsJpg(0, 0, fonts[fonttemp].file + String(digit) + ".jpg", *contentFS);
                spr.pushSprite(0, 0);
                spr.deleteSprite();
            }
        }
    }
}

void showAlarmIcon(uint16_t nextAlarm) {
    int fontr = std::clamp(static_cast<int>(colors[currentColor].r * 1.5), 0, 255);
    int fontg = std::clamp(static_cast<int>(colors[currentColor].g * 1.5), 0, 255);
    int fontb = std::clamp(static_cast<int>(colors[currentColor].b * 1.5), 0, 255);

    if (nightmode) {
        fontr = 255;
        fontg = 0;
        fontb = 0;
    }
    // https://www.iconsdb.com/black-icons/alarm-clock-icon.html
    // https://notisrac.github.io/FileToCArray/
    const unsigned char alarmIcon[] PROGMEM =
        {
            0xf3, 0xe7, 0xcf, 0xc1, 0xc3, 0x83, 0x83, 0xff, 0xc1, 0x87, 0x00, 0xe1, 0x0e, 0x00, 0x30, 0x98, 0x24, 0x19, 0xb0, 0xe7, 0x0d, 0xf1, 0xe7, 0x87, 0xe3, 0xe7, 0xc7, 0xe7, 0xe7, 0xe7, 0xc7, 0xe7, 0xe3, 0xc7, 0xe7, 0xe3, 0xc3, 0xe7, 0xc3, 0xc3, 0xe3, 0xc3, 0xc7, 0xf1, 0xf3, 0xc7, 0xfc, 0xe3, 0xe7, 0xfe, 0xe7, 0xe3, 0xff, 0xc7, 0xf1, 0xff, 0x8f, 0xf0, 0xe7, 0x0f, 0xfc, 0x00, 0x1f, 0xfc, 0x00, 0x3f, 0xfd, 0x81, 0xbf, 0xf9, 0xff, 0x9f};
    uint16_t labelBG = tft.color565(15, 10, 5);
    tft.fillSmoothRoundRect(layoutNextalarmX, layoutNextalarmY, 130, 30, 5, labelBG, TFT_BLACK);
    tft.drawSmoothRoundRect(layoutNextalarmX, layoutNextalarmY, 5, 5, 130, 30, tft.color565(fontr, fontg, fontb), TFT_BLACK);
    tft.drawBitmap(layoutNextalarmX + 10, layoutNextalarmY + 4, alarmIcon, 24, 24, labelBG, tft.color565(fontr, fontg, fontb));
    tft.setTextColor(tft.color565(fontr, fontg, fontb), labelBG);
    tft.setTextDatum(TL_DATUM);
    tft.loadFont("/dejavusanscond24", *contentFS);
    tft.drawString(formatTime(nextAlarm), layoutNextalarmX + 40, layoutNextalarmY + 5);
    tft.unloadFont();
}

void debugTFT(String message) {
    selectScreen(1);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.fillRect(0, 105, 240, 26, TFT_BLACK);
    tft.setCursor(40, 110, 2);
    tft.println(message);
    deselectScreen(1);
}
FT_FILE *OFR_fopen(const char *filename, const char *mode) {
    ttfFile = contentFS->open(filename, mode);
    return &ttfFile;
}

void OFR_fclose(FT_FILE *stream) {
    ((File *)stream)->close();
}

size_t OFR_fread(void *ptr, size_t size, size_t nmemb, FT_FILE *stream) {
    return ((File *)stream)->read((uint8_t *)ptr, size * nmemb);
}

int OFR_fseek(FT_FILE *stream, long int offset, int whence) {
    return ((File *)stream)->seek(offset, (SeekMode)whence);
}

long int OFR_ftell(FT_FILE *stream) {
    return ((File *)stream)->position();
}
