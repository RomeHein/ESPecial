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
    UniversalTelegramBot* bot;
    PreferenceHandler &preference;
    WiFiClientSecure &client;
    void handleNewMessages(int numNewMessages);
    String generateInlineKeyboards();
public:
    TelegramHandler(PreferenceHandler& preference, WiFiClientSecure &client) : preference(preference), client(client) {};
    void handle();
    void begin();
    ~TelegramHandler() { delete bot; }
};
#endif