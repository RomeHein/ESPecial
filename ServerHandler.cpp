#include "ServerHandler.h"
#include "PreferenceHandler.h"
#include <Update.h>
#include <ArduinoJson.h>
#include "index.h"

//unmark following line to enable debug mode
#define __debug

void ServerHandler::begin()
{
    #ifdef __debug  
        Serial.println("Server: init");
    #endif
    server.on("/", [this]() { handleRoot(); });
    server.onNotFound([this]() {handleNotFound(); });
    server.on("/clear/settings", [this]() { handleClearSettings(); });
    server.on("/health", [this]() { handleSystemHealth(); });
    server.on("/settings", [this]() { getSettings(); });
    server.on("/install", HTTP_POST, [this]() { handleUpload(); }, [this]() { install(); });
    server.on("/mqtt", HTTP_POST, [this]() { handleMqttEdit(); });
    server.on("/mqtt/retry", [this]() { handleMqttRetry(); });
    server.on("/telegram", HTTP_POST, [this]() { handleTelegramEdit(); });

    // Gpio related endpoints
    server.on("/gpio/{}/value/{}", [this]() { handleSetGpioState(); });
    server.on("/gpio/{}/value", [this]() { handleGetGpioState(); });
    server.on("/gpio/{}/scan", [this]() { handleScan(); });
    server.on("/gpios/available", [this]() { handleAvailableGpios(); });
    server.on("/gpios", [this]() { getGpios(); });
    server.on("/gpio/{}", HTTP_DELETE,[this]() { handleGpioRemove(); });
    server.on("/gpio", HTTP_PUT, [this]() { handleGpioEdit(); });
    server.on("/gpio", HTTP_POST, [this]() { handleGpioNew(); });
    server.on("/gpios", [this]() { getGpios(); });
    server.on("/gpio/{}", HTTP_DELETE,[this]() { handleGpioRemove(); });
    server.on("/gpio", HTTP_PUT, [this]() { handleGpioEdit(); });
    server.on("/gpio", HTTP_POST, [this]() { handleGpioNew(); });

    // Automation related endpoints
    server.on("/automations", [this]() { getAutomations(); });
    server.on("/automation/{}", HTTP_DELETE,[this]() { handleAutomationRemove(); });
    server.on("/automation/run/{}", [this]() { handleRunAutomation(); });
    server.on("/automation", HTTP_PUT, [this]() { handleAutomationEdit(); });
    server.on("/automation", HTTP_POST, [this]() { handleAutomationNew(); });

    server.begin();
}

// Main

void ServerHandler::handleRoot()
{
    server.send(200, "text/html", MAIN_page);
}

void ServerHandler::handleNotFound()
{
    #ifdef __debug  
        Serial.println("Server: page not found");
    #endif
    server.send(404, "text/plain", "Not found");
}

void ServerHandler::handleClearSettings()
{
    preference.clear();
    server.send(200, "text/plain", "Settings clear");
}

void ServerHandler::handleSystemHealth()
{
    const size_t capacity = JSON_OBJECT_SIZE(3) + 10;
    StaticJsonDocument<(capacity)> doc;
    doc["api"] = preference.health.api;
    doc["telegram"] = preference.health.telegram;
    doc["mqtt"] = preference.health.mqtt;
    String output;
    serializeJson(doc, output);
    server.send(200, "text/json", output);
    return;
}

// Settings

void ServerHandler::getSettings() {
    const size_t capacity = JSON_OBJECT_SIZE(8) + JSON_ARRAY_SIZE(MAX_TELEGRAM_USERS_NUMBER) + 300;
    StaticJsonDocument<(capacity)> doc;
    JsonObject telegram = doc.createNestedObject("telegram");
    telegram["active"] = preference.telegram.active;
    telegram["token"] = preference.telegram.token;
    telegram["users"] = preference.telegram.token;
    JsonArray users = telegram.createNestedArray("users");
    for (int userId: preference.telegram.users) {
        if (userId) users.add(userId);
    }
    JsonObject mqtt = doc.createNestedObject("mqtt");
    mqtt["active"] = preference.mqtt.active;
    mqtt["fn"] = preference.mqtt.fn;
    mqtt["host"] = preference.mqtt.host;
    mqtt["port"] = preference.mqtt.port;
    mqtt["user"] = preference.mqtt.user;
    mqtt["password"] = preference.mqtt.password;
    mqtt["topic"] = preference.mqtt.topic;
    JsonObject general = doc.createNestedObject("general");
    general["maxAutomations"] = MAX_AUTOMATIONS_NUMBER;
    general["maxConditions"] = MAX_AUTOMATIONS_CONDITIONS_NUMBER;
    general["maxActions"] = MAX_AUTOMATION_ACTION_NUMBER;
    general["maxChannels"] = MAX_DIGITALS_CHANNEL;
    String output;
    serializeJson(doc, output);
    server.send(200, "text/json", output);
    return;
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

void ServerHandler::handleMqttEdit () {
    const size_t capacity = JSON_OBJECT_SIZE(6) + 300;
    DynamicJsonDocument doc(capacity);

    DeserializationError error = deserializeJson(doc, server.arg(0));
    if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.c_str());
        return;
    }
    const int active = doc["active"].as<int>();
    const char* fn = doc["fn"].as<char*>();
    const char* host = doc["host"].as<char*>();
    const int port = doc["port"].as<int>();
    const char* user = doc["user"].as<char*>();
    const char* password = doc["password"].as<char*>();
    const char* topic = doc["topic"].as<char*>();
    if (fn && host && port && user && password && topic) {
        preference.editMqtt(active,fn,host,port,user,password,topic);
        server.send(200, "text/json", server.arg(0));
        return;
    }
    server.send(404, "text/plain", "Missing parameters");
}

