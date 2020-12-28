#include "ServerHandler.h"
#include <HTTPClient.h>
#include <AsyncJson.h>
#include <Update.h>
#include <SPIFFS.h>
#include "AsyncResponses.h"

void ServerHandler::begin()
{
    if(!SPIFFS.begin()){
        Serial.println("[SERVER] An Error has occurred while mounting SPIFFS");
        return;
    }
    #ifdef __debug  
        Serial.println("[SERVER] init");
    #endif
    server.on("/",HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(SPIFFS, "/index.html", "text/html");
    });
    server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(SPIFFS, "/style.css", "text/css");
    });
    server.on("/gpio.js", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(SPIFFS, "/gpio.js", "text/js");
    });
    server.on("/camera.js", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(SPIFFS, "/camera.js", "text/js");
    });
    server.on("/automation.js", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(SPIFFS, "/automation.js", "text/js");
    });
    server.on("/settings.js", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(SPIFFS, "/settings.js", "text/js");
    });
    server.onNotFound([](AsyncWebServerRequest *request) {
        request->send(404, "text/plain", "Not found");
    });
    
    server.on("/settings/reset", HTTP_GET, [this](AsyncWebServerRequest *request) { handleClearSettings(request); });
    server.on("/health", HTTP_GET, [this](AsyncWebServerRequest *request) { handleSystemHealth(request); });
    server.on("/settings", HTTP_GET, [this](AsyncWebServerRequest *request) { getSettings(request); });
    server.on("/restart", HTTP_GET, [this](AsyncWebServerRequest *request) { handleRestart(request); });
    server.on("/mqtt/retry",HTTP_GET, [this](AsyncWebServerRequest *request) { handleMqttRetry(request); });
    server.on("/firmware/list",HTTP_GET, [this](AsyncWebServerRequest *request) { handleFirmwareList(request); });
    server.on("/update/version", HTTP_GET, [this](AsyncWebServerRequest *request) { handleUpdateToVersion(request); });
    server.on("/update", HTTP_POST, [this](AsyncWebServerRequest *request) {
        shouldRestart = !Update.hasError();
        AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", shouldRestart?"OK":"FAIL");
        response->addHeader("Connection", "close");
        request->send(response);
    }, [this](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) { handleUpdate(request, filename, index, data, len, final);});

    AsyncCallbackJsonWebHandler* editMqttHandler = new AsyncCallbackJsonWebHandler("/mqtt",[this](AsyncWebServerRequest *request,JsonVariant &json) { handleMqttEdit(request,json); });
    editMqttHandler->setMethod(HTTP_POST);
    AsyncCallbackJsonWebHandler* editTelegramHandler = new AsyncCallbackJsonWebHandler("/telegram",[this](AsyncWebServerRequest *request,JsonVariant &json) { handleTelegramEdit(request,json); });
    editTelegramHandler->setMethod(HTTP_POST);
    AsyncCallbackJsonWebHandler* editWifiHandler = new AsyncCallbackJsonWebHandler("/wifi",[this](AsyncWebServerRequest *request,JsonVariant &json) { handleWifiEdit(request,json); });
    editWifiHandler->setMethod(HTTP_POST);
    AsyncCallbackJsonWebHandler* importBackupHandler = new AsyncCallbackJsonWebHandler("/import/backup",[this](AsyncWebServerRequest *request,JsonVariant &json) { handleImportBackup(request,json); });
    importBackupHandler->setMethod(HTTP_POST);
    server.addHandler(editMqttHandler);
    server.addHandler(editTelegramHandler);
    server.addHandler(editWifiHandler);
    server.addHandler(importBackupHandler);

    // Gpio related endpoints
    server.on("/gpio/value", HTTP_GET, [this](AsyncWebServerRequest *request) { handleGpioState(request); });
    server.on("/gpios/available", HTTP_GET, [this](AsyncWebServerRequest *request) { handleAvailableGpios(request); });
    server.on("/gpios", HTTP_GET, [this](AsyncWebServerRequest *request) { getGpios(request); });
    server.on("/gpio", HTTP_DELETE,[this](AsyncWebServerRequest *request) { handleGpioRemove(request); });

    AsyncCallbackJsonWebHandler* addGpioHandler = new AsyncCallbackJsonWebHandler("/gpio",[this](AsyncWebServerRequest *request,JsonVariant &json) { handleGpioNew(request,json); });
    addGpioHandler->setMethod(HTTP_POST);
    AsyncCallbackJsonWebHandler* editGpioHandler = new AsyncCallbackJsonWebHandler("/gpio",[this](AsyncWebServerRequest *request,JsonVariant &json) { handleGpioEdit(request,json); });
    editGpioHandler->setMethod(HTTP_PUT);
    server.addHandler(addGpioHandler);
    server.addHandler(editGpioHandler);

    // I2c related endpoints
    server.on("/gpio/scan", HTTP_GET, [this](AsyncWebServerRequest *request) { handleScan(request); });
    server.on("/slave/command", HTTP_GET,[this](AsyncWebServerRequest *request) { handleSendSlaveCommands(request); });
    server.on("/slave", HTTP_DELETE,[this](AsyncWebServerRequest *request) { handleSlaveRemove(request); });
    server.on("/slaves", HTTP_GET,[this](AsyncWebServerRequest *request) { getSlaves(request); });

    AsyncCallbackJsonWebHandler* addSlaveHandler = new AsyncCallbackJsonWebHandler("/slave",[this](AsyncWebServerRequest *request,JsonVariant &json) { handleSlaveNew(request,json); });
    addSlaveHandler->setMethod(HTTP_POST);
    AsyncCallbackJsonWebHandler* editSlaveHandler = new AsyncCallbackJsonWebHandler("/slave",[this](AsyncWebServerRequest *request,JsonVariant &json) { handleSlaveEdit(request,json); });
    editSlaveHandler->setMethod(HTTP_PUT);
    server.addHandler(addSlaveHandler);
    server.addHandler(editSlaveHandler);
    
    // Automation related endpoints
    server.on("/automations",HTTP_GET, [this](AsyncWebServerRequest *request) { getAutomations(request); });
    server.on("/automation", HTTP_DELETE,[this](AsyncWebServerRequest *request) { handleAutomationRemove(request); });
    server.on("/automation/run",HTTP_GET, [this](AsyncWebServerRequest *request) { handleRunAutomation(request); });

    AsyncCallbackJsonWebHandler* addAutomationHandler = new AsyncCallbackJsonWebHandler("/automation",[this](AsyncWebServerRequest *request,JsonVariant &json) { handleAutomationNew(request,json); });
    addAutomationHandler->setMethod(HTTP_POST);
    AsyncCallbackJsonWebHandler* editAutomationHandler = new AsyncCallbackJsonWebHandler("/automation",[this](AsyncWebServerRequest *request,JsonVariant &json) { handleAutomationEdit(request,json); });
    editAutomationHandler->setMethod(HTTP_PUT);
    server.addHandler(addAutomationHandler);
    server.addHandler(editAutomationHandler);

    // Camera endpoints
    // server.on("/camera",HTTP_GET, [this](AsyncWebServerRequest *request) { setCameraVar(request); });
    server.on("/camera/init",HTTP_GET, [this](AsyncWebServerRequest *request) { handleCameraInit(request); });
    server.on("/camera/status",HTTP_GET, [this](AsyncWebServerRequest *request) { getCameraStatus(request); });
    server.on("/camera/save",HTTP_GET, [this](AsyncWebServerRequest *request) { saveCameraVar(request); });
    server.on("/camera/stream",HTTP_GET, [this](AsyncWebServerRequest *request) { streamJpg(request); });

    // TODO: implement rewrite function to get back to restful api
    // server.addRewrite(new OneParamRewrite("/gpio/{pin}/value/{value}", "/gpio/value/set?p={pin}"));
    // server.addRewrite(new OneParamRewrite("/gpio/{pin}/value", "/gpio/value/get?p={pin}"));
    // server.addRewrite(new OneParamRewrite("/gpio/{pin}", "/gpio?p={pin}"));
    // server.addRewrite(new OneParamRewrite("/gpio/{pin}/scan", "/gpio/scan?p={pin}"));
    // server.addRewrite(new OneParamRewrite("/automation/{id}", "/automation?id={id}"));
    // server.addRewrite(new OneParamRewrite("/automation/run/{id}", "/automation/run?id={id}"));

    server.addHandler(&events);
    server.begin();

    Update.onProgress([this](size_t prg, size_t sz) {
        Serial.printf("[SERVER] Progress: %d\n", prg);
    });
}

