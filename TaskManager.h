#pragma once

#ifndef TaskManager_h
#define TaskManager_h
#include <Arduino.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include "PreferenceHandler.h"
#include "ServerHandler.h"
#include "TelegramHandler.h"
#include "MqttHandler.h"
#include <H4.h>

typedef struct
{
    uint8_t type; // 1: automation, 2: gpio, 3: OTA, 4: ESP (sleep, restart)
    char label[50]; // what the task does. often used in if/else statement to determine what to do
    uint8_t pin; // used for type 2
    int16_t value; // depending on the type, can contain the id of the automation/gpio, the version of the OTA
 }  Task;

 using TaskCallback = void(*)(Task);

class TaskManager
{
private:  
    WiFiClientSecure client;
    WiFiClient clientNotSecure;
    PreferenceHandler &preference;
    ServerHandler *serverhandler;
    TelegramHandler *telegramhandler;
    MqttHandler *mqtthandler;
    void readPins();
    void getFirmwareList();
    void runAutomation(int id);
    void runAutomation(AutomationFlash& automation);
    void unTriggeredEventAutomations(bool onlyTimeConditionned);
public:
    ~TaskManager() {};
    H4 h4 = {115200};
    void begin();
    void executeTask(Task& task);
};
#endif