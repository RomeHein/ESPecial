#pragma once

#ifndef MqttHandler_h
#define MqttHandler_h
#include <Arduino.h>
#include "PreferenceHandler.h"
#include <WiFiClient.h>
#include <PubSubClient.h>

typedef struct
{
    char debug[TOPIC_MAX_SIZE];
    char gpio[TOPIC_MAX_SIZE];
    char automation[TOPIC_MAX_SIZE];
    char config[TOPIC_MAX_SIZE];
 }  MqttTopics;

class MqttHandler
{
private:  
    unsigned long lastSend = 0;
    MqttTopics topics;
    PreferenceHandler &preference;
    WiFiClient &client;
    PubSubClient* mqtt_client;
    bool isInit = false;
    void handleNewMessages(int numNewMessages);
    void connect();
    void callback(char* topic, byte* payload, unsigned int length);
public:
    ~MqttHandler() { delete mqtt_client; };
    MqttHandler(PreferenceHandler& preference, WiFiClient &client) : preference(preference), client(client) {};
    int automationsQueued[MAX_AUTOMATIONS_NUMBER] = {};
    void begin();
    void handle();
    void disconnect();
    void publish(int pin);
    void publishConfig();
};
#endif