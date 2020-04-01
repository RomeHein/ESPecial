#include "ServerHandler.h"
#include "PreferenceHandler.h"
#include <Update.h>
#include <ArduinoJson.h>
#include "index.h"

void ServerHandler::begin()
{
    server.on("/", [this]() { handleRoot(); });
    server.on("/clear/settings", [this]() { handleClearSettings(); });
    server.on("/gpios", [this]() { getGpios(); });
    server.on("/settings", [this]() { getSettings(); });
    server.on("/gpios/available", [this]() { handleAvailableGpios(); });
    server.on("/digital/{}/{}", [this]() { handleSetGpioState(); });
    server.on("/digital/{}", [this]() { handleGetGpioState(); });
    server.on("/gpio/{}/delete", [this]() { handleGpioRemove(); });
    server.on("/gpio", HTTP_POST, [this]() { handleGpioEdit(); });
    server.on("/gpio/new", HTTP_POST, [this]() { handleGpioNew(); });
    server.on("/install", HTTP_POST, [this]() { handleUpload(); }, [this]() { install(); });
    server.on("/mqtt", HTTP_POST, [this]() { handleMqttEdit(); });
    server.on("/telegram", HTTP_POST, [this]() { handleTelegramEdit(); });
    server.onNotFound([this]() {handleNotFound(); });

    server.begin();
}

// Main

void ServerHandler::handleNotFound()
{
    server.send(404, "text/plain", "Not found");
}

void ServerHandler::handleClearSettings()
{
    preference.clear();
    server.send(200, "text/plain", "Settings clear");
}

void ServerHandler::handleRoot()
{
    server.sendHeader("Connection", "close");
    String s = MAIN_page;
    server.send(200, "text/html", s);
}

// Settings

void ServerHandler::getSettings() {
    const size_t capacity = JSON_OBJECT_SIZE(7) + 300;
    StaticJsonDocument<(capacity)> doc;
    JsonObject telegram = doc.createNestedObject("telegram");
    telegram["active"] = preference.telegram.active;
    telegram["token"] = preference.telegram.token;
    JsonObject mqtt = doc.createNestedObject("mqtt");
    mqtt["host"] = preference.mqtt.host;
    mqtt["port"] = preference.mqtt.port;
    mqtt["user"] = preference.mqtt.user;
    mqtt["password"] = preference.mqtt.password;
    mqtt["topic"] = preference.mqtt.topic;
    String output;
    serializeJson(doc, output);
    server.send(200, "text/json", output);
    return;
}

// Gpio hanlding

void ServerHandler::getGpios() 
{
    StaticJsonDocument<(2*sizeof(preference.gpios))> doc;
    for (GpioFlash& gpio : preference.gpios) {
        if (gpio.pin) {
            JsonObject object = doc.createNestedObject();
            object["pin"] = gpio.pin;
            object["label"] = gpio.label;
            object["mode"] = gpio.mode;
            object["state"] = gpio.state;
        }
    }
    String output;
    serializeJson(doc, output);
    server.send(200, "text/json", output);
    Serial.println("Preference reference:");
    Serial.printf("%p", &preference);
    return;
}

void ServerHandler::handleGpioEdit()
{
    server.sendHeader("Connection", "close");
    const size_t capacity = JSON_OBJECT_SIZE(1) + 100;
    DynamicJsonDocument doc(capacity);

    DeserializationError error = deserializeJson(doc, server.arg(0));
    if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.c_str());
        return;
    }

    for (GpioFlash& gpio : preference.gpios)
    {
        if (gpio.pin == doc["pin"].as<int>())
        {
            bool saved = preference.editGpio(gpio, doc["settings"]["pin"].as<int>(), doc["settings"]["label"].as<char*>(), doc["settings"]["mode"].as<int>());
            if (saved) {
                const size_t capacity = JSON_OBJECT_SIZE(1) + 100;
                StaticJsonDocument<capacity> doc;
                JsonObject object = doc.createNestedObject();
                object["pin"] = gpio.pin;
                object["label"] = gpio.label;
                object["mode"] = gpio.mode;
                object["state"] = gpio.state;
                String output;
                serializeJson(doc[0], output);
                server.send(200, "text/json", output);
                return;
            } else {
                server.send(404, "text/plain", "Could not save");
            }
        }
    }
    server.send(404, "text/plain", "Not found");
}

