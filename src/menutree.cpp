#include "menutree.h"
#include "sound.h"
#include "common.h"
#include "display.h"
#include "timefunctions.h"
#include "web.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <TFT_eSPI.h>
#include <algorithm>
#include <functional>
#include <map>
#include <vector>

extern MenuState menustate;
std::vector<MenuItem> menuItems;
uint16_t activeItemId = 0;
uint16_t currentMenuId = 0;
uint8_t menuLevel = 0;
bool menuActive = false, inFunction = false;
extern uint16_t time_set[6];
extern bool hasLightmeter;
uint16_t idCount = 0;

void readMenu(JsonArray menuArray, uint16_t parentId = -1) {
    for (JsonObject menuObject : menuArray) {
        String name = menuObject["name"] | "";
        String functionName = menuObject["function"] | "";
        String infoTxt = menuObject["info"] | "";

        MenuItem menuItem;
        menuItem.id = idCount++;
        menuItem.name = name;
        menuItem.parentId = parentId;
        menuItem.hasChildren = menuObject["children"].is<JsonArray>();
        menuItem.functionName = functionName;
        menuItem.infoTxt = infoTxt;
        menuItems.push_back(menuItem);

        /*
                Serial.print("ID: "); Serial.print(menuItem.id);
                Serial.print(", Name: "); Serial.print(menuItem.name);
                Serial.print(", Parent ID: "); Serial.print(menuItem.parentId);
                Serial.print(", Has Children: "); Serial.print(menuItem.hasChildren);
                Serial.print(", Function Name: "); Serial.println(menuItem.functionName);
        */

        if (menuItem.hasChildren) {
            JsonArray childrenArray = menuObject["children"].as<JsonArray>();
            readMenu(childrenArray, menuItem.id);
        }
    }
}

void drawMenu(uint16_t parentId, uint16_t activeItem, uint8_t menuLevel) {
    spr.setColorDepth(16);
    spr.createSprite(TFT_WIDTH, TFT_HEIGHT);
    if (!spr.created()) {
        spr.setAttribute(PSRAM_ENABLE, false);
        spr.createSprite(TFT_WIDTH, TFT_HEIGHT);
    }
    if (!spr.created()) {
        Serial.println("Failed to create sprite for drawmenu");
        return;
    }
    lastmenuactive = millis();
    selectScreen(menuLevel + 1);
    spr.fillScreen(TFT_BLACK);
    int menuPosY = 35;
    spr.drawLine(0, menuPosY, TFT_WIDTH, menuPosY, tft.color565(100, 100, 100));
    spr.loadFont("/dejavusanscond15", *contentFS);
    spr.setTextWrap(false);
    int menuLine = 0;
    for (int i = 0; i < menuItems.size(); i++) {
        MenuItem &item = menuItems[i];
        if (item.parentId == parentId && (item.functionName != "exitMenu" || buttonCount <= 3)) {
            spr.setCursor(35, menuPosY + 3 + menuLine * 18);
            if (i == activeItem) {
                spr.setTextColor(spr.color565(255, 255, 0), TFT_BLACK);
            } else {
                spr.setTextColor(spr.color565(150, 150, 150), TFT_BLACK);
            }
            spr.println(menuItems[i].name);

            if (!item.functionName.isEmpty()) {
                String value = getValue(item.functionName);
                if (value != "") {
                    spr.fillRect(130, menuPosY + 3 + menuLine * 18, TFT_WIDTH - 130, 17, spr.color565(40, 40, 40));
                    spr.setCursor(132, menuPosY + 3 + menuLine * 18);
                    spr.setTextColor(spr.color565(240, 240, 50), spr.color565(40, 40, 40));
                    spr.println(value);
                } else {
                    spr.fillRect(130, menuPosY + 3 + menuLine * 18, TFT_WIDTH - 130, 17, TFT_BLACK);
                }
            }
            menuLine++;
        }
    }
    spr.drawLine(0, menuPosY + 3 + menuLine * 18, TFT_WIDTH, menuPosY + 3 + menuLine * 18, tft.color565(100, 100, 100));
    spr.unloadFont();
    spr.pushSprite(0, 0);
    spr.deleteSprite();
    deselectScreen(menuLevel + 1);
    currentMenuId = parentId;
    activeItemId = activeItem;
}