// Main
void ServerHandler::handleClearSettings(AsyncWebServerRequest *request)
{
    preference.clear();
    request->send(200, "text/plain", "Settings clear");
}

// This will ask the main loop to retrieve firmware list from github account.
// Once the list is retrieved, we'll send an event to the client with the list inside
void ServerHandler::handleFirmwareList(AsyncWebServerRequest *request)
{
    shouldReloadFirmwareList = true;
    request->send(200, "text/plain", "Waiting for result");
    return;
}

void ServerHandler::handleSystemHealth(AsyncWebServerRequest *request)
{
    const size_t capacity = JSON_OBJECT_SIZE(3) + 10;
    StaticJsonDocument<(capacity)> doc;
    doc["api"] = preference.health.api;
    doc["telegram"] = preference.health.telegram;
    doc["mqtt"] = preference.health.mqtt;
    String output;
    serializeJson(doc, output);
    request->send(200, "text/json", output);
    return;
}

void ServerHandler::handleRestart(AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "OK");
    shouldRestart = true;
}

// Settings

void ServerHandler::getSettings(AsyncWebServerRequest *request) {
    const size_t capacity = JSON_OBJECT_SIZE(22) + JSON_ARRAY_SIZE(MAX_TELEGRAM_USERS_NUMBER) + 300;
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
    JsonObject wifi = doc.createNestedObject("wifi");
    wifi["apSsid"] = preference.wifi.apSsid;
    wifi["apPsw"] = preference.wifi.apPsw;
    wifi["staEnable"] = preference.wifi.staEnable;
    wifi["staSsid"] = preference.wifi.staSsid;
    wifi["staPsw"] = preference.wifi.staPsw;
    wifi["dns"] = preference.wifi.dns;
    JsonObject general = doc.createNestedObject("general");
    general["maxAutomations"] = MAX_AUTOMATIONS_NUMBER;
    general["maxConditions"] = MAX_AUTOMATIONS_CONDITIONS_NUMBER;
    general["maxActions"] = MAX_AUTOMATION_ACTION_NUMBER;
    general["maxChannels"] = MAX_DIGITALS_CHANNEL;
    general["maxTextMessage"] = MAX_MESSAGE_TEXT_SIZE;
    general["firmwareVersion"] = FIRMWARE_VERSION;
    String output;
    serializeJson(doc, output);
    request->send(200, "text/json", output);
    return;
}

