#pragma once

#ifndef ServerHandler_h
#define ServerHandler_h
#include <Arduino.h>
#include <WebServer.h>
#include "PreferenceHandler.h"

class ServerHandler {
private:
    PreferenceHandler &preference;
    void handleRoot();
    void handleNotFound();
    void handleClearSettings();
    void handleSystemHealth();
    void getSettings();
    void install();
    void handleUpload();
    void handleUpdate();

    void handleMqttEdit();
    void handleMqttRetry();
    void handleTelegramEdit();

    void getGpios();
    void handleScan();
    void handleAvailableGpios();
    void handleSetGpioState();
    void handleGetGpioState();
    void handleGpioEdit();
    void handleGpioRemove();
    void handleGpioNew();

    void getAutomations();
    void handleRunAutomation();
    void handleAutomationEdit();
    void handleAutomationRemove();
    void handleAutomationNew();
    
public:
    ServerHandler(PreferenceHandler& preference) : preference(preference) {};
    WebServer server = {80};
    int automationsQueued[MAX_AUTOMATIONS_NUMBER] = {};
    void begin();
};
#endif