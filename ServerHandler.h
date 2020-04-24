#pragma once

#ifndef ServerHandler_h
#define ServerHandler_h
#include <Arduino.h>
#include <WebServer.h>
#include "PreferenceHandler.h"

class ServerHandler
{
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
    void handleAvailableGpios();
    void handleSetGpioState();
    void handleGetGpioState();
    void handleGpioEdit();
    void handleGpioRemove();
    void handleGpioNew();

    void getActions();
    void handleRunAction();
    void handleActionEdit();
    void handleActionRemove();
    void handleActionNew();

    void getConditions();
    void handleConditionEdit();
    void handleConditionRemove();
    void handleConditionNew();
    
    
public:
    ServerHandler(PreferenceHandler& preference) : preference(preference) {};
    WebServer server = {80};
    void begin();
};
#endif