void ServerHandler::handleUpdate(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
    bool gotError = false;
    if(!index){
      Serial.printf("[SERVER] Update Start: %s\n", filename.c_str());
      //Update.runAsync(true);
      if(!Update.begin((ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000)){
        Update.printError(Serial);
        gotError = true;
      }
    }
    if(!Update.hasError()){
      if(Update.write(data, len) != len){
        Update.printError(Serial);
        gotError = true;
      }
    }
    if(final){
      if(Update.end(true)){
        Serial.printf("Update Success: %uB\n", index+len);
      } else {
        Update.printError(Serial);
        gotError = true;
      }
    }
    if (gotError) {
        events.send("Could not update firmware","firmwareUpdateError",millis());
    }
}

void ServerHandler::handleUpdateToVersion(AsyncWebServerRequest *request) {
    if (request->hasParam("v")) {
        strcpy(shouldOTAFirmwareVersion,request->getParam("v")->value().c_str());
    }
    request->send(200, "text/plain", "Downloading");
}

void ServerHandler::handleImportBackup(AsyncWebServerRequest *request, JsonVariant &json) {
    preference.clear();
    JsonObject doc = json.as<JsonObject>();
    JsonObject settings = doc["settings"].as<JsonObject>();
    JsonObject automations = doc["automations"].as<JsonObject>();
    JsonObject salves = doc["slaves"].as<JsonObject>();
    JsonObject gpios = doc["gpios"].as<JsonObject>();
    request->send(200, "text/plain", "ok");
}

void ServerHandler::handleMqttEdit (AsyncWebServerRequest *request,JsonVariant &json) {
    JsonObject doc = json.as<JsonObject>();
    if (doc) {
        preference.editMqtt(doc);
        request->send(200, "text/plain", "ok");
        return;
    }
    request->send(404, "text/plain", "Missing parameters");
}

void ServerHandler::handleMqttRetry(AsyncWebServerRequest *request) {
    preference.health.mqtt = 0;
    request->send(200, "text/plain", "Retrying");
}

void ServerHandler::handleTelegramEdit (AsyncWebServerRequest *request,JsonVariant &json) {
    JsonObject doc = json.as<JsonObject>();
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
        request->send(200, "text/plain", "ok");
        return;
    }
    request->send(404, "text/plain", "Missing parameters");
}

