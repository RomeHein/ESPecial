#pragma once

#ifndef TelegramHandler_h
#define TelegramHandler_h
#include <Arduino.h>
#include <UniversalTelegramBot.h>
#include "PreferenceHandler.h"
#include <WiFiClientSecure.h>

#define MAX_QUEUED_MESSAGE_NUMBER 5 // Maximum conditions number that can be set in the system

class TelegramHandler
{
private:
    bool isInit = false;
    char messagesQueue [MAX_QUEUED_MESSAGE_NUMBER][200] = {};
    int lastMessageQueuedPosition = 0;
    UniversalTelegramBot* bot;
    PreferenceHandler &preference;
    WiFiClientSecure &client;
    void handleNewMessages(int numNewMessages);
    String generateButtonFormat(GpioFlash& gpio);
    String generateButtonFormat(AutomationFlash& a);
    String generateInlineKeyboardsForGpios(bool inputMode = false);
    String generateInlineKeyboardsForAutomations();
public:
    TelegramHandler(PreferenceHandler& preference, WiFiClientSecure &client) : preference(preference), client(client) {};
    void handle();
    void begin();
    void queueMessage(const char* message);
    void sendQueuedMessages();
    int automationsQueued[MAX_AUTOMATIONS_NUMBER] = {};
    ~TelegramHandler() { delete bot; }
};
#endif