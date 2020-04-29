#include "PreferenceHandler.h"
#include <Preferences.h>
#include <ArduinoJson.h>

//unmark following line to enable debug mode
#define __debug

Preferences preferences;
void PreferenceHandler::begin()
{
    preferences.begin(PREFERENCES_NAME, false);

    #ifdef __debug
        Serial.println("Preferences: init");
    #endif
    // Init gpios preferences
    {
        size_t schLen = preferences.getBytes(PREFERENCES_GPIOS, NULL, NULL);
        char buffer[schLen];
        preferences.getBytes(PREFERENCES_GPIOS, buffer, schLen);
        memcpy(gpios, buffer, schLen);
    }

    // Init automation preferences
    {
        setAutomationsFromJson(preferences.getString(PREFERENCES_AUTOMATION, "[]").c_str());
        // Note: Can't figure it out why the following code does not work with the AutomationFlash array struct.
        // The first object is saving well, but all other are not...
        // size_t schLen = preferences.getBytes(PREFERENCES_AUTOMATION, NULL, NULL);
        // char buffer[schLen];
        // preferences.getBytes(PREFERENCES_AUTOMATION, buffer, schLen);
        // memcpy(automations, buffer, schLen);
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
    }else if (strcmp(preference, PREFERENCES_AUTOMATION) == 0) {
        // Note: saving a string instead of bytes. 
        // The struct for AutomationFlash does not seem to work properly with the putBytes. 
        // Need further investigation
        preferences.putString(PREFERENCES_AUTOMATION, getAutomationsJson());
    }else if (strcmp(preference, PREFERENCES_MQTT) == 0) {
        preferences.putBytes(PREFERENCES_MQTT, &mqtt, sizeof(mqtt));
    }else if (strcmp(preference, PREFERENCES_TELEGRAM) == 0) {
        preferences.putBytes(PREFERENCES_TELEGRAM, &telegram, sizeof(telegram));
    }
    preferences.end();
}

// Return the highest id of an array, + 1
int PreferenceHandler::newId(char *preference) {
    int newId = 1;
    if (preference == PREFERENCES_AUTOMATION) {
        for (AutomationFlash& automation: automations) {
            if (automation.id>=newId) {
                newId = automation.id + 1;
            }
        }
    }
    return newId;
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
    for (int i=0;i<count;i++) {
        if (!gpios[i].pin) {
            return i;
        }
    }
    return count-1;
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
String PreferenceHandler::addGpio(int pin,const char* label, int mode, int saveState) {
    GpioFlash newGpio = {};
    newGpio.pin = pin;
    strcpy(newGpio.label, label);
    newGpio.mode = mode;
    newGpio.save = saveState;
    gpios[firstEmptyGpioSlot()] = newGpio;
    pinMode(pin, mode);
    // If we don't save state, default state to 0
    newGpio.state = saveState ? digitalRead(pin) : 0;
    save(PREFERENCES_GPIOS);
    return gpioToJson(newGpio);
}
String PreferenceHandler::editGpio(GpioFlash& gpio, int newPin,const char* newLabel, int newMode, int newSave) {
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
    if (gpio.save != newSave) {
        gpio.save = newSave;
        hasChanged = true;
    }
    if (hasChanged) {
        pinMode(gpio.pin, gpio.mode);
        gpio.state = gpio.save ? digitalRead(gpio.pin) : 0;
        save(PREFERENCES_GPIOS);
        gpio.state = digitalRead(gpio.pin);
    }
    return gpioToJson(gpio);
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
            if (gpio.save) {
                save(PREFERENCES_GPIOS);
            }
            return;
        }
    }
}

String PreferenceHandler::gpioToJson(GpioFlash& gpio) {
    StaticJsonDocument<GPIO_JSON_CAPACITY> doc;
    doc["pin"] = gpio.pin;
    doc["label"] = gpio.label;
    doc["mode"] = gpio.mode;
    doc["state"] = gpio.state;
    doc["save"] = gpio.save;
    String output;
    serializeJson(doc, output);
    return output;
}

