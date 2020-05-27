#pragma once

#ifndef ServerHandler_h
#define ServerHandler_h
#include <Arduino.h>
#include <ArduinoJson.h>
#include "ESPAsyncWebServer.h"
#include "PreferenceHandler.h"

class ServerHandler {
private:
    PreferenceHandler &preference;
    void handleClearSettings(AsyncWebServerRequest *request);
    void handleSystemHealth(AsyncWebServerRequest *request);
    void getSettings(AsyncWebServerRequest *request);
    void install(AsyncWebServerRequest *request);
    void handleRestart(AsyncWebServerRequest *request);
    void handleUpload(AsyncWebServerRequest *request);
    void handleUpdate(AsyncWebServerRequest *request);

    void handleMqttEdit(AsyncWebServerRequest *request,JsonVariant &json);
    void handleMqttRetry(AsyncWebServerRequest *request);
    void handleTelegramEdit(AsyncWebServerRequest *request,JsonVariant &json);

    void getGpios(AsyncWebServerRequest *request);
    void handleAvailableGpios(AsyncWebServerRequest *request);
    void handleGpioState(AsyncWebServerRequest *request);
    void handleGpioEdit(AsyncWebServerRequest *request,JsonVariant &json);
    void handleGpioRemove(AsyncWebServerRequest *request);
    void handleGpioNew(AsyncWebServerRequest *request,JsonVariant &json);

    void getSlaves(AsyncWebServerRequest *request);
    void handleScan(AsyncWebServerRequest *request);
    void handleSetSlaveData(AsyncWebServerRequest *request);
    void handleGetSlaveData(AsyncWebServerRequest *request);
    void handleSlaveEdit(AsyncWebServerRequest *request,JsonVariant &json);
    void handleSlaveRemove(AsyncWebServerRequest *request);
    void handleSlaveNew(AsyncWebServerRequest *request,JsonVariant &json);

    void getAutomations(AsyncWebServerRequest *request);
    void handleRunAutomation(AsyncWebServerRequest *request);
    void handleAutomationEdit(AsyncWebServerRequest *request,JsonVariant &json);
    void handleAutomationRemove(AsyncWebServerRequest *request);
    void handleAutomationNew(AsyncWebServerRequest *request,JsonVariant &json);
    
public:
    ServerHandler(PreferenceHandler& preference) : preference(preference) {};
    AsyncWebServer server = {80};
    int automationsQueued[MAX_AUTOMATIONS_NUMBER] = {};
    void begin();
};
#endif