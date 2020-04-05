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
    void getSettings();
    void getGpios();
    void handleAvailableGpios();
    void handleSetGpioState();
    void handleGetGpioState();
    void handleRoot();
    void handleGpioEdit();
    void handleGpioRemove();
    void handleGpioNew();
    void handleUpload();
    void handleUpdate();
    void handleMqttEdit();
    void handleMqttRetry();
    void handleTelegramEdit();
    void install();
    void handleNotFound();
    void handleClearSettings();
    void handleSystemHealth();
public:
    ServerHandler(PreferenceHandler& preference) : preference(preference) {};
    WebServer server = {80};
    void begin();
};
#endif