void initMenu() {
    File file = contentFS->open(JSON_FILE_PATH, "r");
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    if (error) {
        Serial.print("JSON deserialization failed: ");
        Serial.println(error.c_str());
        return;
    }
    JsonArray menuJson = doc["menu"].as<JsonArray>();
    if (menuJson.isNull()) {
        Serial.println("No 'menu' object found in JSON");
        return;
    }
    readMenu(menuJson);
    menuActive = false;
}

void exitmenu() {
    tft.unloadFont();
    spr.unloadFont();
    spr.deleteSprite();
    d1 = 10;
    d2 = 10;
    d3 = 10;
    prevMinute = -1;
    menuActive = false;
    menustate = OFF;
}

int findChild(uint16_t parentId, uint16_t index) {
    std::vector<int> childIds;
    for (const auto &item : menuItems) {
        if (item.parentId == parentId && (item.functionName != "exitMenu" || buttonCount <= 3)) {
            childIds.push_back(item.id);
        }
    }
    if (index >= 0 && index < childIds.size()) {
        return childIds[index];
    } else {
        Serial.println("Error: Index out of bounds or no children found");
        return -1;
    }
}

void selectMenuItem(uint16_t menuId) {
    MenuItem &item = menuItems[menuId];
    if (item.hasChildren) {
        activeItemId = findChild(menuId, 0);
        menuLevel++;
        drawMenu(menuId, activeItemId, menuLevel);
    } else if (!item.functionName.isEmpty()) {
        doFunction(item.functionName, 0);
    }
}

std::tuple<int, int, std::vector<int>> getSiblingInfo(
    const std::vector<MenuItem> &menuItems,
    uint16_t currentMenuId,
    uint16_t activeItemId) {

    std::vector<int> childIds;

    for (const auto &item : menuItems) {
        if (item.parentId == currentMenuId && (item.functionName != "exitMenu" || buttonCount <= 3)) {
            childIds.push_back(item.id);
        }
    }

    int siblingCount = static_cast<int>(childIds.size());

    int siblingIndex = -1;
    auto it = std::find(childIds.begin(), childIds.end(), activeItemId);
    if (it != childIds.end()) {
        siblingIndex = static_cast<int>(std::distance(childIds.begin(), it));
    }

    if (siblingIndex == -1) {
        Serial.println("Warning: activeItemId not found among siblings.");
    }

    return {siblingCount, siblingIndex, childIds};
}

void handleRotaryOutsideMenu(int increment) {
    if (hardware.rotary) {
        if (audioStreaming()) {
            // adjust volume
            uint16_t volume = prefs.getUShort("volume", 50);
            volume = std::clamp(volume + 5 * increment, 0, 100);
            audioVolume(volume);
            prefs.putUShort("volume", volume);
            Serial.println("Volume: " + String(volume) + "%");
            showNotification("Volume: " + String(volume) + "%");
        } else {
            // adjust brightness
            uint16_t brightness = prefs.getUShort("brightness", 15);
            brightness = std::clamp(brightness + increment, 0, hasLightmeter ? 40 : 20);
            prefs.putUShort("brightness", brightness);
            Serial.println("Brightness: " + String(brightness * 5) + "%");
            showNotification("Brightness: " + String(brightness * 5) + "%");
        }
    }
}

void handleMenuInput(int button) {
    handleMenuInput(button, 1);
}