String PreferenceHandler::getGpiosJson() {
    DynamicJsonDocument doc(GPIOS_JSON_CAPACITY);
    for (GpioFlash& gpio : gpios) {
        if (gpio.pin) {
            doc.add(serialized(gpioToJson(gpio)));
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
    }
    return hasChanged;
}

// Telegram

bool PreferenceHandler::editTelegram(const char* newToken,const char* newChatId,const int* newUsers, int newActive) {
    bool hasChanged = false;
    if (newToken && strcmp(telegram.token, newToken) != 0) {
        strcpy(telegram.token, newToken);
        hasChanged = true;
    }
    if (newChatId && strcmp(telegram.currentChatId, newChatId) != 0) {
        strcpy(telegram.currentChatId, newChatId);
        hasChanged = true;
    }
    if (telegram.active != newActive) {
        telegram.active = newActive;
        hasChanged = true;
    }
    if (newUsers && memcmp(telegram.users, newUsers, sizeof(telegram.users)) != 0) {
        memcpy(telegram.users, newUsers, sizeof(telegram.users));
        hasChanged = true;
    }
    if (hasChanged) {
        save(PREFERENCES_TELEGRAM);
    }
    return hasChanged;
}
// Automation
int PreferenceHandler::firstEmptyAutomationSlot() {
    const int count = sizeof(automations)/sizeof(*automations);
    for (int i=0;i<count;i++) {
        if (!automations[i].id) {
            return i;
        }
    }
    return count-1;
}

void PreferenceHandler::setAutomationsFromJson(const char* json) {
    DynamicJsonDocument doc(AUTOMATIONS_JSON_CAPACITY);
    DeserializationError error = deserializeJson(doc, json);
    if (error) {
        #ifdef __debug
            Serial.print(F("Server: deserializeJson() failed: "));
            Serial.println(error.c_str());
        #endif
        return;
    }
    int i = 0;
    for (JsonObject a : doc.as<JsonArray>()) {
        AutomationFlash newAutomation = {};
        newAutomation.id = a["id"].as<int>();
        strcpy(newAutomation.label, a["label"].as<char*>());
        newAutomation.autoRun = a["autoRun"].as<int>();
        newAutomation.loopCount = a["loopCount"].as<int>();
        newAutomation.nextAutomationId = a["nextAutomationId"].as<int>();
        int16_t conditions[MAX_AUTOMATIONS_CONDITIONS_NUMBER][4] = {};
        int j = 0;
        for(JsonArray condition: a["conditions"].as<JsonArray>()) {
            int16_t conditionToFlash[4];
            for (int k = 0; k<4; k++) {
                conditionToFlash[k] = condition[k];
            }
            memcpy(conditions[j], conditionToFlash, sizeof(conditionToFlash));
            j++;
        }
        memcpy(newAutomation.conditions, conditions, sizeof(conditions));

        char actions[MAX_AUTOMATION_ACTION_NUMBER][3][100] = {};
        int l = 0;
        for(JsonArray action: a["actions"].as<JsonArray>()) {
            char actionToFlash[3][100];
            for (int k = 0; k<3; k++) {
                strcpy(actionToFlash[k], action[k].as<char*>());
            }
            memcpy(actions[l], actionToFlash, sizeof(actionToFlash));
            l++;
        }
        memcpy(newAutomation.actions, actions, sizeof(actions));

        automations[i] = newAutomation;
        i++;
    }
}

String PreferenceHandler::automationToJson(AutomationFlash& a) {
    DynamicJsonDocument doc(AUTOMATION_JSON_CAPACITY);
    doc["id"] = a.id;
    doc["label"] = a.label;
    doc["autoRun"] = a.autoRun;
    doc["loopCount"] = a.loopCount;
    doc["nextAutomationId"] = a.nextAutomationId;
    JsonArray conditions = doc.createNestedArray("conditions");
    for (int i = 0; i<MAX_AUTOMATIONS_CONDITIONS_NUMBER; i++) {
        if (a.conditions[i][1]) { // Check if the condition has an operator type, in that case, it's define, so add it
            JsonArray condition = conditions.createNestedArray();
            for (int16_t param: a.conditions[i]) {
                condition.add(param);
            }
        }
    }
    JsonArray actions = doc.createNestedArray("actions");
    for (int i = 0; i<MAX_AUTOMATION_ACTION_NUMBER; i++) {
        if (a.actions[i][0] && strlen(a.actions[i][0])!=0) { // Check if the action has a type, in that case, it's define, so add it
            JsonArray action = actions.createNestedArray();
            for (int j=0; j<3; j++) {
                action.add(a.actions[i][j]);
            }
        }
    }
    String output;
    serializeJson(doc, output);
    return output;
}

String PreferenceHandler::getAutomationsJson() {
    DynamicJsonDocument doc(AUTOMATIONS_JSON_CAPACITY);
    for (AutomationFlash& a : automations) {
        if (a.id) {
            doc.add(serialized(automationToJson(a)));
        }
    }
    String output;
    serializeJson(doc, output);
    return output;
}

bool PreferenceHandler::removeAutomation(int id) {
    const int count = sizeof(automations)/sizeof(*automations);
    for (int i=0;i<count;i++) {
        if (automations[i].id == id) {
            automations[i] = {};
            save(PREFERENCES_AUTOMATION);
            return true;
        }
    }
    return false;
}
String PreferenceHandler::addAutomation(const char* label,int autoRun,const int16_t conditions[MAX_AUTOMATIONS_CONDITIONS_NUMBER][4],char actions[MAX_AUTOMATION_ACTION_NUMBER][3][100], int loopCount, int nextAutomationId) {
    AutomationFlash newAutomation = {};
    newAutomation.id = newId(PREFERENCES_AUTOMATION);
    strcpy(newAutomation.label, label);
    memcpy(newAutomation.conditions, conditions, sizeof(newAutomation.conditions));
    memcpy(newAutomation.actions, actions, sizeof(newAutomation.actions));
    newAutomation.autoRun = autoRun;
    newAutomation.loopCount = loopCount;
    newAutomation.nextAutomationId = nextAutomationId;
    automations[firstEmptyAutomationSlot()] = newAutomation;
    save(PREFERENCES_AUTOMATION);
    return automationToJson(newAutomation);
}
String PreferenceHandler::editAutomation(AutomationFlash& automation, const char* newLabel, int newAutoRun,const int16_t newConditions[MAX_AUTOMATIONS_NUMBER][4],char newActions[MAX_AUTOMATION_ACTION_NUMBER][3][100], int newLoopCount, int newNextAutomationId) {
    bool hasChanged = false;
    if (newConditions && memcmp(automation.conditions, newConditions, sizeof(automation.conditions)) != 0) {
        memcpy(automation.conditions, newConditions, sizeof(automation.conditions));
        hasChanged = true;
    }
    if (newActions && memcmp(automation.actions, newActions, sizeof(automation.actions)) != 0) {
        memcpy(automation.actions, newActions, sizeof(automation.actions));
        hasChanged = true;
    }
    if (newAutoRun && automation.autoRun != newAutoRun) {
        automation.autoRun = newAutoRun;
        hasChanged = true;
    }
    if (newLabel && strcmp(automation.label, newLabel) != 0) {
        strcpy(automation.label, newLabel);
        hasChanged = true;
    }
    if (automation.loopCount != newLoopCount) {
        automation.loopCount = newLoopCount;
        hasChanged = true;
    }
    if (automation.nextAutomationId != newNextAutomationId) {
        automation.nextAutomationId = newNextAutomationId;
        hasChanged = true;
    }
    if (hasChanged) {
        save(PREFERENCES_AUTOMATION);
    }
    return automationToJson(automation);
}