void ServerHandler::handleWifiEdit(AsyncWebServerRequest *request,JsonVariant &json) {
    JsonObject doc = json.as<JsonObject>();
    const char* dns = doc["dns"].as<char*>();
    const char* apSsid = doc["apSsid"].as<char*>();
    const char* apPsw = doc["apPsw"].as<char*>();
    const int staActive = doc["staEnable"].as<int>();
    const char* staSsid = doc["staSsid"].as<char*>();
    const char* staPsw = doc["staPsw"].as<char*>();
    
    if (apSsid || staSsid) {
        preference.editWifi(dns,apSsid,apPsw,staActive,staSsid,staPsw);
        request->send(200, "text/plain", "ok");
        return;
    }
    request->send(404, "text/json", "Missing parameters");
}

// Gpio hanlding

void ServerHandler::getGpios(AsyncWebServerRequest *request) 
{
    request->send(200, "text/json", preference.getGpiosJson());
    return;
}

void ServerHandler::handleGpioEdit(AsyncWebServerRequest *request,JsonVariant &json)
{
    JsonObject doc = json.as<JsonObject>();
    if (preference.gpios[doc["pin"].as<int>()].pin) {
        request->send(200, "text/json", preference.editGpio(doc["pin"].as<int>(), doc["settings"]["pin"].as<int>(), doc["settings"]["label"].as<char*>(), doc["settings"]["mode"].as<int>(),doc["settings"]["sclpin"].as<int>(),doc["settings"]["frequency"].as<int>(),doc["settings"]["resolution"].as<int>(),doc["settings"]["channel"].as<int>(), doc["settings"]["save"].as<int>(), doc["settings"]["invert"].as<int>()));
        return;
    }
    request->send(404, "text/plain", "Not found");
}

void ServerHandler::handleGpioNew(AsyncWebServerRequest *request,JsonVariant &json)
{
    JsonObject doc = json.as<JsonObject>();
    const int pin = doc["settings"]["pin"].as<int>();
    const char* label = doc["settings"]["label"].as<char*>();
    const int mode = doc["settings"]["mode"].as<int>();
    const int sclpin = doc["settings"]["sclpin"].as<int>();
    const int frequency = doc["settings"]["frequency"].as<int>();
    const int resolution = doc["settings"]["resolution"].as<int>();
    const int channel = doc["settings"]["channel"].as<int>();
    const int save = doc["settings"]["save"].as<int>();
    const int invert = doc["settings"]["invert"].as<int>();
    if (pin && label && mode) {
        request->send(200, "text/json", preference.addGpio(pin,label,mode,sclpin,frequency,resolution,channel,save,invert));
        return;
    }
    request->send(404, "text/plain", "Missing parameters");
}

void ServerHandler::handleGpioRemove(AsyncWebServerRequest *request) 
{
    if (request->hasParam(pinParamName)) {
        const int pin = request->getParam(pinParamName)->value().toInt();
        bool removed = preference.removeGpio(pin);
        if (removed){
            request->send(200, "text/plain", "Done");
            return;
        }
    }
    request->send(404, "text/plain", "Not found");
}

