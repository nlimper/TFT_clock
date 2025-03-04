
#include "display.h"
#include "OpenFontRender.h"
#include "common.h"
#include <Arduino.h>
#include <JPEGDEC.h>
#include <TFT_eSPI.h>
#include <algorithm>

fs::File ttfFile;
TFT_eSprite spr = TFT_eSprite(&tft);
JPEGDEC jpeg;

uint8_t digits[] = {DIGIT1, DIGIT2, DIGIT3, DIGIT4};
uint8_t backlight[] = {BACKLIGHT1, BACKLIGHT2, BACKLIGHT3, BACKLIGHT4};
int timerNotificationId;
bool isAttached[4] = {false, false, false, false};

// Custom I/O callbacks for JPEGDEC
static void *myOpen(const char *filename, int32_t *size) {
    File *file = new File(contentFS->open(filename, "r")); // Use your filesystem (e.g., SD, SPIFFS)
    if (!file || !*file) return nullptr;
    *size = file->size();
    return (void *)file;
}

static void myClose(void *handle) {
    File *file = (File *)handle;
    if (file) file->close();
}

static int32_t myRead(JPEGFILE *handle, uint8_t *buf, int32_t len) {
    File *file = (File *)handle->fHandle;
    return file->read(buf, len);
}

static int32_t mySeek(JPEGFILE *handle, int32_t pos) {
    File *file = (File *)handle->fHandle;
    return file->seek(pos) ? pos : -1;
}

int JPEGDraw(JPEGDRAW *pDraw) {
    spr.pushImage(pDraw->x, pDraw->y, pDraw->iWidth, pDraw->iHeight, pDraw->pPixels);
    return 1;
}

const uint8_t resolution = 12;
const uint16_t freq = 9700;

void initTFT() {

    for (int pin : {DIGIT1, DIGIT2, DIGIT3, DIGIT4}) {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, HIGH);
    }

    pinMode(TFT_RST, OUTPUT);
    digitalWrite(TFT_RST, LOW);
    vTaskDelay(50 / portTICK_PERIOD_MS);
    digitalWrite(TFT_RST, HIGH);
    vTaskDelay(250 / portTICK_PERIOD_MS);

    for (int pin : {DIGIT1, DIGIT2, DIGIT3, DIGIT4}) {
        digitalWrite(pin, LOW);
    }
    vTaskDelay(1 / portTICK_PERIOD_MS);
    tft.init();
    vTaskDelay(1 / portTICK_PERIOD_MS);
    tft.initDMA();
    tft.setRotation(0);
    tft.fillScreen(TFT_BLACK);
    vTaskDelay(1 / portTICK_PERIOD_MS);
    for (int pin : {DIGIT1, DIGIT2, DIGIT3, DIGIT4}) {
        digitalWrite(pin, HIGH);
    }
#if ESP_ARDUINO_VERSION_MAJOR == 2
    ledcSetup(1, freq, resolution);
    ledcSetup(2, freq, resolution);
