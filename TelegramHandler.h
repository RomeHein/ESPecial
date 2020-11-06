#pragma once

#ifndef TelegramHandler_h
#define TelegramHandler_h
#include <Arduino.h>
#include <UniversalTelegramBot.h>
#include "PreferenceHandler.h"
#include <WiFiClientSecure.h>

typedef struct
{
    char text[200];
    bool sendWithPicture = false;
}  TelegramMessageQueue;

class TelegramHandler
{
private:
    bool isInit = false;
    TelegramMessageQueue messagesQueue[MAX_QUEUED_MESSAGE_NUMBER] = {};
    int lastMessageQueuedPosition = 0;
    UniversalTelegramBot* bot;
    PreferenceHandler &preference;
    WiFiClientSecure &client;
    void handleNewMessages(int numNewMessages);
    String generateButtonFormat(GpioFlash& gpio);
    String generateButtonFormat(AutomationFlash& a);
    String generateInlineKeyboardsForGpios(bool inputMode = false);
    String generateInlineKeyboardsForAutomations();
    void sendPictureFromCameraToChat(int chat_id);
public:
    TelegramHandler(PreferenceHandler& preference, WiFiClientSecure &client) : preference(preference), client(client) {};
    void handle();
    void begin();
    void queueMessage(const char* message, bool withPicture);
    void sendQueuedMessages();
    int automationsQueued[MAX_AUTOMATIONS_NUMBER] = {};
    ~TelegramHandler() { delete bot; }
};
#endif