void ServerHandler::handleMqttRetry() {
    preference.health.mqtt = 0;
    server.send(200, "text/plain", "Retrying");
}

void ServerHandler::handleTelegramEdit () {
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
    int users[MAX_TELEGRAM_USERS_NUMBER] = {};
    int i = 0;
    for(int userId: doc["users"].as<JsonArray>()) {
        users[i] = userId;
        i++;
    }
    if (token) {
        preference.editTelegram(token,users,active);
        server.send(200, "text/json", server.arg(0));
        return;
    }
    server.send(404, "text/plain", "Missing parameters");
}

// Gpio hanlding

void ServerHandler::getGpios() 
{
    server.send(200, "text/json", preference.getGpiosJson());
    return;
}

void ServerHandler::handleGpioEdit()
{
    DynamicJsonDocument doc(GPIO_JSON_CAPACITY);
    DeserializationError error = deserializeJson(doc, server.arg(0));
    if (error) {
        #ifdef __debug
            Serial.print(F("Server: deserializeJson() failed: "));
            Serial.println(error.c_str());
        #endif
        return;
    }
    if (preference.gpios[doc["pin"].as<int>()].pin) {
        server.send(200, "text/json", preference.editGpio(doc["pin"].as<int>(), doc["settings"]["pin"].as<int>(), doc["settings"]["label"].as<char*>(), doc["settings"]["mode"].as<int>(),doc["settings"]["sclpin"].as<int>(),doc["settings"]["frequency"].as<int>(),doc["settings"]["resolution"].as<int>(),doc["settings"]["channel"].as<int>(), doc["settings"]["save"].as<int>()));
        return;
    }
    server.send(404, "text/plain", "Not found");
}

void ServerHandler::handleScan() {
    const int pin = atoi(server.pathArg(0).c_str());
    server.send(200, "text/json", preference.scan(preference.gpios[pin]));
}

void ServerHandler::handleGpioNew()
{
    DynamicJsonDocument doc(GPIO_JSON_CAPACITY);

    DeserializationError error = deserializeJson(doc, server.arg(0));
    if (error) {
        #ifdef __debug
            Serial.print(F("Server: deserializeJson() failed: "));
            Serial.println(error.c_str());
        #endif
        return;
    }
    const int pin = doc["settings"]["pin"].as<int>();
    const char* label = doc["settings"]["label"].as<char*>();
    const int mode = doc["settings"]["mode"].as<int>();
    const int sclpin = doc["settings"]["sclpin"].as<int>();
    const int frequency = doc["settings"]["frequency"].as<int>();
    const int resolution = doc["settings"]["resolution"].as<int>();
    const int channel = doc["settings"]["channel"].as<int>();
    const int save = doc["settings"]["save"].as<int>();
    if (pin && label && mode) {
        server.send(200, "text/json", preference.addGpio(pin,label,mode,sclpin,frequency,resolution,channel,save));
        return;
    }
    server.send(404, "text/plain", "Missing parameters");
}

void ServerHandler::handleGpioRemove() 
{
    bool removed = preference.removeGpio(atoi(server.pathArg(0).c_str()));
    if (removed){
        server.send(200, "text/plain", "Done");
        return;
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
    int state;
    GpioFlash& gpio = preference.gpios[pin];
    if (gpio.pin == pin) {
        if (gpio.mode>0) {
            state = digitalRead(pin);
        } else {
            state = ledcRead(gpio.channel);
        }
    }
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
        #ifdef __debug
            Serial.printf("Server: handle set gpio %i state %i\n",pin, newState);
        #endif
        // We set the persist flag to false, to allow the mainloop to pick up new changes and react accordingly
        preference.setGpioState(pin, newState);
    }
    handleGetGpioState();
}

