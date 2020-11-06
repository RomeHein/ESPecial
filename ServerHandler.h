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
    void handleFirmwareList(AsyncWebServerRequest *request);
    void handleSystemHealth(AsyncWebServerRequest *request);
    void getSettings(AsyncWebServerRequest *request);
    void install(AsyncWebServerRequest *request);
    void handleRestart(AsyncWebServerRequest *request);
    void handleUpdate(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final);
    void handleUpdateToVersion(AsyncWebServerRequest *request);
    void handleImportBackup(AsyncWebServerRequest *request, JsonVariant &json);

    void handleMqttEdit(AsyncWebServerRequest *request,JsonVariant &json);
    void handleMqttRetry(AsyncWebServerRequest *request);
    void handleTelegramEdit(AsyncWebServerRequest *request,JsonVariant &json);
    void handleWifiEdit(AsyncWebServerRequest *request,JsonVariant &json);

    void getGpios(AsyncWebServerRequest *request);
    void handleAvailableGpios(AsyncWebServerRequest *request);
    void handleGpioState(AsyncWebServerRequest *request);
    void handleGpioEdit(AsyncWebServerRequest *request,JsonVariant &json);
    void handleGpioRemove(AsyncWebServerRequest *request);
    void handleGpioNew(AsyncWebServerRequest *request,JsonVariant &json);

    void getSlaves(AsyncWebServerRequest *request);
    void handleScan(AsyncWebServerRequest *request);
    void handleSendSlaveCommands(AsyncWebServerRequest *request);
    void handleSlaveEdit(AsyncWebServerRequest *request,JsonVariant &json);
    void handleSlaveRemove(AsyncWebServerRequest *request);
    void handleSlaveNew(AsyncWebServerRequest *request,JsonVariant &json);

    void getAutomations(AsyncWebServerRequest *request);
    void handleRunAutomation(AsyncWebServerRequest *request);
    void handleAutomationEdit(AsyncWebServerRequest *request,JsonVariant &json);
    void handleAutomationRemove(AsyncWebServerRequest *request);
    void handleAutomationNew(AsyncWebServerRequest *request,JsonVariant &json);
    
    void handleCameraInit(AsyncWebServerRequest *request);
    void streamJpg(AsyncWebServerRequest *request);
    void getCameraStatus(AsyncWebServerRequest *request);
    void setCameraVar(AsyncWebServerRequest *request);
    void saveCameraVar(AsyncWebServerRequest *request);
    
public:
    ServerHandler(PreferenceHandler& preference) : preference(preference) {};
    AsyncWebServer server = {80};
    AsyncEventSource events = {"/events"};
    int automationsQueued[MAX_AUTOMATIONS_NUMBER] = {};
    bool shouldRestart = false; // Set this flag in the callbacks to restart ESP in the main loop
    bool shouldReloadFirmwareList = false;
    char shouldOTAFirmwareVersion[10] = "";
    void begin();
};
#endif