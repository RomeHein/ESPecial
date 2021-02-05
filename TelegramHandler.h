#pragma once

#ifndef TelegramHandler_h
#define TelegramHandler_h
#include <Arduino.h>
#include <UniversalTelegramBot.h>
#include "PreferenceHandler.h"
#include <WiFiClientSecure.h>

class TelegramHandler
{
private:
    bool isInit = false;
    UniversalTelegramBot* bot;
    PreferenceHandler &preference;
    WiFiClientSecure &client;
    TaskCallback &taskCallback;
    void handleNewMessages(int numNewMessages);
    String generateButtonFormat(GpioFlash& gpio);
    String generateButtonFormat(AutomationFlash& a);
    String generateInlineKeyboardsForGpios(bool inputMode = false);
    String generateInlineKeyboardsForAutomations();
    void sendPictureFromCameraToChat(int chat_id);
public:
    TelegramHandler(PreferenceHandler& preference, WiFiClientSecure &client, TaskCallback taskCallback) : preference(preference), client(client), taskCallback(taskCallback)  {};
    void handle();
    void begin();
    void sendMessage(const char* message, bool withPicture);
    ~TelegramHandler() { delete bot; }
};
#endif