#endif

    for (int i = 0; i < 4; i++) {
        selectScreen(i + 1, true);
        tft.fillScreen(TFT_MAROON);

        tft.setTextColor(TFT_YELLOW, TFT_MAROON);
        tft.setCursor(50, 50, 4);
        tft.println("display " + String(i + 1));
        tft.setRotation(2);
        tft.setCursor(50, 50, 4);
        tft.println("display " + String(i + 1));
        tft.setRotation(0);
        setBrightness(1, 2048);

        deselectScreen(i + 1);
        vTaskDelay(350 / portTICK_PERIOD_MS);
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
    currentFont = prefs.getUShort("font", 0);
    currentColor = prefs.getUShort("color", 0);

    if (TFT_WIDTH * TFT_HEIGHT > 320 * 240) return;

    int fontr = nightmode ? 255 : colors[currentColor].r;
    int fontg = nightmode ? 0 : colors[currentColor].g;
    int fontb = nightmode ? 0 : colors[currentColor].b;

    Serial.println("font: " + String(currentFont) + " " + String(fonts[currentFont].file));
    time_t t = millis();
    if (String(fonts[currentFont].file).endsWith(".ttf")) {

        loadTruetype(fonts[currentFont].file, fonts[currentFont].size);
        truetype.setFontColor(fontr, fontg, fontb);
        truetype.setAlignment(Align::Center);

        int posx = TFT_WIDTH / 2 + fonts[currentFont].posX;
        int posy = TFT_HEIGHT / 2 + fonts[currentFont].posY - fonts[currentFont].size / 2;

        for (int i = 0; i < 10; i++) {
            if (TFT_WIDTH * TFT_HEIGHT > 320 * 240) {
                // 1 bit color depth works, but no anti-aliasing so less beautiful
                sprites[i].setColorDepth(1);
                sprites[i].setBitmapColor(tft.color565(fontr, fontg, fontb), TFT_BLACK);
            } else {
                sprites[i].setColorDepth(16);
            }
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

        spr.setColorDepth(16);
        spr.createSprite(TFT_WIDTH, TFT_HEIGHT);
        spr.setSwapBytes(true);
        if (spr.getPointer() == nullptr) {
            Serial.println("Failed to create sprite in initSprites");
            return;
        }
        for (int i = 0; i < 10; i++) {
            sprites[i].setColorDepth(16);
            sprites[i].createSprite(TFT_WIDTH, TFT_HEIGHT);
            if (sprites[i].created()) {
                String path = fonts[currentFont].file + String(i) + ".jpg";
                if (jpeg.open((const char *)path.c_str(), myOpen, myClose, myRead, mySeek, JPEGDraw)) {
                    jpeg.decode(0, 0, 0);
                    jpeg.close();
                }
                // spr.pushSprite(0, 0);
                memcpy(sprites[i].getPointer(), spr.getPointer(), spr.width() * spr.height() * 2);
            } else {
                Serial.println("Failed to create sprite " + String(i));
            }
        }
        spr.deleteSprite();
    }
    Serial.println("Sprites loaded in " + String(millis() - t) + "ms");
}

void clearScreen(uint8_t digitId, bool enableBacklight) {
    uint8_t screenId = flipOrientation ? 3 - (digitId - 1) : digitId - 1;
    if (enableBacklight) {
        if (!isAttached[screenId]) addPWM(backlight[screenId], 1);
        isAttached[screenId] = true;
    } else {
        if (isAttached[screenId]) {
            removePWM(backlight[screenId]);
            pinMode(backlight[screenId], OUTPUT);
            digitalWrite(backlight[screenId], hardware.invertbacklight ? HIGH : LOW);
        }
        isAttached[screenId] = false;
    }
    digitalWrite(digits[screenId], LOW);
    vTaskDelay(1 / portTICK_PERIOD_MS);
    tft.fillScreen(TFT_BLACK);
    digitalWrite(digits[screenId], HIGH);
};

void selectScreen(uint8_t digitId, bool enableBacklight) {
    uint8_t screenId = flipOrientation ? 3 - (digitId - 1) : digitId - 1;
    if (enableBacklight) {
        if (!isAttached[screenId]) addPWM(backlight[screenId], 1);
        isAttached[screenId] = true;
    } else {
        if (isAttached[screenId]) {
            removePWM(backlight[screenId]);
            pinMode(backlight[screenId], OUTPUT);
            digitalWrite(backlight[screenId], hardware.invertbacklight ? HIGH : LOW);
        }
        isAttached[screenId] = false;
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
    time_t t = millis();
    if (sprites[digit].created() && useSprite) {
        sprites[digit].pushSprite(0, 0);
        Serial.println("draw sprite " + String(millis() - t) + "ms");
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
            Serial.println("loadTruetype " + String(millis() - t) + "ms");
            truetype.setFontColor(fontr, fontg, fontb);
            truetype.setAlignment(Align::Center);
            truetype.setCursor(posx, posy);
            truetype.printf("%d", digit);
            Serial.println("printf " + String(millis() - t) + "ms");
            if (digitspr.created()) {
                digitspr.pushSprite(0, 0);
                digitspr.deleteSprite();
            }
            Serial.println("finished " + String(millis() - t) + "ms");

        } else {

            spr.setColorDepth(16);
            spr.createSprite(TFT_WIDTH, TFT_HEIGHT);
            spr.setSwapBytes(true);
            if (spr.getPointer() == nullptr) {
                Serial.println("Failed to create sprite in drawDigit");
            } else {

                String path = fonts[fonttemp].file + String(digit) + ".jpg";
                if (jpeg.open((const char *)path.c_str(), myOpen, myClose, myRead, mySeek, JPEGDraw)) {
                    jpeg.decode(0, 0, 0);
                    jpeg.close();
                }
                Serial.println("load jpg " + String(millis() - t) + "ms");
                spr.pushSprite(0, 0);
                spr.deleteSprite();
            }
            Serial.println("finished " + String(millis() - t) + "ms");
        }
    }
}

void showAlarmIcon(uint16_t nextAlarm) {
    int fontr = nightmode ? 255 : std::clamp(static_cast<int>(colors[currentColor].r * 1.5), 0, 255);
    int fontg = nightmode ? 0 : std::clamp(static_cast<int>(colors[currentColor].g * 1.5), 0, 255);
    int fontb = nightmode ? 0 : std::clamp(static_cast<int>(colors[currentColor].b * 1.5), 0, 255);

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

void showNotification(String text) {
    int fontr = nightmode ? 255 : std::clamp(static_cast<int>(colors[currentColor].r * 1.5), 0, 255);
    int fontg = nightmode ? 0 : std::clamp(static_cast<int>(colors[currentColor].g * 1.5), 0, 255);
    int fontb = nightmode ? 0 : std::clamp(static_cast<int>(colors[currentColor].b * 1.5), 0, 255);

    // https://www.iconsdb.com/black-icons/info-2-icon.html (24x24 bitmap)
    // https://notisrac.github.io/FileToCArray/
    const unsigned char infoIcon[] PROGMEM = {
        0xff, 0x81, 0xff, 0xfc, 0x00, 0x3f, 0xf8, 0x1c, 0x1f, 0xf1, 0xff, 0x8f, 0xe3, 0xff, 0xc7, 0xc7, 0xe7, 0xe3, 0x8f, 0xc3, 0xf1, 0x8f, 0xc3, 0xf9, 0x9f, 0xe7, 0xf9, 0x1f, 0xff, 0xf8, 0x1f, 0xff, 0xfc, 0x1f, 0xc3, 0xfc, 0x1f, 0xc3, 0xfc, 0x1f, 0xc3, 0xfc, 0x1f, 0xc3, 0xf8, 0x9f, 0xc3, 0xf9, 0x8f, 0xc3, 0xf9, 0xcf, 0xc3, 0xf1, 0xc7, 0xef, 0xe3, 0xe3, 0xff, 0xc7, 0xf1, 0xff, 0x8f, 0xf8, 0x3c, 0x1f, 0xfc, 0x00, 0x3f, 0xff, 0x81, 0xff};
    uint16_t labelBG = tft.color565(15, 10, 5);
    selectScreen(1);
    tft.fillSmoothRoundRect(5, TFT_HEIGHT / 2 - 15, TFT_WIDTH - 10, 30, 5, labelBG, TFT_BLACK);
    tft.drawSmoothRoundRect(5, TFT_HEIGHT / 2 - 15, 5, 5, TFT_WIDTH - 10, 30, tft.color565(fontr, fontg, fontb), TFT_BLACK);
    tft.drawBitmap(5 + 5, TFT_HEIGHT / 2 - 15 + 4, infoIcon, 24, 24, labelBG, tft.color565(fontr, fontg, fontb));
    tft.setTextColor(tft.color565(fontr, fontg, fontb), labelBG);
    tft.setTextDatum(TC_DATUM);
    tft.loadFont("/dejavusanscond24", *contentFS);
    tft.drawString(text, TFT_WIDTH / 2 + 14, TFT_HEIGHT / 2 - 15 + 5);
    tft.unloadFont();
    deselectScreen(1);
    if (timerNotificationId) timer.deleteTimer(timerNotificationId);
    timerNotificationId = timer.setTimer(1000, resetNotification, 1);
}

void resetNotification() {
    d1 = 10;
    prevMinute = -1;
}

void setBrightness(uint8_t channel, uint32_t value) {
#if ESP_ARDUINO_VERSION_MAJOR == 2
    ledcWrite(channel, hardware.invertbacklight ? 4095 - value : value);
#else
    ledcWriteChannel(channel, hardware.invertbacklight ? 4095 - value : value);
#endif
}

void removePWM(uint8_t pin) {
#if ESP_ARDUINO_VERSION_MAJOR == 2
    ledcDetachPin(pin);
#else
    ledcDetach(pin);
#endif
}

void addPWM(uint8_t pin, uint8_t channel) {
#if ESP_ARDUINO_VERSION_MAJOR == 2
    ledcAttachPin(pin, channel);
#else
    ledcAttachChannel(pin, freq, resolution, channel);
#endif
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
