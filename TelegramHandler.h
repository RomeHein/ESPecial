#pragma once

#ifndef TelegramHandler_h
#define TelegramHandler_h
#include <Arduino.h>
#include <UniversalTelegramBot.h>
#include "PreferenceHandler.h"

class TelegramHandler
{
private:
    UniversalTelegramBot* bot;
    PreferenceHandler &preference;
    void handleNewMessages(int numNewMessages);
    String generateInlineKeyboards();
public:
    TelegramHandler(PreferenceHandler& preference) : preference(preference) {};
    ~TelegramHandler() {
        delete bot;
    }
    void handle();
};
#endif