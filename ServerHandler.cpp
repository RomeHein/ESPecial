#include "ServerHandler.h"
#include <ESPmDNS.h>
#include <Update.h>
#include <Preferences.h>
#include "index.h"
#include "update.h"

#define PREFERENCES_NAME "esp32-api"
#define PREFERENCES_GPIOS "esp32-api"
Preferences preferences;

void ServerHandler::begin()
{
    preferences.begin(PREFERENCES_NAME, false);

    if (preferences.getBool("gpios_are_init")) {
        size_t schLen = preferences.getBytes(PREFERENCES_GPIOS, NULL, NULL);
        char buffer[schLen]; // prepare a buffer for the data
        preferences.getBytes(PREFERENCES_GPIOS, buffer, schLen);
        memcpy(gpios, buffer, schLen);
    } else {
        Serial.println("gpios are not init");
        GpioFlash tmpGpios[]  = {
            {13, "Pin 13", OUTPUT, 0},
            {17, "Pin 17", OUTPUT, 0},
            {21, "Pin 21", OUTPUT, 0},
            {22, "Pin 22", OUTPUT, 0},
            {25, "Pin 25", OUTPUT, 0},
            {26, "Pin 26", OUTPUT, 0},
            {27, "Pin 27", OUTPUT, 0},
            {32, "Pin 32", OUTPUT, 0},
            {33, "Pin 33", OUTPUT, 0}
        };
        for (int i = 0; i < 9; i++) {
            gpios[i] = tmpGpios[i];
        }
        preferences.putBytes(PREFERENCES_GPIOS, &gpios, sizeof(gpios));
        preferences.putBool("gpios_are_init", true);  
    }
    preferences.end();

    server.on("/", [this]() { handleRoot(); });
    server.on("/clear/settings", [this]() { handleClearSettings(); });
    server.on("/gpios", [this]() { getGpios(); });
    server.on("/gpios/available", [this]() { handleAvailableGpios(); });
    server.on("/digital/{}/{}", [this]() { handleSetGpioState(); });
    server.on("/digital/{}", [this]() { handleGetGpioState(); });
    server.on("/update", [this]() { handleUpdate(); });
    server.on("/gpio/{}/delete", [this]() { handleGpioRemove(); });
    server.on("/gpio", HTTP_POST, [this]() { handleGpioEdit(); });
    server.on("/gpio/new", HTTP_POST, [this]() { handleGpioNew(); });
    server.on("/install", HTTP_POST, [this]() { handleUpload(); }, [this]() { install(); });
    server.onNotFound([this]() {handleNotFound(); });

    server.begin();

    // init all gpios
    initGpios();
}

void  ServerHandler::initGpios() 
{
    for (GpioFlash& gpio : gpios) {
        pinMode(gpio.pin, gpio.mode);
        digitalWrite(gpio.pin, gpio.state);
    }
}

int ServerHandler::firstEmptyGpioSlot() {
    const int count = sizeof(gpios)/sizeof(*gpios);
    for (int i=1;i<count;i++) {
        if (!gpios[i].pin) {
            return i;
        }
    }
    return count;
}

void ServerHandler::getGpios() 
{
    StaticJsonDocument<(2*sizeof(gpios))> doc;
    for (GpioFlash& gpio : gpios) {
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
    return;
}

void ServerHandler::handleNotFound()
{
    server.send(404, "text/plain", "Not found");
}

void ServerHandler::handleClearSettings()
{
    preferences.begin(PREFERENCES_NAME, false);
    preferences.clear();
    preferences.end();
    server.send(200, "text/plain", "Settings clear");
}

void ServerHandler::handleRoot()
{
    server.sendHeader("Connection", "close");
    String s = MAIN_page;
    server.send(200, "text/html", s);
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

    for (GpioFlash& gpio : gpios)
    {
        if (gpio.pin == doc["pin"].as<int>())
        {
            bool hasChanged = false;
            const int newPin = doc["settings"]["pin"].as<int>();
            if (newPin && gpio.pin != newPin) {
                gpio.pin = newPin;
                hasChanged = true;
            }
            const char* newLabel = doc["settings"]["label"].as<char*>();
            if (newLabel && strcmp(gpio.label, newLabel) != 0) {
                strcpy(gpio.label, newLabel);
                hasChanged = true;
            }
            const int newMode = doc["settings"]["mode"].as<int>();
            if (newMode && gpio.mode != newMode) {
                gpio.mode = newMode;
                hasChanged = true;
            }
            if (hasChanged) {
                pinMode(gpio.pin, gpio.mode);
                gpio.state = digitalRead(gpio.pin);
                saveGpios();
            }
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
        }
    }
    server.send(404, "text/plain", "Not found");
}

void ServerHandler::handleGpioNew()
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
    const int pin = doc["settings"]["pin"].as<int>();
    const char* label = doc["settings"]["label"].as<char*>();
    const int mode = doc["settings"]["mode"].as<int>();
    if (pin && label && mode) {
        GpioFlash newGpio = {};
        newGpio.pin = pin;
        strcpy(newGpio.label, label);
        newGpio.mode = mode;
        gpios[firstEmptyGpioSlot()] = newGpio;
        pinMode(pin, mode);
        newGpio.state = digitalRead(pin);
        saveGpios();
        server.send(200, "text/json", server.arg(0));
        return;
    }
    server.send(404, "text/plain", "Missing parameters");
}

void ServerHandler::handleGpioRemove() 
{
    const int pin = atoi(server.pathArg(0).c_str());
    const int count = sizeof(gpios)/sizeof(*gpios);
    for (int i=0;i<count;i++) {
        if (gpios[i].pin == pin) {
            gpios[i] = {};
            server.send(200, "text/plain", "done");
            return;
        }
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

void ServerHandler::handleUpdate()
{
    server.sendHeader("Connection", "close");
    String s = UPDATE_page;
    server.send(200, "text/html", s);
}

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
    const int currentState = digitalRead(pin);
    if (server.pathArg(1) && server.pathArg(1) != "")
    {
        const int newState = atoi(server.pathArg(1).c_str());
        if (newState != currentState) {
            setGpioState(pin, newState);
        }
    }
    handleGetGpioState();
}

void ServerHandler::setGpioState(int pin, int value) {
    for (GpioFlash& gpio : gpios)
    {
        if (gpio.pin == pin && value != gpio.state)
        {
            gpio.state = value;
            digitalWrite(pin, value);
            saveGpios();
        }
    }
}

void ServerHandler::saveGpios() {
    preferences.begin(PREFERENCES_NAME, false);
    preferences.putBytes(PREFERENCES_GPIOS, &gpios, sizeof(gpios));
    preferences.end();
}