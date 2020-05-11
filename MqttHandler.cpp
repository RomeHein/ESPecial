#include "MqttHandler.h"
#include "PreferenceHandler.h"

//unmark following line to enable debug mode
#define __debug

String getId() {
    uint64_t macAddress = ESP.getEfuseMac();
    uint64_t macAddressTrunc = macAddress << 40;
    uint32_t chipID = macAddressTrunc >> 40;
    return String(chipID, HEX);
}

void MqttHandler::begin()
{
    if (!isInit && preference.mqtt.host && preference.mqtt.port && preference.mqtt.topic && preference.mqtt.fn) {
        #ifdef __debug  
            Serial.println("MQTT: init server");
	    #endif
        mqtt_client = new PubSubClient(client);
        mqtt_client->setServer(preference.mqtt.host, preference.mqtt.port);
        mqtt_client->setCallback([this](char* topic, byte* payload, unsigned int length) { callback(topic, payload, length);});

        snprintf(topic.config, 512, "%s/%s/config\0", preference.mqtt.topic, preference.mqtt.fn);
        snprintf(topic.gpio, 512, "%s/%s/gpio\0", preference.mqtt.topic, preference.mqtt.fn);
        snprintf(topic.automation, 512, "%s/%s/automation\0", preference.mqtt.topic, preference.mqtt.fn);
        snprintf(topic.debug, 512, "%s/%s/debug\0", preference.mqtt.topic, preference.mqtt.fn);
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
        #ifdef __debug  
            Serial.printf("MQTT: Connection attempt(%i)\n", attempts);
        #endif
        // Attempt to connect
        mqtt_client->connect(getId().c_str(), preference.mqtt.user, preference.mqtt.password);
        // If state < 0 (MQTT_CONNECTED) => network problem we retry RETRY_ATTEMPT times and then waiting for MQTT_RETRY_INTERVAL_MS and retry reapeatly
        if (mqtt_client->state() < MQTT_CONNECTED) {
            #ifdef __debug  
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
            #ifdef __debug  
                Serial.println("MQTT: server connected");
            #endif
            preference.health.mqtt = 1;
            return;
        }
        // We are connected
        else if (topic.gpio) {
            #ifdef __debug  
                Serial.println("MQTT: server subscribed");
            #endif
            char gpiosTopic[strlen(topic.gpio)+3];
            sprintf(gpiosTopic, "%s/+\0",topic.gpio);
            mqtt_client->subscribe(gpiosTopic);
            char automationsTopic[strlen(topic.automation)+3];
            sprintf(automationsTopic, "%s/+\0",topic.automation);
            mqtt_client->subscribe(automationsTopic);
            mqtt_client->subscribe(topic.config);
            mqtt_client->subscribe(topic.debug);
            publishConfig();
        }
    }
}

void MqttHandler::disconnect() {
    if (mqtt_client->state() == MQTT_CONNECTED) {
        #ifdef __debug  
            Serial.println("MQTT: disconnected");
        #endif
      mqtt_client->disconnect();
      preference.health.mqtt = 0;
    }
}

void MqttHandler::callback(char* topic, byte* payload, unsigned int length) {
    #ifdef __debug  
        Serial.printf("MQTT: callback data on topic %s\n", topic);
    #endif
    // Copy payload into message buffer
    char message[length + 1];
    for (int i = 0; i < length; i++) {
        message[i] = (char)payload[i];
    }
    message[length] = '\0';
    // Check if topic is contained in the topic payload
    if (strstr(topic,this->topic.gpio)) {
        // Logic to get what pin was published
        int len = (strlen(topic) - strlen(this->topic.gpio))+1;
        char pin_c[len];
        strncpy(pin_c, topic + strlen(this->topic.gpio)+1, len-1);
        pin_c[len] = '\0';
        int pin = atoi(pin_c);
        int state = atoi(message);
        #ifdef __debug
            Serial.printf("MQTT: message %s for pin %i\n", message, pin);
        #endif
        if (pin && digitalRead(pin) != state) {
            // We set the persist flag to false, to allow the mainloop to pick up new changes and react accordingly
            preference.setGpioState(pin, state);
        } else {
            #ifdef __debug
                Serial.print("MQTT: state unchanged. Message dissmissed\n");
            #endif
        }
    } else if (strstr(topic,this->topic.automation)) {
        int len = (strlen(topic) - strlen(this->topic.automation))+1;
        char id_c[len];
        strncpy(id_c, topic + strlen(this->topic.automation)+1, len-1);
        id_c[len] = '\0';
        int id = atoi(id_c);
        int state = atoi(message);
        #ifdef __debug
            Serial.printf("MQTT: message %s for automation id %i\n", message, id);
        #endif
        if (state) {
            // queue automation id, to be picked up by esp32.ino script
            for (int i=0; i<MAX_AUTOMATIONS_NUMBER; i++) {
                if (automationsQueued[i] == 0) {
                    automationsQueued[i] = id;
                    break;
                }
            }
        }
    }
}

void MqttHandler::publish(int pin){
    if (isInit && mqtt_client->state() == MQTT_CONNECTED) {
        char pinTopic[strlen(topic.gpio)+3];
        snprintf(pinTopic,strlen(pinTopic), "%s/%i\0",topic.gpio,pin);
        char state[2];
        snprintf(state,sizeof(state), "%i\0",digitalRead(pin));
        #ifdef __debug  
            Serial.printf("MQTT: publishing pin %i state %s on topic: %s\n", pin, state, pinTopic);
        #endif
        if (!mqtt_client->publish(pinTopic, state, true)) {
            #ifdef __debug
                Serial.println("MQTT: failed to publish");
            #endif
            mqtt_client->publish(topic.debug, (char*)(F("Failed to publish gpios list change")));
        }
    }
}

void MqttHandler::publishConfig(){
    #ifdef __debug  
        Serial.println("MQTT: publishing configuration");
    #endif
    if (mqtt_client->publish_P(topic.config, preference.getGpiosJson().c_str(), false)) {
         #ifdef __debug  
            Serial.println("MQTT: failed to publish");
        #endif
        mqtt_client->publish(topic.debug, (char*)(F("Failed to publish gpios list change")));
    }
}