void handleMenuInput(int button, int stepSize) {
    MenuItem &currentItem = menuItems[activeItemId];
    MenuItem &currentMenu = menuItems[currentMenuId];

    switch (button) {
    case 0:
        // up
        if (!menuActive) {
            handleRotaryOutsideMenu(-stepSize);
            break;
        };
        if (inFunction) {
            doFunction(currentItem.functionName, -stepSize);
        } else {
            auto [siblingCount, siblingIndex, childIds] = getSiblingInfo(menuItems, currentMenuId, activeItemId);
            if (siblingCount) {
                siblingIndex = (siblingIndex + siblingCount - 1) % siblingCount;
                activeItemId = childIds[siblingIndex];
            }
            drawMenu(currentMenuId, activeItemId, menuLevel);
        }
        break;

    case 1:
        // down
        if (!menuActive) {
            handleRotaryOutsideMenu(stepSize);
            break;
        };
        if (inFunction) {
            doFunction(currentItem.functionName, stepSize);
        } else {
            auto [siblingCount, siblingIndex, childIds] = getSiblingInfo(menuItems, currentMenuId, activeItemId);
            if (siblingCount) {
                siblingIndex = (siblingIndex + 1) % siblingCount;
                activeItemId = childIds[siblingIndex];
            }
            drawMenu(currentMenuId, activeItemId, menuLevel);
        }
        break;

    case 2:
        // enter
        if (!menuActive) {
            activeItemId = 0;
            menuLevel = 0;
            currentMenuId = -1;
            menuActive = true;
            inFunction = false;
            menustate = MENU;

            time_t now;
            struct tm timeinfo;
            time(&now);
            localtime_r(&now, &timeinfo);
            time_set[0] = timeinfo.tm_year + 1900;
            time_set[1] = timeinfo.tm_mon + 1;
            time_set[2] = timeinfo.tm_mday;
            time_set[3] = timeinfo.tm_hour;
            time_set[4] = timeinfo.tm_min;

            clearScreen(2);
            clearScreen(3);
            clearScreen(4);

            drawMenu(currentMenuId, activeItemId, menuLevel);
            setBrightness(1, hardware.invertbacklight ? 1000 : 3000);
        } else {
            selectMenuItem(currentItem.id);
        }
        break;

    case 3:
        // escape
        if (inFunction) {
            doFunction(currentItem.functionName, 0);
        } else if (menuLevel > 0) {
            clearScreen(menuLevel + 1);
            activeItemId = currentMenuId;
            currentMenuId = menuItems[currentMenuId].parentId;
            menuLevel--;
            drawMenu(currentMenuId, activeItemId, menuLevel);
        } else if (menuActive) {
            exitmenu();
        } else {
            Serial.println("Manual nightmode toggle");
            int ledc_from = perc2ledc(manualNightmode ? 0 : prefs.getUShort("brightness", 15));
            manualNightmode = !manualNightmode;
            nightmode = manualNightmode;
            showNotification(manualNightmode ? "Nightmode On" : "Nightmode Off");
            int ledc_to = perc2ledc(manualNightmode ? 0 : prefs.getUShort("brightness", 15));
            fadeLEDC(1, ledc_from, ledc_to, 800);
            initSprites(true);
        }
        break;
    }
}

void handleMenuHold(int button) {
    MenuItem &currentItem = menuItems[activeItemId];
    if (button == 2 && currentItem.functionName.startsWith("setAlarm")) {
        int i = currentItem.functionName.charAt(currentItem.functionName.length() - 1) - '0';
        alarm_set[i] = 24 * 60;
        prefs.putBytes("alarm_set", alarm_set, sizeof(alarm_set));
        clearScreen(menuLevel + 2);
        drawMenu(currentMenuId, activeItemId, menuLevel);
        inFunction = false;
    } else {
        handleMenuInput(button, 1);
    }
}

