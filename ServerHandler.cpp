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
    server.on("/digital/{}/{}", [this]() { handleSetGpioState(); });
    server.on("/digital/{}", [this]() { handleGetGpioState(); });
    server.on("/gpios/available", [this]() { handleAvailableGpios(); });
    server.on("/gpios", [this]() { getGpios(); });
    server.on("/gpio/{}", HTTP_DELETE,[this]() { handleGpioRemove(); });
    server.on("/gpio", HTTP_PUT, [this]() { handleGpioEdit(); });
    server.on("/gpio", HTTP_POST, [this]() { handleGpioNew(); });
    server.on("/gpios", [this]() { getGpios(); });
    server.on("/gpio/{}", HTTP_DELETE,[this]() { handleGpioRemove(); });
    server.on("/gpio", HTTP_PUT, [this]() { handleGpioEdit(); });
    server.on("/gpio", HTTP_POST, [this]() { handleGpioNew(); });

    // Action related endpoints
    server.on("/actions", [this]() { getActions(); });
    server.on("/action/{}", HTTP_DELETE,[this]() { handleActionRemove(); });
    server.on("/action/run/{}", [this]() { handleRunAction(); });
    server.on("/action", HTTP_PUT, [this]() { handleActionEdit(); });
    server.on("/action", HTTP_POST, [this]() { handleActionNew(); });

    // Condition related endpoints
    server.on("/conditions", [this]() { getConditions(); });
    server.on("/condition/{}", HTTP_DELETE,[this]() { handleConditionRemove(); });
    server.on("/condition", HTTP_PUT, [this]() { handleConditionEdit(); });
    server.on("/condition", HTTP_POST, [this]() { handleConditionNew(); });

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
    const size_t capacity = JSON_OBJECT_SIZE(7) + 300;
    StaticJsonDocument<(capacity)> doc;
    JsonObject telegram = doc.createNestedObject("telegram");
    telegram["active"] = preference.telegram.active;
    telegram["token"] = preference.telegram.token;
    JsonObject mqtt = doc.createNestedObject("mqtt");
    mqtt["active"] = preference.mqtt.active;
    mqtt["fn"] = preference.mqtt.fn;
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
    if (token) {
        preference.editTelegram(token, active);
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

    for (GpioFlash& gpio : preference.gpios)
    {
        if (gpio.pin == doc["pin"].as<int>())
        {
            server.send(200, "text/json", preference.editGpio(gpio, doc["settings"]["pin"].as<int>(), doc["settings"]["label"].as<char*>(), doc["settings"]["mode"].as<int>(), doc["settings"]["save"].as<int>()));
            return;
        }
    }
    server.send(404, "text/plain", "Not found");
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
    const int save = doc["settings"]["save"].as<int>();
    if (pin && label && mode) {
        server.send(200, "text/json", preference.addGpio(pin, label, mode, save));
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

// actions

void ServerHandler::getActions() 
{
    server.send(200, "text/json", preference.getActionsJson());
    return;
}

void ServerHandler::handleRunAction() {

}
void ServerHandler::handleActionEdit()
{
    DynamicJsonDocument doc(ACTION_JSON_CAPACITY);
    DeserializationError error = deserializeJson(doc, server.arg(0));
    if (error) {
        #ifdef __debug
            Serial.print(F("Server: deserializeJson() failed: "));
            Serial.println(error.c_str());
        #endif
        return;
    }

    for (ActionFlash& action : preference.actions)
    {
        if (action.id == doc["id"].as<int>())
        {
            int conditions[MAX_ACTIONS_CONDITIONS_NUMBER] = {};
            int i = 0;
            for(int conditionId: doc["settings"]["conditions"].as<JsonArray>()) {
                conditions[i] = conditionId;
                i++;
            }
            server.send(200, "text/json", preference.editAction(action, doc["settings"]["label"].as<char*>(),doc["settings"]["type"].as<int>(), conditions,doc["settings"]["autoRun"].as<int>(),doc["settings"]["message"].as<char*>(),doc["settings"]["pinC"].as<int>(), doc["settings"]["valueC"].as<int>(),doc["settings"]["loopCount"].as<int>(),doc["settings"]["delay"].as<int>(), doc["settings"]["nextActionId"].as<int>()));
            return;
        }
    }
    server.send(404, "text/plain", "Not found");
}

void ServerHandler::handleActionNew()
{
    DynamicJsonDocument doc(ACTION_JSON_CAPACITY);
    DeserializationError error = deserializeJson(doc, server.arg(0));
    if (error) {
        #ifdef __debug
            Serial.print(F("Server: deserializeJson() failed: "));
            Serial.println(error.c_str());
        #endif
        return;
    }
    const char* label = doc["settings"]["label"].as<char*>();
    const int type = doc["settings"]["type"].as<int>();
    int conditions[MAX_ACTIONS_CONDITIONS_NUMBER] = {};
    int i = 0;
    for(int conditionId: doc["settings"]["conditions"].as<JsonArray>()) {
        conditions[i] = conditionId;
        i++;
    }
    const int autoRun = doc["settings"]["autoRun"].as<int>();
    const char* mes = doc["settings"]["message"].as<char*>();
    const int pinC = doc["settings"]["pinC"].as<int>();
    const int valueC = doc["settings"]["valueC"].as<int>();
    const int loopCount = doc["settings"]["loopCount"].as<int>();
    const int delay = doc["settings"]["delay"].as<int>();
    const int nextActionId = doc["settings"]["nextActionId"].as<int>();
    if (label && type) {
        server.send(200, "text/json", preference.addAction(label, type, conditions, autoRun, mes, pinC, valueC, loopCount, delay, nextActionId));
        return;
    }
    server.send(404, "text/plain", "Missing parameters");
}

void ServerHandler::handleActionRemove() 
{
    bool removed = preference.removeAction(atoi(server.pathArg(0).c_str()));
    if (removed){
        server.send(200, "text/plain", "Done");
        return;
    }
    server.send(404, "text/plain", "Not found");
}

// Conditions

void ServerHandler::getConditions() 
{
    server.send(200, "text/json", preference.getConditionsJson());
    return;
}

void ServerHandler::handleConditionEdit()
{
    DynamicJsonDocument doc(CONDITION_JSON_CAPACITY);
    DeserializationError error = deserializeJson(doc, server.arg(0));
    if (error) {
        #ifdef __debug
            Serial.print(F("Server: deserializeJson() failed: "));
            Serial.println(error.c_str());
        #endif
        return;
    }
    for (ConditionFlash& condition : preference.conditions)
    {
        if (condition.id == doc["id"].as<int>())
        {
            server.send(200, "text/json", preference.editCondition(condition,doc["settings"]["type"].as<int>(),doc["settings"]["label"].as<char*>(), doc["settings"]["pin"].as<int>(),doc["settings"]["value"].as<int>()));
            return;
        }
    }
    server.send(404, "text/plain", "Not found");
}

void ServerHandler::handleConditionNew()
{
    DynamicJsonDocument doc(CONDITION_JSON_CAPACITY);
    DeserializationError error = deserializeJson(doc, server.arg(0));
    if (error) {
        #ifdef __debug
            Serial.print(F("Server: deserializeJson() failed: "));
            Serial.println(error.c_str());
        #endif
        return;
    }
    const char* label = doc["settings"]["label"].as<char*>();
    const int type = doc["settings"]["type"].as<int>();
    const int pin = doc["settings"]["pin"].as<int>();
    const int value = doc["settings"]["value"].as<int>();
    if (label && type && pin) {
        server.send(200, "text/json", preference.addCondition(label, type, pin, value));
        return;
    }
    server.send(404, "text/plain", "Missing parameters");
}

void ServerHandler::handleConditionRemove() 
{
    bool removed = preference.removeCondition(atoi(server.pathArg(0).c_str()));
    if (removed){
        server.send(200, "text/plain", "Done");
        return;
    }
    server.send(404, "text/plain", "Not found");
}