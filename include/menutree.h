#include <Arduino.h>
#include <LittleFS.h>
#include <FS.h>
#include <Preferences.h>
#include <TFT_eSPI.h>
#include <functional>
#include <map>
#include <vector>

extern TFT_eSPI tft;
extern fs::FS *contentFS;
extern int d1, d2, d3, minuutoud;
extern uint32_t lastmenuactive;
extern uint16_t alarm_set[8];
extern Preferences prefs;

#ifndef MENUTREEH
#define MENUTREEH
struct MenuItem {
	uint16_t id;					   // Unique ID of the menu item
	String name;			   // Display name of the item
	uint16_t parentId;			   // ID of the parent item (-1 for root)
	bool hasChildren;		   // True if item has children (is a submenu)
	String functionName;
	String infoTxt;
};

enum MenuState {
	OFF = 0,
	PREVIEW = 1,
	MENU = 2
};
#endif

void drawMenu(uint16_t parentId, uint16_t activeItem, uint8_t menuLevel);
void initMenu();
void exitmenu();
void handleMenuInput(int button);
void handleMenuInput(int button, int stepSize);
void handleMenuHold(int button);

String getValue(const String &name);
void doFunction(String &name, int8_t increment);
void showValue(String value, uint8_t menuLevel, bool initialise = false);