String getValue(const String &name) {
    static const std::map<String, std::function<String()>> menuFunctions = {
        {"setAlarm0", []() { return formatTime(alarm_set[0]); }},
        {"setAlarm1", []() { return formatTime(alarm_set[1]); }},
        {"setAlarm2", []() { return formatTime(alarm_set[2]); }},
        {"setAlarm3", []() { return formatTime(alarm_set[3]); }},
        {"setAlarm4", []() { return formatTime(alarm_set[4]); }},
        {"setAlarm5", []() { return formatTime(alarm_set[5]); }},
        {"setAlarm6", []() { return formatTime(alarm_set[6]); }},
        {"setAlarm7", []() { return formatTime(alarm_set[7]); }},
        {"selectHourMode", []() { return (prefs.getUShort("hourmode", 0) == 0) ? "0:00" : (prefs.getUShort("hourmode", 0) == 1) ? "00:00": "0:00 AM"; }},
        {"selectAlarmSound", []() { return String(sounds[prefs.getUShort("alarmsound", 0)].name); }},
        {"selectMinuteSound", []() { return (prefs.getUShort("minutesound", 0) == 0) ? "OFF" : (prefs.getUShort("minutesound", 0) == 1) ? "Soft"
                                                                                                                                        : "Loud"; }},
        {"selectHourSound", []() {
             String modeStrs[] = {"OFF", "Once", "Count", "Cuckoo"};
             uint16_t hourSound = prefs.getUShort("hoursound", 0);
             String modeStr = (hourSound >= 0 && hourSound < 4) ? modeStrs[hourSound] : "?";
             return modeStr;
         }},
        {"selectNightFrom", []() { return String(formatTime(prefs.getUShort("night_from", 22) * 60)); }},
        {"selectNightTo", []() { return String(formatTime(prefs.getUShort("night_to", 9) * 60)); }},
        {"adjustVolume", []() { return String(prefs.getUShort("volume", 50)) + "%"; }},
        {"exitMenu", []() { return ""; }},
        {"setYear", []() { return String(time_set[0]); }},
        {"setMonth", []() { return String(time_set[1]); }},
        {"setDay", []() { return String(time_set[2]); }},
        {"setHour", []() { return String(time_set[3]); }},
        {"setMinute", []() { return String(time_set[4]); }},
        {"setTime", []() { return ""; }},
        {"setWifi", []() { return prefs.getBool("enablewifi", false) ? "ON" : "OFF"; }},
        {"selectFont", []() { return String(fonts[prefs.getUShort("font", 0)].name); }},
        {"setBrightness", []() { return String(prefs.getUShort("brightness", 15) * 5) + "%"; }},
        {"setMinBrightness", []() { return String(prefs.getULong("minbrightness", 40)); }},
        {"showVersion", []() { return ""; }},
        {"setColor", []() { return String(colors[prefs.getUShort("color", 0)].name); }},
        {"setTimezone", []() { return String(timezones[prefs.getUShort("timezone", 1)].name); }}};

    auto it = menuFunctions.find(name);
    if (it != menuFunctions.end()) {
        return it->second();
    } else {
        Serial.println("Function not found: " + name);
        return "";
    }
}

void updateTime(int index, int increment, int minValue, int maxValue) {
    if (increment == 0) {
        if (inFunction) {
            clearScreen(menuLevel + 2);
            setSystemTime();
        } else {
            showValue(String(time_set[index]), menuLevel + 1, true);
        }
    } else {
        time_set[index] = (time_set[index] + increment - minValue + (maxValue - minValue + 1)) % (maxValue - minValue + 1) + minValue;
        showValue(String(time_set[index]), menuLevel + 1);
    }
}

