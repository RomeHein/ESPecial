
#include "MqttHandler.h"
#include "PreferenceHandler.h"

String getId() {
    uint64_t macAddress = ESP.getEfuseMac();
    uint64_t macAddressTrunc = macAddress << 40;
    uint32_t chipID = macAddressTrunc >> 40;
    return String(chipID, HEX);
}

void MqttHandler::begin()
{
    if (!isInit && preference.mqtt.host && preference.mqtt.port && preference.mqtt.topic && preference.mqtt.fn) {
        #ifdef _debug  
            Serial.println("MQTT: init server");
	    #endif
        mqtt_client = new PubSubClient(client);
        mqtt_client->setServer(preference.mqtt.host, preference.mqtt.port);
        mqtt_client->setCallback([this](char* topic, byte* payload, unsigned int length) { callback(topic, payload, length);});

        snprintf(topic.gpios, 512, "%s/%s/gpios", preference.mqtt.topic, preference.mqtt.fn);
        snprintf(topic.debug, 512, "%s/%s/debug", preference.mqtt.topic, preference.mqtt.fn);
        isInit = true;
    }
}
void MqttHandler::handle()
{
    begin();
    if (isInit && preference.mqtt.active && preference.mqtt.user && preference.mqtt.password && preference.health.mqtt >= 0)  {
        //MQTT failed retry to connect
        if (mqtt_client->state() < MQTT_CONNECTED)
        {
            connect();
        }
        //MQTT config problem on MQTT do nothing
        else if (mqtt_client->state() > MQTT_CONNECTED ) return;
        //MQTT connected send status
        else {
            if (millis() > lastSend) {
                //hpStatusChanged(hp.getStatus());
                lastSend = millis();
            }
            mqtt_client->loop();
        }
    }
}

void MqttHandler::connect() {
    // Loop until we're reconnected
    int attempts = 0;
    while (!mqtt_client->connected()) {
        preference.health.mqtt = 0;
        #ifdef _debug  
            Serial.printf("MQTT: Connecxion attempt(%i)\n", attempts);
        #endif
        // Attempt to connect
        mqtt_client->connect(getId().c_str(), preference.mqtt.user, preference.mqtt.password);
        // If state < 0 (MQTT_CONNECTED) => network problem we retry RETRY_ATTEMPT times and then waiting for MQTT_RETRY_INTERVAL_MS and retry reapeatly
        if (mqtt_client->state() < MQTT_CONNECTED) {
            #ifdef _debug  
                Serial.println("MQTT: Failed ");
            #endif
            if (attempts == RETRY_ATTEMPT) {
                // Set health to -1 to block any new attempt. This value can be remotely reset to 0 by calling mqtt/retry via the api
                preference.health.mqtt = -1;
                return;
            } else {
                delay(250);
                attempts++;
            }
        }
        // If state > 0 (MQTT_CONNECTED) => config or server problem we stop retry
        else if (mqtt_client->state() > MQTT_CONNECTED) {
            #ifdef _debug  
                Serial.println("MQTT: server connected");
            #endif
            preference.health.mqtt = 1;
            return;
        }
        // We are connected
        else if (topic.gpios) {
            #ifdef _debug  
                Serial.println("MQTT: server subscribed");
            #endif
            mqtt_client->subscribe(topic.gpios);
            // see haConfig();
            publish();
        }
    }
}

void MqttHandler::disconnect() {
    if (mqtt_client->state() == MQTT_CONNECTED) {
        #ifdef _debug  
            Serial.println("MQTT: disconnected");
        #endif
      mqtt_client->disconnect();
      preference.health.mqtt = 0;
    }
}

void MqttHandler::callback(char* topic, byte* payload, unsigned int length) {
    #ifdef _debug  
        Serial.printf("MQTT: callback data on topic %s\n", topic);
    #endif
    // Copy payload into message buffer
    char message[length + 1];
    for (int i = 0; i < length; i++) {
        message[i] = (char)payload[i];
    }
    message[length] = '\0';
    if (strcmp(topic, this->topic.gpios) == 0) {
        // Handle here setting pin state
    }
}

void MqttHandler::publish(){
    #ifdef _debug  
        Serial.println(F("MQTT: publishing"));
    #endif
    if (!mqtt_client->publish_P(topic.gpios, preference.getGpiosJson().c_str(), false)) {
         #ifdef _debug  
            Serial.println("MQTT: failed to publish");
        #endif
        mqtt_client->publish(topic.debug, (char*)(F("Failed to publish gpios list change")));
    }
}