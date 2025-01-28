#include "display.h"
#include "OpenFontRender.h"
#include <Arduino.h>
#include <TFT_eSPI.h>

fs::File ttfFile;

uint8_t digits[] = {DIGIT1, DIGIT2, DIGIT3, DIGIT4};
uint8_t backlight[] = {BACKLIGHT1, BACKLIGHT2, BACKLIGHT3, BACKLIGHT4};

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
	tft.setRotation(0);
	tft.fillScreen(TFT_DARKGREEN);
	for (int pin : {DIGIT1, DIGIT2, DIGIT3, DIGIT4}) {
		digitalWrite(pin, HIGH);
	}

	ledcSetup(1, 5000, 12);
	ledcWrite(1, 2048);
	ledcSetup(2, 5000, 12);
	ledcWrite(2, 2048);
	vTaskDelay(1 / portTICK_PERIOD_MS);

	for (int i = 0; i < 4; i++) {
		selectScreen(i + 1, true);
		tft.fillScreen(TFT_RED);
		tft.setTextColor(TFT_YELLOW, TFT_RED);
		tft.setCursor(50, 50, 2);
		tft.println("display " + String(i + 1));
		deselectScreen(i + 1);
		vTaskDelay(200 / portTICK_PERIOD_MS);
	}
	vTaskDelay(3000 / portTICK_PERIOD_MS);
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
	if (TFT_WIDTH * TFT_HEIGHT > 320 * 240) return;

	currentColor = prefs.getUShort("color", 0);
	int fontr = nightmode ? 255 : colors[currentColor].r;
	int fontg = nightmode ? 0 : colors[currentColor].g;
	int fontb = nightmode ? 0 : colors[currentColor].b;

	currentFont = prefs.getUShort("font", 0);
	Serial.println("font: " + String(currentFont) + " " + String(fonts[currentFont].file));
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
    uint8_t squareSize = 16;  // Size of each square (16x16)

    // Loop through all 256 RGB332 colors
    int colorIndex = 0;
    for (int i = 0; i < 8; i++) {        // 8 levels of red
        for (int j = 0; j < 8; j++) {    // 8 levels of green
            for (int k = 0; k < 4; k++) { // 4 levels of blue
                // Calculate the RGB332 color
                uint8_t r = i * 36;      // Red levels
                uint8_t g = j * 36;      // Green levels
                uint8_t b = k * 85;      // Blue levels
                
                // Get the TFT color equivalent
                uint16_t color = rgb332ToTFT(r, g, b);
                
                // Calculate position for the square
                int x = i * squareSize;  // 20 squares per row
                int y = j * squareSize + k * (8 * squareSize + 10);  // 30 squares per column
                
                // Draw the square with the calculated color
                tft.fillRect(x, y, squareSize, squareSize, color);
                
                colorIndex++;  // Move to the next color
            }
        }
    }
}

void drawDigit(uint8_t digit, bool useSprite) {
	if (sprites[digit].created() && useSprite) {
		sprites[digit].pushSprite(0, 0);
	} else {
		currentColor = prefs.getUShort("color", 0);
		int fontr = colors[currentColor].r;
		int fontg = colors[currentColor].g;
		int fontb = colors[currentColor].b;

		if (nightmode && useSprite) {
			fontr = 255;
			fontg = 0;
			fontb = 0;
		}

		int fonttemp = prefs.getUShort("font", 0);
		int posx = TFT_WIDTH / 2 + fonts[fonttemp].posX;
		int posy = TFT_HEIGHT / 2 + fonts[fonttemp].posY - fonts[fonttemp].size / 2;
		TFT_eSprite spr = TFT_eSprite(&tft);
		spr.setColorDepth(16);
		spr.createSprite(TFT_WIDTH, TFT_HEIGHT);
		if (spr.created()) {
			spr.fillSprite(TFT_BLACK);
			truetype.setDrawer(spr);
		} else {
			tft.fillScreen(TFT_BLACK);
			truetype.setDrawer(tft);
		}
		loadTruetype(fonts[fonttemp].file, fonts[fonttemp].size);
		truetype.setFontColor(fontr, fontg, fontb);
		truetype.setAlignment(Align::Center);
		truetype.setCursor(posx, posy);
		truetype.printf("%d", digit);
		if (spr.created()) {
			spr.pushSprite(0, 0);
			spr.deleteSprite();
		}
	}
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
