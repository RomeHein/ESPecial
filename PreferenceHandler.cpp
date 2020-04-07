#include "PreferenceHandler.h"
#include <Preferences.h>
#include <ArduinoJson.h>

//unmark following line to enable debug mode
#define __debug

#define PREFERENCES_NAME "esp32-api"
#define PREFERENCES_GPIOS "gpios"
#define PREFERENCES_MQTT "mqtt"
#define PREFERENCES_TELEGRAM "telegram"
Preferences preferences;

void PreferenceHandler::begin()
{
    preferences.begin(PREFERENCES_NAME, false);

    #ifdef __debug
        Serial.println("Preferences: init");
    #endif
    if (preferences.getBool("gpios_are_init")) {
        size_t schLen = preferences.getBytes(PREFERENCES_GPIOS, NULL, NULL);
        char buffer[schLen];
        preferences.getBytes(PREFERENCES_GPIOS, buffer, schLen);
        memcpy(gpios, buffer, schLen);
    } else {
        #ifdef __debug
            Serial.println("Preferences: gpios not init, filling with default");
        #endif
        GpioFlash tmpGpios[]  = {
            {13, "Exemple pin 13", OUTPUT, 0},
            {17, "Exemple pin 17", INPUT, 0}
        };
        for (int i = 0; i < 2; i++) {
            gpios[i] = tmpGpios[i];
        }
        preferences.putBytes(PREFERENCES_GPIOS, &gpios, sizeof(gpios));
        preferences.putBool("gpios_are_init", true);  
    }

    // Init telegram preferences
    {
        size_t schLen = preferences.getBytes(PREFERENCES_TELEGRAM, NULL, NULL);
        char buffer[schLen];
        preferences.getBytes(PREFERENCES_TELEGRAM, buffer, schLen);
        memcpy(&telegram, buffer, schLen);
    }

    // Init mqtt preferences
    {
        size_t schLen = preferences.getBytes(PREFERENCES_MQTT, NULL, NULL);
        char buffer[schLen];
        preferences.getBytes(PREFERENCES_MQTT, buffer, schLen);
        memcpy(&mqtt, buffer, schLen);
    }
    preferences.end();

    initGpios();
}

void PreferenceHandler::clear() {
    #ifdef __debug  
        Serial.println("Preferences: clear all");
    #endif
    preferences.begin(PREFERENCES_NAME, false);
    preferences.clear();
    preferences.end();
}

void PreferenceHandler::save(char* preference) {
    preferences.begin(PREFERENCES_NAME, false);
    #ifdef __debug  
        Serial.printf("Preferences: saving in %s \n", preference);
    #endif
    if (strcmp(preference, PREFERENCES_GPIOS) == 0) {
        preferences.putBytes(PREFERENCES_GPIOS, &gpios, sizeof(gpios));
    }else if (strcmp(preference, PREFERENCES_MQTT) == 0) {
        preferences.putBytes(PREFERENCES_MQTT, &mqtt, sizeof(mqtt));
    }else if (strcmp(preference, PREFERENCES_TELEGRAM) == 0) {
        preferences.putBytes(PREFERENCES_TELEGRAM, &telegram, sizeof(telegram));
    }
    preferences.end();
}

// Gpio

void  PreferenceHandler::initGpios() 
{
    for (GpioFlash& gpio : gpios) {
        if (gpio.pin) {
            #ifdef __debug  
                Serial.printf("Preferences: init pin %i on mode %i\n", gpio.pin, gpio.mode);
            #endif
            pinMode(gpio.pin, gpio.mode);
            digitalWrite(gpio.pin, gpio.state);
        }
    }
}

int PreferenceHandler::firstEmptyGpioSlot() {
    const int count = sizeof(gpios)/sizeof(*gpios);
    for (int i=1;i<count;i++) {
        if (!gpios[i].pin) {
            return i;
        }
    }
    return count;
}