void ServerHandler::handleGpioNew()
{
    server.sendHeader("Connection", "close");
    const size_t capacity = JSON_OBJECT_SIZE(4) + 90;
    DynamicJsonDocument doc(capacity);

    DeserializationError error = deserializeJson(doc, server.arg(0));
    if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.c_str());
        return;
    }
    const int pin = doc["settings"]["pin"].as<int>();
    const char* label = doc["settings"]["label"].as<char*>();
    const int mode = doc["settings"]["mode"].as<int>();
    if (pin && label && mode) {
        preference.addGpio(pin, label, mode);
        server.send(200, "text/json", server.arg(0));
        return;
    }
    server.send(404, "text/plain", "Missing parameters");
}

void ServerHandler::handleGpioRemove() 
{
    bool removed = preference.removeGpio(atoi(server.pathArg(0).c_str()));
    if (removed){
        server.send(404, "text/plain", "Done");
    }
    server.send(404, "text/plain", "Not found");
}

void ServerHandler::handleAvailableGpios() {
    const size_t capacity = JSON_ARRAY_SIZE(1) + GPIO_PIN_COUNT*(JSON_OBJECT_SIZE(2)+20);
    StaticJsonDocument<capacity> doc;
    for (int i = 0; i<GPIO_PIN_COUNT; i++) {
        JsonObject object = doc.createNestedObject();
        object["pin"] = i;
        object["inputOnly"] = !GPIO_IS_VALID_OUTPUT_GPIO(i);
    }
    String output;
    serializeJson(doc, output);
    server.send(200, "text/json", output);
}

void ServerHandler::handleGetGpioState()
{
    const int pin = atoi(server.pathArg(0).c_str());
    const int state = digitalRead(pin);
    char json[50];
    snprintf(json, sizeof(json), "{\"pin\":%i,\"state\":%i}", pin, state);
    server.send(200, "text/json", json);
}

void ServerHandler::handleSetGpioState()
{
    const int pin = atoi(server.pathArg(0).c_str());
    if (server.pathArg(1) && server.pathArg(1) != "")
    {
        const int newState = atoi(server.pathArg(1).c_str());
        preference.setGpioState(pin, newState);
    }
    handleGetGpioState();
}

// Settings API

void ServerHandler::handleUpload()
{
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart(); 
}

void ServerHandler::install()
{
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
        Serial.printf("Update: %s\n", upload.filename.c_str());
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
            Update.printError(Serial);
        }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        /* flashing firmware to ESP*/
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
            Update.printError(Serial);
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) {
            Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
        } else {
            Update.printError(Serial);
        }
    } 
}

void ServerHandler::handleMqttEdit () {
    server.sendHeader("Connection", "close");
    const size_t capacity = JSON_OBJECT_SIZE(4) + 90;
    DynamicJsonDocument doc(capacity);

    DeserializationError error = deserializeJson(doc, server.arg(0));
    if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.c_str());
        return;
    }
    const char* host = doc["host"].as<char*>();
    const int port = doc["port"].as<int>();
    const char* user = doc["user"].as<char*>();
    const char* password = doc["password"].as<char*>();
    const char* topic = doc["topic"].as<char*>();
    if (host && port && user && password && topic) {
        preference.editMqtt(host,port,user,password,topic);
        server.send(200, "text/json", server.arg(0));
        return;
    }
    server.send(404, "text/plain", "Missing parameters");
}

void ServerHandler::handleTelegramEdit () {
    server.sendHeader("Connection", "close");
    const size_t capacity = JSON_OBJECT_SIZE(2) + 150;
    DynamicJsonDocument doc(capacity);

    DeserializationError error = deserializeJson(doc, server.arg(0));
    if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.c_str());
        return;
    }
    const char* token = doc["token"].as<char*>();
    const int active = doc["active"].as<int>();
    if (token) {
        preference.editTelegram(token, active);
        server.send(200, "text/json", server.arg(0));
        return;
    }
    server.send(404, "text/plain", "Missing parameters");
}