std::map<String, std::function<void(int)>> &getFunctionMap() {
    static std::map<String, std::function<void(int)>> functionMap = {
        {"setYear", [](int increment) { updateTime(0, increment, 2020, 2500); }},
        {"setMonth", [](int increment) { updateTime(1, increment, 1, 12); }},
        {"setDay", [](int increment) { updateTime(2, increment, 1, 31); }},
        {"setHour", [](int increment) { updateTime(3, increment, 0, 23); }},
        {"setMinute", [](int increment) { updateTime(4, increment, 0, 59); }},
        {"setTime", [](int increment) {
             setSystemTime();
             exitmenu();
         }},
        {"setWifi", [](int increment) {
             bool wifimode = prefs.getBool("enablewifi", false);
             if (increment != 0) wifimode = !wifimode;
             String modeStr = (wifimode) ? "ON" : "OFF";
             if (increment == 0) {
                 if (inFunction) {
                     clearScreen(menuLevel + 2);
                     if (wifimode) {
                         init_web();
                     } else {
                         WiFi.disconnect(false, true);
                         WiFi.mode(WIFI_OFF);
                     }
                     return;
                 } else {
                     showValue(modeStr, menuLevel + 1, true);
                 }
             } else {
                 prefs.putBool("enablewifi", wifimode);
                 showValue(modeStr, menuLevel + 1);
             }
         }},
        {"selectHourMode", [](int increment) {
             uint16_t hourMode = prefs.getUShort("hourmode", 0);
             increment = std::clamp(increment, -1, 1);
             hourMode = (hourMode + increment + 3) % 3;
             String modeStr = (hourMode == 0) ? "0:00" : (hourMode == 1) ? "00:00" : "0:00 AM";
             if (increment == 0) {
                 if (inFunction) {
                     clearScreen(menuLevel + 2);
                     return;
                 } else {
                     showValue(modeStr, menuLevel + 1, true);
                 }
             } else {
                 showValue(modeStr, menuLevel + 1);
                 prefs.putUShort("hourmode", hourMode);
             }
         }},
        {"selectHourSound", [](int increment) {
             uint16_t hourSound = prefs.getUShort("hoursound", 0);
             increment = std::clamp(increment, -1, 1);
             hourSound = (hourSound + increment + 4) % 4;
             String modeStrs[] = {"OFF", "Once", "Count", "Cuckoo"};
             String modeStr = (hourSound >= 0 && hourSound < 4) ? modeStrs[hourSound] : "?";
             if (increment == 0) {
                 if (inFunction) {
                     audioStop();
                     clearScreen(menuLevel + 2);
                     return;
                 } else {
                     if (hourSound == 1 || hourSound == 2) {
                         audioStart("bell.mp3");
                     } else if (hourSound == 3) {
                         audioStart("cuckoo.mp3");
                     }
                     showValue(modeStr, menuLevel + 1, true);
                 }
             } else {
                 if (hourSound == 1 || hourSound == 2) {
                     audioStart("bell.mp3");
                 } else if (hourSound == 3) {
                     audioStart("cuckoo.mp3");
                 }
                 showValue(modeStr, menuLevel + 1);
                 prefs.putUShort("hoursound", hourSound);
             }
         }},
        {"adjustVolume", [](int increment) {
             uint16_t volume = prefs.getUShort("volume", 50);
             if (increment == 0) {
                 if (inFunction) {
                     clearScreen(menuLevel + 2);
                     return;
                 } else {
                     audioStart("cuckoo.mp3");
                     showValue(String(static_cast<int>(volume)) + "%", menuLevel + 1, true);
                 }
             } else {
                 volume = std::clamp(volume + increment, 0, 100);
                 if (!audioRunning()) {
                    audioStart("cuckoo.mp3", volume);
                 } else {
                    audioVolume(volume);
                 }
                 showValue(String(static_cast<int>(volume)) + "%", menuLevel + 1);
                 prefs.putUShort("volume", volume);
             }
         }},
        {"selectNightFrom", [](int increment) {
             uint16_t nightfrom = prefs.getUShort("night_from", 22);
             increment = std::clamp(increment, -1, 1);
             nightfrom = (nightfrom + increment + 24) % 24;
             if (increment == 0) {
                 if (inFunction) {
                     clearScreen(menuLevel + 2);
                     return;
                 } else {
                     showValue(formatTime(nightfrom * 60), menuLevel + 1, true);
                 }
             } else {
                 showValue(formatTime(nightfrom * 60), menuLevel + 1);
                 prefs.putUShort("night_from", nightfrom);
             }
         }},
        {"selectNightTo", [](int increment) {
             uint16_t nightto = prefs.getUShort("night_to", 9);
             increment = std::clamp(increment, -1, 1);
             nightto = (nightto + increment + 24) % 24;
             if (increment == 0) {
                 if (inFunction) {
                     clearScreen(menuLevel + 2);
                     return;
                 } else {
                     showValue(formatTime(nightto * 60), menuLevel + 1, true);
                 }
             } else {
                 showValue(formatTime(nightto * 60), menuLevel + 1);
                 prefs.putUShort("night_to", nightto);
             }
         }},
        {"selectMinuteSound", [](int increment) {
             uint16_t minuteSound = prefs.getUShort("minutesound", 0);

             increment = std::clamp(increment, -1, 1);
             minuteSound = (minuteSound + increment + 3) % 3;
             String modeStr = (minuteSound == 0) ? "OFF" : (minuteSound == 1) ? "Soft"
                                                                              : "Loud";
             if (increment == 0) {
                 if (inFunction) {
                     clearScreen(menuLevel + 2);
                     return;
                 } else {
                     showValue(modeStr, menuLevel + 1, true);
                 }
             } else {
                 showValue(modeStr, menuLevel + 1);
                 prefs.putUShort("minutesound", minuteSound);
             }
         }},
        {"setBrightness", [](int increment) {
             uint16_t brightness = prefs.getUShort("brightness", 15);
             if (increment == 0) {
                 if (inFunction) {
                     menustate = MENU;
                     setBrightness(1, hardware.invertbacklight ? 1000 : 3000);
                     clearScreen(menuLevel + 2);
                     clearScreen(3);
                     clearScreen(4);
                     return;
                 } else {
                     menustate = PREVIEW;
                     d3 = 10;
                     showValue(String(brightness * 5) + "%", menuLevel + 1, true);
                 }
             } else {
                 increment = std::clamp(increment, -1, 1);
                 brightness = std::clamp(brightness + increment, 0, hasLightmeter ? 40 : 20);
                 avgLux = config.luxfactor;
                 int ledc = perc2ledc(brightness);
                 setBrightness(1, ledc);
                 vTaskDelay(10 / portTICK_PERIOD_MS);
                 showValue(String(brightness * 5) + "%", menuLevel + 1);
                 prefs.putUShort("brightness", brightness);
             }
         }},
        {"setMinBrightness", [](int increment) {
             uint16_t brightness = prefs.getULong("minbrightness", 40);
             if (increment == 0) {
                 int digitId = 4;
                 uint8_t screenId = flipOrientation ? 3 - (digitId - 1) : digitId - 1;
                 if (inFunction) {
                     menustate = MENU;
                     removePWM(backlight[screenId]);
                     clearScreen(menuLevel + 2);
                     clearScreen(3);
                     clearScreen(4);
                     return;
                 } else {
                     menustate = MENU;
                     selectScreen(4);
                     drawDigit(0, false);
                     deselectScreen(4);
                     addPWM(backlight[screenId], 2);
                     setBrightness(2, hardware.invertbacklight ? 4095 - brightness : brightness);
                     showValue(String(brightness), menuLevel + 1, true);
                 }
             } else {
                 brightness = std::clamp(brightness + increment, 0, 4095);
                 setBrightness(2, hardware.invertbacklight ? 4095 - brightness : brightness);
                 vTaskDelay(10 / portTICK_PERIOD_MS);
                 showValue(String(brightness), menuLevel + 1);
                 prefs.putULong("minbrightness", brightness);
             }
         }},
        {"setColor", [](int increment) {
             uint16_t color = prefs.getUShort("color", 0);
             increment = std::clamp(increment, -1, 1);
             color = (color + increment + colorCount) % colorCount;
             if (increment == 0) {
                 if (inFunction) {
                     menustate = MENU;
                     clearScreen(menuLevel + 2);
                     clearScreen(3);
                     clearScreen(4);
                     return;
                 } else {
                     menustate = PREVIEW;
                     d3 = 10;
                     prevMinute = -1;
                     showValue(colors[color].name, menuLevel + 1, true);
                 }
             } else {
                 d3 = 10;
                 prevMinute = -1;
                 showValue(colors[color].name, menuLevel + 1);
                 prefs.putUShort("color", color);
             }
         }},
        {"selectFont", [](int increment) {
             uint16_t font = prefs.getUShort("font", 0);
             if (increment == 0) {
                 if (inFunction) {
                     menustate = MENU;
                     clearScreen(menuLevel + 2);
                     clearScreen(3);
                     clearScreen(4);
                     return;
                 } else {
                     menustate = PREVIEW;
                     prevMinute = -1;
                     d3 = 10;
                     showValue(fonts[font].name, menuLevel + 1, true);
                 }
             } else {
                 prevMinute = -1;
                 d3 = 10;
                 increment = std::clamp(increment, -1, 1);
                 font = (font + increment + fontCount) % fontCount;
                 showValue(fonts[font].name, menuLevel + 1);
                 prefs.putUShort("font", font);
             }
         }},
        {"selectAlarmSound", [](int increment) {
             uint16_t soundid = prefs.getUShort("alarmsound", 0);
             if (increment == 0) {
                 if (inFunction) {
                     audioStop();
                     clearScreen(menuLevel + 2);
                     return;
                 } else {
                     audioStop();
                     alarmStart(soundid);
                     showValue(sounds[soundid].name, menuLevel + 1, true);
                 }
             } else {
                 increment = std::clamp(increment, -1, 1);
                 soundid = (soundid + increment + soundCount) % soundCount;
                 audioStop();
                 alarmStart(soundid);
                 showValue(sounds[soundid].name, menuLevel + 1);
                 prefs.putUShort("alarmsound", soundid);
             }
         }},
        {"setTimezone", [](int increment) {
             uint16_t tzid = prefs.getUShort("timezone", 1);
             if (increment == 0) {
                 if (inFunction) {
                     clearScreen(menuLevel + 2);
                     String tz = timezones[tzid].tzstring;
                     setenv("TZ", tz.c_str(), 1);
                     tzset();
                     return;
                 } else {
                     showValue(timezones[tzid].name + " " + timezones[tzid].tzcity, menuLevel + 1, true);
                 }
             } else {
                 increment = std::clamp(increment, -1, 1);
                 tzid = (tzid + increment + timeZoneCount) % timeZoneCount;
                 showValue(timezones[tzid].name + " " + timezones[tzid].tzcity, menuLevel + 1);
                 prefs.putUShort("timezone", tzid);
             }
         }},
        {"showVersion", [](int increment) {
             increment = std::clamp(increment, -1, 1);
             if (increment == 0) {
                 if (inFunction) {
                     menustate = MENU;
                     clearScreen(menuLevel + 2);
                     clearScreen(menuLevel + 3);
                     return;
                 } else {
                     menustate = DEBUG;
                     showVersion(menuLevel + 2);
                 }
             } else {
                 showVersion(menuLevel + 2);
             }
         }},
        {"exitMenu", [](int increment) {
             inFunction = true;
             if (menuLevel > 0) {
                 activeItemId = currentMenuId;
                 currentMenuId = menuItems[currentMenuId].parentId;
                 clearScreen(menuLevel + 1);
                 menuLevel--;
             } else {
                 exitmenu();
             }
         }}};

    constexpr int totalMinutes = 24 * 60 + 5;
    for (int i = 0; i <= 7; ++i) {
        functionMap["setAlarm" + String(i)] = [i](int increment) {
            if (increment == 0) {
                if (inFunction) {
                    clearScreen(menuLevel + 2);
                    prefs.putBytes("alarm_set", alarm_set, sizeof(alarm_set));
                } else {
                    showValue(formatTime(alarm_set[i]), menuLevel + 1, true);
                }
            } else {
                alarm_set[i] = (alarm_set[i] + increment * 5 + totalMinutes) % totalMinutes;
                showValue(formatTime(alarm_set[i]), menuLevel + 1);
            }
        };
    }

    return functionMap;
}