bool PreferenceHandler::removeGpio(int pin) {
    const int count = sizeof(gpios)/sizeof(*gpios);
    for (int i=0;i<count;i++) {
        if (gpios[i].pin == pin) {
            gpios[i] = {};
            save(PREFERENCES_GPIOS);
            return true;
        }
    }
    return false;
}
bool PreferenceHandler::addGpio(int pin,const char* label, int mode) {
    GpioFlash newGpio = {};
    newGpio.pin = pin;
    strcpy(newGpio.label, label);
    newGpio.mode = mode;
    gpios[firstEmptyGpioSlot()] = newGpio;
    pinMode(pin, mode);
    newGpio.state = digitalRead(pin);
    save(PREFERENCES_GPIOS);
    return false;
}
bool PreferenceHandler::editGpio(GpioFlash& gpio, int newPin,const char* newLabel, int newMode) {
    bool hasChanged = false;
    if (newPin && gpio.pin != newPin) {
        gpio.pin = newPin;
        hasChanged = true;
    }
    if (newLabel && strcmp(gpio.label, newLabel) != 0) {
        strcpy(gpio.label, newLabel);
        hasChanged = true;
    }
    if (newMode && gpio.mode != newMode) {
        gpio.mode = newMode;
        hasChanged = true;
    }
    if (hasChanged) {
        pinMode(gpio.pin, gpio.mode);
        gpio.state = digitalRead(gpio.pin);
        save(PREFERENCES_GPIOS);
        return true;
    }
    return false;
}

void PreferenceHandler::setGpioState(int pin, int value) {
    for (GpioFlash& gpio : gpios)
    {
        if (gpio.pin == pin && value != gpio.state)
        {
            if (value == -1) {
                gpio.state = !gpio.state;
            } else {
                gpio.state = value;
            }
            digitalWrite(pin, value);
            save(PREFERENCES_GPIOS);
            return;
        }
    }
}

String PreferenceHandler::getGpiosJson() {
    StaticJsonDocument<(2*sizeof(gpios))> doc;
    for (GpioFlash& gpio : gpios) {
        if (gpio.pin) {
            JsonObject object = doc.createNestedObject();
            object["pin"] = gpio.pin;
            object["label"] = gpio.label;
            object["mode"] = gpio.mode;
            object["state"] = gpio.state;
        }
    }
    String output;
    serializeJson(doc, output);
    return output;
}

// mqtt

bool PreferenceHandler::editMqtt(int newActive, const char* newFn, const char* newHost,int newPort, const char* newUser, const char* newPassword, const char* newTopic) {
    bool hasChanged = false;
    if (newFn && strcmp(mqtt.fn, newFn) != 0) {
        strcpy(mqtt.fn, newFn);
        hasChanged = true;
    }
    if (newHost && strcmp(mqtt.host, newHost) != 0) {
        strcpy(mqtt.host, newHost);
        hasChanged = true;
    }
    if (newPort && mqtt.port != newPort) {
        mqtt.port = newPort;
        hasChanged = true;
    }
    if (newUser && strcmp(mqtt.user, newUser) != 0) {
        strcpy(mqtt.user, newUser);
        hasChanged = true;
    }
    if (newPassword && strcmp(mqtt.password, newPassword) != 0) {
        strcpy(mqtt.password, newPassword);
        hasChanged = true;
    }
    if (newTopic && strcmp(mqtt.topic, newTopic) != 0) {
        strcpy(mqtt.topic, newTopic);
        hasChanged = true;
    }
    if (mqtt.active != newActive) {
        mqtt.active = newActive;
        hasChanged = true;
    }
    if (hasChanged) {
        save(PREFERENCES_MQTT);
        return true;
    }
    return false;
}

// Telegram

bool PreferenceHandler::editTelegram(const char* newToken,int newActive) {
    bool hasChanged = false;
    if (newToken && strcmp(telegram.token, newToken) != 0) {
        strcpy(telegram.token, newToken);
        hasChanged = true;
    }
    if (telegram.active != newActive) {
        telegram.active = newActive;
        hasChanged = true;
    }
    if (hasChanged) {
        save(PREFERENCES_TELEGRAM);
        return true;
    }
    return false;
}