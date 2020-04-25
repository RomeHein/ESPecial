#pragma once

#ifndef TelegramHandler_h
#define TelegramHandler_h
#include <Arduino.h>
#include <UniversalTelegramBot.h>
#include "PreferenceHandler.h"
#include <WiFiClientSecure.h>

#define MAX_QUEUED_MESSAGE_NUMBER 20 // Maximum conditions number that can be set in the system

class TelegramHandler
{
private:
    bool isInit = false;
    char messagesQueue [MAX_QUEUED_MESSAGE_NUMBER][150] = {};
    int lastMessageQueuedPosition = 0;
    UniversalTelegramBot* bot;
    PreferenceHandler &preference;
    WiFiClientSecure &client;
    void handleNewMessages(int numNewMessages);
    String generateInlineKeyboards();
public:
    TelegramHandler(PreferenceHandler& preference, WiFiClientSecure &client) : preference(preference), client(client) {};
    void handle();
    void begin();
    void queueMessage(const char* message);
    ~TelegramHandler() { delete bot; }
};
#endif