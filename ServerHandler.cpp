#include "ServerHandler.h"
#include "PreferenceHandler.h"
#include <AsyncJson.h>
#include <Update.h>
#include <SPIFFS.h>

//unmark following line to enable debug mode
#define __debug

const char *idParamName = "id";
const char *pinParamName = "pin";
const char *valueParamName = "value";

void ServerHandler::begin()
{
    if(!SPIFFS.begin(true)){
        Serial.println("Server: An Error has occurred while mounting SPIFFS");
        return;
    }
    #ifdef __debug  
        Serial.println("Server: init");
    #endif
    server.on("/",HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(SPIFFS, "/index.html", "text/html");
    });
    server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(SPIFFS, "/style.css", "text/css");
    });
    server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(SPIFFS, "/script.js", "text/js");
    });
    server.onNotFound([](AsyncWebServerRequest *request) {
        request->send(404, "text/plain", "Not found");
    });
    
    // server.on("/install", HTTP_POST, [this](AsyncWebServerRequest *request) { handleUpload(request); }, [this](AsyncWebServerRequest *request) { install(request); });
    server.on("/clear/settings", HTTP_GET, [this](AsyncWebServerRequest *request) { handleClearSettings(request); });
    server.on("/health", HTTP_GET, [this](AsyncWebServerRequest *request) { handleSystemHealth(request); });
    server.on("/settings", HTTP_GET, [this](AsyncWebServerRequest *request) { getSettings(request); });
    server.on("/restart", HTTP_GET, [this](AsyncWebServerRequest *request) { handleRestart(request); });
    server.on("/mqtt/retry",HTTP_GET, [this](AsyncWebServerRequest *request) { handleMqttRetry(request); });

    AsyncCallbackJsonWebHandler* editMqttHandler = new AsyncCallbackJsonWebHandler("/gpio",[this](AsyncWebServerRequest *request,JsonVariant &json) { handleMqttEdit(request,json); });
    editMqttHandler->setMethod(HTTP_POST);
    AsyncCallbackJsonWebHandler* editTelegramHandler = new AsyncCallbackJsonWebHandler("/gpio",[this](AsyncWebServerRequest *request,JsonVariant &json) { handleTelegramEdit(request,json); });
    editTelegramHandler->setMethod(HTTP_POST);
    server.addHandler(editMqttHandler);
    server.addHandler(editTelegramHandler);

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

    // TODO: implement rewrite function to get back to restful api
    // server.addRewrite(new OneParamRewrite("/gpio/{pin}/value/{value}", "/gpio/value/set?p={pin}"));
    // server.addRewrite(new OneParamRewrite("/gpio/{pin}/value", "/gpio/value/get?p={pin}"));
    // server.addRewrite(new OneParamRewrite("/gpio/{pin}", "/gpio?p={pin}"));
    // server.addRewrite(new OneParamRewrite("/gpio/{pin}/scan", "/gpio/scan?p={pin}"));
    // server.addRewrite(new OneParamRewrite("/automation/{id}", "/automation?id={id}"));
    // server.addRewrite(new OneParamRewrite("/automation/run/{id}", "/automation/run?id={id}"));

    server.begin();
}

// Main
void ServerHandler::handleClearSettings(AsyncWebServerRequest *request)
{
    preference.clear();
    request->send(200, "text/plain", "Settings clear");
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
    ESP.restart();
}

// Settings

void ServerHandler::getSettings(AsyncWebServerRequest *request) {
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
    general["maxTextMessage"] = MAX_MESSAGE_TEXT_SIZE;
    String output;
    serializeJson(doc, output);
    request->send(200, "text/json", output);
    return;
}

// void ServerHandler::handleUpload(AsyncWebServerRequest *request)
// {
//     request->sendHeader("Connection", "close");
//     request->send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
//     ESP.restart(); 
// }

// void ServerHandler::install(AsyncWebServerRequest *request)
// {
//     HTTPUpload& upload = request->upload();
//     if (upload.status == UPLOAD_FILE_START) {
//         Serial.printf("Update: %s\n", upload.filename.c_str());
//         if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
//             Update.printError(Serial);
//         }
//     } else if (upload.status == UPLOAD_FILE_WRITE) {
//         /* flashing firmware to ESP*/
//         if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
//             Update.printError(Serial);
//         }
//     } else if (upload.status == UPLOAD_FILE_END) {
//         if (Update.end(true)) {
//             Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
//         } else {
//             Update.printError(Serial);
//         }
//     } 
// }

void ServerHandler::handleMqttEdit (AsyncWebServerRequest *request,JsonVariant &json) {
    JsonObject doc = json.as<JsonObject>();
    const int active = doc["active"].as<int>();
    const char* fn = doc["fn"].as<char*>();
    const char* host = doc["host"].as<char*>();
    const int port = doc["port"].as<int>();
    const char* user = doc["user"].as<char*>();
    const char* password = doc["password"].as<char*>();
    const char* topic = doc["topic"].as<char*>();
    if (fn && host && port && user && password && topic) {
        preference.editMqtt(active,fn,host,port,user,password,topic);
        request->send(200, "text/json", request->pathArg(0));
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
        request->send(200, "text/json", request->pathArg(0));
        return;
    }
    request->send(404, "text/plain", "Missing parameters");
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
        request->send(200, "text/json", preference.addGpio(pin,label,mode,sclpin,frequency,resolution,channel,save, invert));
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
    const size_t capacity = JSON_ARRAY_SIZE(1) + GPIO_PIN_COUNT*(JSON_OBJECT_SIZE(2)+20);
    StaticJsonDocument<capacity> doc;
    for (int i = 0; i<GPIO_PIN_COUNT; i++) {
        JsonObject object = doc.createNestedObject();
        object["pin"] = i;
        object["inputOnly"] = !GPIO_IS_VALID_OUTPUT_GPIO(i);
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
                Serial.printf("Server: handle set gpio %i state %i\n",pin, state);
            #endif
            // We set the persist flag to false, to allow the mainloop to pick up new changes and react accordingly
            preference.setGpioState(pin, state);
        }
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