void doFunction(String &name, int8_t increment) {
    lastmenuactive = millis();
    auto &functionMap = getFunctionMap();

    auto it = functionMap.find(name);
    if (it != functionMap.end()) {
        it->second(increment);
        if (increment == 0) {
            inFunction = !inFunction;
            if (!inFunction && menuActive) {
                drawMenu(currentMenuId, activeItemId, menuLevel);
            }
        }
    } else {
        Serial.println("Function not found: " + name);
    }
}

void showValue(String value, uint8_t menuLevel, bool initialise) {
    selectScreen(menuLevel + 1);
    if (initialise) {
        tft.fillScreen(TFT_BLACK);
        tft.loadFont("/dejavusanscond24", *contentFS);
        tft.setTextColor(tft.color565(200, 200, 200), TFT_BLACK);
        tft.setCursor(50, 50);
        tft.println(menuItems[activeItemId].name);
        if (menuItems[activeItemId].infoTxt != "") {
            tft.loadFont("/dejavusanscond15", *contentFS);
            tft.setViewport(50, 150, TFT_WIDTH - 50, TFT_HEIGHT - 150);
            tft.setCursor(0, 0);
            tft.println(menuItems[activeItemId].infoTxt);
            tft.resetViewport();
        }
        tft.unloadFont();
        tft.loadFont("/dejavusanscond15", *contentFS);
    }
    tft.fillTriangle(TFT_WIDTH / 2, 85, TFT_WIDTH / 2 + 10, 95, TFT_WIDTH / 2 - 10, 95, tft.color565(200, 200, 200));
    tft.fillTriangle(TFT_WIDTH / 2, 137, TFT_WIDTH / 2 + 10, 127, TFT_WIDTH / 2 - 10, 127, tft.color565(200, 200, 200));
    tft.fillRect(50, 100, TFT_WIDTH - 80, 22, spr.color565(40, 40, 40));
    tft.setTextColor(spr.color565(240, 240, 50), spr.color565(40, 40, 40));
    tft.setCursor(53, 104);
    tft.println(value);
    // tft.unloadFont();
    deselectScreen(menuLevel + 1);
};