void ServerHandler::handleAvailableGpios(AsyncWebServerRequest *request) {
    const size_t capacity = JSON_ARRAY_SIZE(1) + GPIO_PIN_COUNT*(JSON_OBJECT_SIZE(3)+20);
    StaticJsonDocument<capacity> doc;
    for (int i = 0; i<GPIO_PIN_COUNT; i++) {
        JsonObject object = doc.createNestedObject();
        object["inputOnly"] = !GPIO_IS_VALID_OUTPUT_GPIO(i);
        object["adc"] = (i>=32); // Only GPIOs 32 to 39 are working correctly for adc mode when wifi is active.
        object["dac"] = (i==26 || i==25);
        if (i == 4 || i == 2 || i == 12 || i == 13 || i == 14 || i == 15 || i == 27 || i == 32 || i ==33) {
            object["touch"] = true;
        }
    }
    String output;
    serializeJson(doc, output);
    request->send(200, "text/json", output);
}

void ServerHandler::handleGpioState(AsyncWebServerRequest *request)
{
    if (request->hasParam(pinParamName)) {
        int state;
        const int pin = request->getParam(pinParamName)->value().toInt();
        // If we have a value parameter, we want to set the state
        if (request->hasParam(valueParamName))
        {
            state = request->getParam(valueParamName)->value().toInt();
            #ifdef __debug
                Serial.printf("[SERVER] handle set gpio %i state %i\n",pin, state);
            #endif
            // We set the persist flag to false, to allow the mainloop to pick up new changes and react accordingly
            preference.setGpioState(pin, state);
        }
        GpioFlash& gpio = preference.gpios[pin];
        if (gpio.pin == pin) {
            state = preference.getGpioState(pin);
        }
        char json[50];
        snprintf(json, sizeof(json), "{\"pin\":%i,\"state\":%i}", pin, state);
        request->send(200, "text/json", json);
    } else {
        request->send(404, "text/plain", "Not found");
    }
}

// I2c
void ServerHandler::handleScan(AsyncWebServerRequest *request) {
    if (request->hasParam(pinParamName)) {
        const int pin = request->getParam(pinParamName)->value().toInt();
        request->send(200, "text/json", preference.scan(preference.gpios[pin]));
    } else {
        request->send(404, "text/plain", "No pin provided");
    }
}

void ServerHandler::getSlaves(AsyncWebServerRequest *request) {
    request->send(200, "text/json", preference.getI2cSlavesJson());
    return;
}
void ServerHandler::handleSendSlaveCommands(AsyncWebServerRequest *request) {
    if (request->hasParam(idParamName)) {
        const int id = request->getParam(idParamName)->value().toInt();
        request->send(200, "text/json", preference.sendSlaveCommands(id));
    }
    request->send(404, "text/plain", "No Id provided");
}
void ServerHandler::handleSlaveEdit(AsyncWebServerRequest *request,JsonVariant &json) {
    JsonObject doc = json.as<JsonObject>();
    for (I2cSlaveFlash& s : preference.i2cSlaves)
    {
        if (s.id == doc["id"].as<int>())
        {
            uint8_t commands[MAX_I2C_COMMAND_NUMBER] = {};
            JsonArray commandsArray = doc["settings"]["commands"].as<JsonArray>();
            for (int i= 0; i<MAX_I2C_COMMAND_NUMBER; i++) {
                int command = commandsArray[i].as<int>();
                if (command) {
                    commands[i] = commandsArray[i].as<int>();
                }
            }
            request->send(200, "text/json", preference.editSlave(s, doc["settings"]["label"].as<char*>(),commands,doc["settings"]["data"].as<char*>(),doc["settings"]["octetRequest"].as<int>(),doc["settings"]["save"].as<int>()));
            return;
        }
    }
    request->send(404, "text/plain", "Not found");
}
void ServerHandler::handleSlaveRemove(AsyncWebServerRequest *request) {
    if (request->hasParam(idParamName)) {
        const int id = request->getParam(idParamName)->value().toInt();
        bool removed = preference.removeSlave(id);
        if (removed){
            request->send(200, "text/plain", "Done");
            return;
        }
    }
    request->send(404, "text/plain", "Not found");

}
void ServerHandler::handleSlaveNew(AsyncWebServerRequest *request,JsonVariant &json) {
    Serial.println("In slave new");
    JsonObject doc = json.as<JsonObject>();
    const int address = doc["settings"]["address"].as<int>();
    const int mPin = doc["settings"]["mPin"].as<int>();
    const char* label = doc["settings"]["label"].as<char*>();
    uint8_t commands[MAX_I2C_COMMAND_NUMBER] = {};
    JsonArray commandsArray = doc["settings"]["commands"].as<JsonArray>();
    for (int i= 0; i<MAX_I2C_COMMAND_NUMBER; i++) {
        int command = commandsArray[i].as<int>();
        if (command) {
            commands[i] = commandsArray[i].as<int>();
        }
    }
    const char* data = doc["settings"]["data"].as<char*>();
    const int octetRequest = doc["settings"]["octetRequest"].as<int>();
    const int save = doc["settings"]["save"].as<int>();
    if (address && mPin && label) {
        request->send(200, "text/json", preference.addSlave(address,mPin,label,commands,data,octetRequest,save));
        return;
    }
    request->send(404, "text/plain", "Missing parameters");
}

