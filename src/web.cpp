#include "web.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>
#include <FS.h>
#include <Preferences.h>
#include <WiFi.h>

#include <algorithm>

#include "AsyncJson.h"
#include "LittleFS.h"
#include "SPIFFSEditor.h"
#include "menutree.h"
#include "timefunctions.h"
#include "udp.h"
#include "sound.h"

AsyncWebServer server(80);
extern std::vector<MenuItem> menuItems;

uint32_t lastssidscan = 0;

void init_web() {
    WiFi.mode(WIFI_STA);

    wm.connectToWifi();

    server.on("/reboot", HTTP_POST, [](AsyncWebServerRequest *request) {
        request->send(200, "text/plain", "OK Reboot");
        delay(100);
        ESP.restart();
    });

    server.on("/get_db", HTTP_GET, [](AsyncWebServerRequest *request) {
        String json = "";
        if (request->hasParam("mac")) {
            String dst = request->getParam("mac")->value();
            json = "{\"error\": \"malformatted parameter\"}";
        }
        request->send(200, "application/json", json);
    });

    server.on("/setup", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(*contentFS, "/www/setup.html");
    });

    server.on("/menu", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(*contentFS, "/www/menu.html");
    });

    server.on("/radio", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(*contentFS, "/www/radio.html");
    });

    server.on("/get_wifi_config", HTTP_GET, [](AsyncWebServerRequest *request) {
        Preferences preferences;
        AsyncResponseStream *response = request->beginResponseStream("application/json");
        JsonDocument doc;
        preferences.begin("wifi", false);
        const char *keys[] = {"ssid", "pw", "ip", "mask", "gw", "dns"};
        const size_t numKeys = sizeof(keys) / sizeof(keys[0]);
        for (size_t i = 0; i < numKeys; i++) {
            doc[keys[i]] = preferences.getString(keys[i], "");
        }
        doc["mac"] = Network.macAddress();
        serializeJson(doc, *response);
        request->send(response);
    });

    server.on("/get_ssid_list", HTTP_GET, [](AsyncWebServerRequest *request) {
        AsyncResponseStream *response = request->beginResponseStream("application/json");
        JsonDocument doc;
        doc["scanstatus"] = WiFi.scanComplete();
        JsonArray networks = doc["networks"].to<JsonArray>();

        for (int i = 0; i < (WiFi.scanComplete() > 50 ? 50 : WiFi.scanComplete()); ++i) {
            if (WiFi.SSID(i) != "") {
                JsonObject network = networks.add<JsonObject>();
                network["ssid"] = WiFi.SSID(i);
                network["ch"] = WiFi.channel(i);
                network["rssi"] = WiFi.RSSI(i);
                network["enc"] = WiFi.encryptionType(i);
            }
        }
        if (WiFi.scanComplete() != -1 && (WiFi.scanComplete() == -2 || millis() - lastssidscan > 30000)) {
            WiFi.scanDelete();
            Serial.println("start scanning");
            WiFi.scanNetworks(true, true);
            lastssidscan = millis();
        }

        serializeJson(doc, *response);
        request->send(response);
    });

    server.on("/menujson", HTTP_GET, [](AsyncWebServerRequest *request) {
        AsyncResponseStream *response = request->beginResponseStream("application/json");
        JsonDocument doc;

        // Create a JSON array for the menu items
        JsonArray menuArray = doc["menu"].to<JsonArray>();

        for (const auto &item : menuItems) {
            JsonObject obj = menuArray.add<JsonObject>();
            if (item.functionName == "exitMenu") continue;
            obj["id"] = item.id;
            obj["name"] = item.name;
            obj["parentId"] = item.parentId;
            obj["hasChildren"] = item.hasChildren;
            obj["functionName"] = item.functionName;
            if (item.functionName) {
                obj["value"] = getValue(item.functionName);
            }
        }

        // Serialize the JSON document to the response stream
        serializeJsonPretty(doc, *response);
        request->send(response);
    });

    server.on("/getaudio", HTTP_GET, [](AsyncWebServerRequest *request) {
        AsyncResponseStream *response = request->beginResponseStream("application/json");
        JsonDocument doc;
        doc["volume"] = prefs.getUShort("volume", 50);
        doc["url"] = prefs.getString("radiostation", "");
        doc["uuid"] = prefs.getString("radiouuid", "");
        doc["streaming"] = audioStreaming();
        serializeJsonPretty(doc, *response);
        request->send(response);
    });

    AsyncCallbackJsonWebHandler *handler = new AsyncCallbackJsonWebHandler("/save_wifi_config", [](AsyncWebServerRequest *request, JsonVariant &json) {
        const JsonObject &jsonObj = json.as<JsonObject>();
        Preferences preferences;
        preferences.begin("wifi", false);
        const char *keys[] = {"ssid", "pw", "ip", "mask", "gw", "dns"};
        const size_t numKeys = sizeof(keys) / sizeof(keys[0]);
        for (size_t i = 0; i < numKeys; i++) {
            String key = keys[i];
            if (jsonObj[key].is<String>()) {
                preferences.putString(key.c_str(), jsonObj[key].as<String>());
            }
        }
        preferences.end();
        Serial.println("config saved");
        request->send(200, "text/plain", "Ok, saved");

        if (jsonObj["ssid"].as<String>() == "factory") {
            preferences.begin("wifi", false);
            preferences.putString("ssid", "");
            preferences.putString("pw", "");
            preferences.end();

            prefs.putBool("enablewifi", false);

            delay(100);
            esp_deep_sleep_start();
        }

        delay(100);
        ESP.restart();
    });

    server.on("/setalarm", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (request->hasParam("alarmtime", true)) {
            uint16_t alarmTime = request->getParam("alarmtime", true)->value().toInt();
            setDailyAlarm(alarmTime);
        }
        request->send(200, "text/plain", "OK");
    });

    server.on("/getalarm", HTTP_GET, [](AsyncWebServerRequest *request) {
        uint16_t nextAlarm = getDailyAlarm();
        request->send(200, "text/plain", String(nextAlarm));
    });

    server.on("/playradio", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (request->hasParam("url", true)) {
            String url = request->getParam("url", true)->value();
            prefs.putString("radiostation", url);
            audioStart(url);
        }
        if (request->hasParam("uuid", true)) {
            String uuid = request->getParam("uuid", true)->value();
            prefs.putString("radiouuid", uuid);
        }
        request->send(200, "text/plain", "OK");
    });

    server.on("/setvolume", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (request->hasParam("volume", true)) {
            int volume = request->getParam("volume", true)->value().toInt();
            audioVolume(volume, true);
        }
        request->send(200, "text/plain", "OK");
    });

    server.on("/audiostop", HTTP_POST, [](AsyncWebServerRequest *request) {
        audioStop();
        request->send(200, "text/plain", "OK");
    });

    server.addHandler(handler);
    server.addHandler(new SPIFFSEditor(*contentFS));

    server.onNotFound([](AsyncWebServerRequest *request) {
        if (request->url() == "/" || request->url() == "index.htm") {
            request->send(200, "text/html", "index.html not found. Did you forget to upload the file system partition?");
            return;
        }
        request->send(404);
    });

    server.serveStatic("/", *contentFS, "/www/").setDefaultFile("index.html");

    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "content-type");

    server.begin();
}