void printTableRow(const char *label, const String &value, int y) {
    tft.setTextColor(tft.color565(120, 120, 120), TFT_BLACK);
    tft.setCursor(0, y, 2);
    tft.print(label);

    tft.loadFont("/dejavusanscond15", *contentFS);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(80, y);
    tft.println(value);
    tft.unloadFont();
}

void showVersion(int8_t digitId) {
    selectScreen(digitId);
    tft.fillScreen(TFT_BLACK);
    tft.loadFont("/dejavusanscond24", *contentFS);
    tft.setTextColor(tft.color565(200, 200, 200), TFT_BLACK);
    tft.setCursor(50, 25);
    tft.println("Version");
    tft.unloadFont();
    tft.setViewport(30, 40, TFT_WIDTH - 30, TFT_HEIGHT - 50);

    int y = 0;
    int lineHeight = 17;
    printTableRow("Serial", generateSerialWord(), y += lineHeight);
    printTableRow("Firmware", parseDate(__DATE__), y += lineHeight);
    y += lineHeight;
    printTableRow("Chip", String(ESP.getChipModel()), y += lineHeight);
    printTableRow("Flash", String(ESP.getFlashChipSize() / 1024) + " kB", y += lineHeight);
    printTableRow("PSRAM", String(ESP.getPsramSize() / 1024) + " kB", y += lineHeight);
    printTableRow("Free PSRAM", String(ESP.getFreePsram() / 1024) + " kB", y += lineHeight);
    printTableRow("Free Heap", String(ESP.getFreeHeap() / 1024) + " kB", y += lineHeight);

    tft.resetViewport();
    deselectScreen(digitId);

    selectScreen(digitId + 1);
    tft.fillScreen(TFT_BLACK);
    tft.loadFont("/dejavusanscond24", *contentFS);
    tft.setTextColor(tft.color565(200, 200, 200), TFT_BLACK);
    tft.setCursor(50, 25);
    tft.println("Debug info");
    tft.unloadFont();
    tft.setViewport(30, 40, TFT_WIDTH - 30, TFT_HEIGHT - 50);

    y = 0;
    printTableRow("Lightmeter", "0", y += lineHeight);
    printTableRow("Orientation", "0", y += lineHeight);
    y += lineHeight;
    if (WiFi.localIP().toString() != "0.0.0.0") {
        printTableRow("SSID", WiFi.SSID(), y += lineHeight);
        printTableRow("IP", WiFi.localIP().toString(), y += lineHeight);
    }
    tft.resetViewport();
    deselectScreen(digitId + 1);

    Serial.println("===== System Info =====");
    Serial.println("Compiled: " + String(__DATE__) + " " + String(__TIME__));
    Serial.println("Chip: " + String(ESP.getChipModel()));
    Serial.println("Flash: " + String(ESP.getFlashChipSize() / 1024 / 1024) + "MB");
    Serial.println("Total PSRAM: " + String(ESP.getPsramSize() / 1024 / 1024) + "MB");
    Serial.println("Free PSRAM: " + String(ESP.getFreePsram() / 1024) + "KB");
    Serial.println("Heap: " + String(ESP.getFreeHeap() / 1024) + "KB");
    Serial.println("Serial: " + generateSerialWord());
    Serial.println("=======================");
}