// automationq

void ServerHandler::getAutomations() 
{
    server.send(200, "text/json", preference.getAutomationsJson());
    return;
}

void ServerHandler::handleRunAutomation() {
    const int id = atoi(server.pathArg(0).c_str());
    // queue automation id, to be picked up by esp32.ino script
    for (int i=0; i<MAX_AUTOMATIONS_NUMBER; i++) {
        if (automationsQueued[i] == 0) {
            automationsQueued[i] = id;
            break;
        }
    }
    server.send(200, "text/plain", "Done");
}
void ServerHandler::handleAutomationEdit()
{
    DynamicJsonDocument doc(AUTOMATION_JSON_CAPACITY);
    DeserializationError error = deserializeJson(doc, server.arg(0));
    if (error) {
        #ifdef __debug
            Serial.print(F("Server: deserializeJson() failed: "));
            Serial.println(error.c_str());
        #endif
        return;
    }

    for (AutomationFlash& a : preference.automations)
    {
        if (a.id == doc["id"].as<int>())
        {
            int16_t conditions[MAX_AUTOMATIONS_CONDITIONS_NUMBER][4] = {};
            int j = 0;
            for(JsonArray condition: doc["settings"]["conditions"].as<JsonArray>()) {
                int16_t conditionToFlash[4];
                for (int k = 0; k<4; k++) {
                    conditionToFlash[k] = condition[k];
                }
                memcpy(conditions[j], conditionToFlash, sizeof(conditionToFlash));
                j++;
            }
            char actions[MAX_AUTOMATION_ACTION_NUMBER][4][100] = {};
            int l = 0;
            for(JsonArray action: doc["settings"]["actions"].as<JsonArray>()) {
                char actionToFlash[4][100];
                for (int k = 0; k<4; k++) {
                    strcpy(actionToFlash[k], action[k].as<char*>());
                }
                memcpy(actions[l], actionToFlash, sizeof(actionToFlash));
                l++;
            }
            server.send(200, "text/json", preference.editAutomation(a, doc["settings"]["label"].as<char*>(),doc["settings"]["autoRun"].as<int>(),conditions,actions,doc["settings"]["loopCount"].as<int>(),doc["settings"]["debounceDelay"].as<int>(), doc["settings"]["nextAutomationId"].as<int>()));
            return;
        }
    }
    server.send(404, "text/plain", "Not found");
}

void ServerHandler::handleAutomationNew()
{
    DynamicJsonDocument doc(AUTOMATION_JSON_CAPACITY);
    DeserializationError error = deserializeJson(doc, server.arg(0));
    if (error) {
        #ifdef __debug
            Serial.print(F("Server: deserializeJson() failed: "));
            Serial.println(error.c_str());
        #endif
        return;
    }
    const char* label = doc["settings"]["label"].as<char*>();
    int16_t conditions[MAX_AUTOMATIONS_CONDITIONS_NUMBER][4] = {};
    int j = 0;
    for(JsonArray condition: doc["settings"]["conditions"].as<JsonArray>()) {
        int16_t conditionToFlash[4];
        for (int k = 0; k<4; k++) {
            conditionToFlash[k] = condition[k];
        }
        memcpy(conditions[j], conditionToFlash, sizeof(conditionToFlash));
        j++;
    }
    char actions[MAX_AUTOMATION_ACTION_NUMBER][4][100] = {};
    int l = 0;
    for(JsonArray action: doc["settings"]["actions"].as<JsonArray>()) {
        char actionToFlash[4][100];
        for (int k = 0; k<4; k++) {
            strcpy(actionToFlash[k], action[k].as<char*>());
        }
        memcpy(actions[l], actionToFlash, sizeof(actionToFlash));
        l++;
    }
    const int autoRun = doc["settings"]["autoRun"].as<int>();
    const int loopCount = doc["settings"]["loopCount"].as<int>();
    const int debounceDelay = doc["settings"]["debounceDelay"].as<int>();
    const int nextAutomationId = doc["settings"]["nextAutomationId"].as<int>();
    if (label) {
        server.send(200, "text/json", preference.addAutomation(label,autoRun, conditions, actions , loopCount,debounceDelay, nextAutomationId));
        return;
    }
    server.send(404, "text/plain", "Missing parameters");
}

void ServerHandler::handleAutomationRemove() 
{
    bool removed = preference.removeAutomation(atoi(server.pathArg(0).c_str()));
    if (removed){
        server.send(200, "text/plain", "Done");
        return;
    }
    server.send(404, "text/plain", "Not found");
}