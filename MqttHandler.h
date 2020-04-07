#pragma once

#ifndef MqttHandler_h
#define MqttHandler_h
#include <Arduino.h>
#include "PreferenceHandler.h"
#include <WiFiClient.h>
#include <PubSubClient.h>  

//unmark following line to enable debug mode
#define _debug

#define RETRY_ATTEMPT 5

typedef struct
{
    char debug[256];
    char gpios[256];
 }  MqttTopic;

class MqttHandler
{
private:  
    unsigned long lastSend = 0;
    MqttTopic topic;
    PreferenceHandler &preference;
    WiFiClient &client;
    PubSubClient* mqtt_client;
    bool isInit = false;
    void handleNewMessages(int numNewMessages);
    void connect();
    void callback(char* topic, byte* payload, unsigned int length);
    void config();
public:
    ~MqttHandler() { delete mqtt_client; };
    MqttHandler(PreferenceHandler& preference, WiFiClient &client) : preference(preference), client(client) {};
    void begin();
    void handle();
    void disconnect();
    void publish();
};
#endif