// automations

void ServerHandler::getAutomations(AsyncWebServerRequest *request) 
{
    request->send(200, "text/json", preference.getAutomationsJson());
    return;
}

void ServerHandler::handleRunAutomation(AsyncWebServerRequest *request) {
    if (request->hasParam(idParamName)) {
        const int id = request->getParam(idParamName)->value().toInt();
        // queue automation id, to be picked up by esp32.ino script
        for (int i=0; i<MAX_AUTOMATIONS_NUMBER; i++) {
            if (automationsQueued[i] == 0) {
                automationsQueued[i] = id;
                break;
            }
        }
        request->send(200, "text/plain", "Done");
    }
    request->send(404, "text/plain", "Not found");
}
void ServerHandler::handleAutomationEdit(AsyncWebServerRequest *request,JsonVariant &json)
{
    JsonObject doc = json.as<JsonObject>();
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
            request->send(200, "text/json", preference.editAutomation(a, doc["settings"]["label"].as<char*>(),doc["settings"]["autoRun"].as<int>(),conditions,actions,doc["settings"]["loopCount"].as<int>(),doc["settings"]["debounceDelay"].as<int>()));
            return;
        }
    }
    request->send(404, "text/plain", "Not found");
}

void ServerHandler::handleAutomationNew(AsyncWebServerRequest *request,JsonVariant &json)
{
    JsonObject doc = json.as<JsonObject>();
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
    if (label) {
        request->send(200, "text/json", preference.addAutomation(label,autoRun, conditions, actions , loopCount,debounceDelay));
        return;
    }
    request->send(404, "text/plain", "Missing parameters");
}

void ServerHandler::handleAutomationRemove(AsyncWebServerRequest *request) 
{
    if (request->hasParam(idParamName)) {
        const int id = request->getParam(idParamName)->value().toInt();
        bool removed = preference.removeAutomation(id);
        if (removed){
            request->send(200, "text/plain", "Done");
            return;
        }
    }
    request->send(404, "text/plain", "Not found");
}

void ServerHandler::handleCameraInit(AsyncWebServerRequest *request) 
{
    if (request->hasParam(modelParamName)) {
        const int model = request->getParam(modelParamName)->value().toInt();
        bool initialised = preference.initCamera(model);
        if (initialised){
            preference.createCamera(model);
            request->send(200, "text/json", preference.getGpiosJson());
            return;
        }
    }
    request->send(404, "text/plain", "Not found");
}

void ServerHandler::streamJpg(AsyncWebServerRequest *request){
    AsyncJpegStreamResponse *response = new AsyncJpegStreamResponse();
    if(!response){
        request->send(501);
        return;
    }
    response->addHeader("Access-Control-Allow-Origin", "*");
    request->send(response);
}

void ServerHandler::getCameraStatus(AsyncWebServerRequest *request){
    request->send(200, "text/json",preference.getCameraJson());
}

void ServerHandler::setCameraVar(AsyncWebServerRequest *request){
    if(!request->hasArg("var") || !request->hasArg("val")){
        request->send(404);
        return;
    }
    String var = request->arg("var");
    const char * variable = var.c_str();
    int val = atoi(request->arg("val").c_str());

    if (preference.setCameraVar(variable,val)) {
        AsyncWebServerResponse * response = request->beginResponse(200);
        response->addHeader("Access-Control-Allow-Origin", "*");
        request->send(response);
    } else {
        request->send(404);
    }
}

void ServerHandler::saveCameraVar(AsyncWebServerRequest *request){
    preference.saveCameraSettings();
    request->send(200);
}