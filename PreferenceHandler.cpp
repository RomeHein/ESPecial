#include "PreferenceHandler.h"
#include <Preferences.h>
#include <ArduinoJson.h>

//unmark following line to enable debug mode
#define __debug

#define PREFERENCES_NAME "esp32-api"
#define PREFERENCES_GPIOS "gpios"
#define PREFERENCES_ACTION "action"
#define PREFERENCES_CONDITION "condition"
#define PREFERENCES_MQTT "mqtt"
#define PREFERENCES_TELEGRAM "telegram"

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

    // Init action preferences
    {
        setActionsFromJson(preferences.getString(PREFERENCES_ACTION, "[]").c_str());
        // Note: Can't figure it out why the following code does not work with the ActionFlash array struct.
        // The first object is saving well, but all other are not...
        // size_t schLen = preferences.getBytes(PREFERENCES_ACTION, NULL, NULL);
        // char buffer[schLen];
        // preferences.getBytes(PREFERENCES_ACTION, buffer, schLen);
        // memcpy(actions, buffer, schLen);
    }

    // Init condition preferences
    {
        size_t schLen = preferences.getBytes(PREFERENCES_CONDITION, NULL, NULL);
        char buffer[schLen];
        preferences.getBytes(PREFERENCES_CONDITION, buffer, schLen);
        memcpy(conditions, buffer, schLen);
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
    }else if (strcmp(preference, PREFERENCES_ACTION) == 0) {
        // Note: saving a string instead of bytes. 
        // The struct for ActionFlash does not seem to work properly with the putBytes. 
        // Need further investigation
        preferences.putString(PREFERENCES_ACTION, getActionsJson());
    }else if (strcmp(preference, PREFERENCES_CONDITION) == 0) {
        preferences.putBytes(PREFERENCES_CONDITION, &conditions, sizeof(conditions));
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
    if (preference == PREFERENCES_ACTION) {
        for (ActionFlash& action: actions) {
            if (action.id>=newId) {
                newId = action.id + 1;
            }
        }
    } else if (preference == PREFERENCES_CONDITION) {
        for (ConditionFlash& condition: conditions) {
            if (condition.id>=newId) {
                newId = condition.id + 1;
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
    }
    return hasChanged;
}
// Action
int PreferenceHandler::firstEmptyActionSlot() {
    const int count = sizeof(actions)/sizeof(*actions);
    for (int i=0;i<count;i++) {
        if (!actions[i].id) {
            return i;
        }
    }
    return count-1;
}

void PreferenceHandler::setActionsFromJson(const char* json) {
    DynamicJsonDocument doc(ACTIONS_JSON_CAPACITY);
    DeserializationError error = deserializeJson(doc, json);
    if (error) {
        #ifdef __debug
            Serial.print(F("Server: deserializeJson() failed: "));
            Serial.println(error.c_str());
        #endif
        return;
    }
    int i = 0;
    for (JsonObject action : doc.as<JsonArray>()) {
        ActionFlash newAction = {};
        newAction.id = action["id"].as<int>();
        newAction.type = action["type"].as<int>();
        strcpy(newAction.label, action["label"].as<char*>());
        strcpy(newAction.mes, action["message"].as<char*>());
        newAction.pinC = action["pinC"].as<int>();
        newAction.valueC = action["valueC"].as<int>();
        newAction.loopCount = action["loopCount"].as<int>();
        newAction.nextActionId = action["nextActionId"].as<int>();
        newAction.autoRun = action["autoRun"].as<int>();
        newAction.delay = action["delay"].as<int>();
        int8_t conditions[MAX_ACTIONS_CONDITIONS_NUMBER] = {};
        int j = 0;
        for(int8_t conditionId: action["conditions"].as<JsonArray>()) {
            conditions[j] = conditionId;
            j++;
        }
        memcpy(newAction.conditions, conditions, 5);
        actions[i] = newAction;
        i++;
    }
}

String PreferenceHandler::actionToJson(ActionFlash& action) {
    DynamicJsonDocument doc(ACTION_JSON_CAPACITY);
    doc["id"] = action.id;
    doc["label"] = action.label;
    doc["autoRun"] = action.autoRun;
    doc["type"] = action.type;
    doc["message"] = action.mes;
    doc["pinC"] = action.pinC;
    doc["valueC"] = action.valueC;
    doc["loopCount"] = action.loopCount;
    doc["nextActionId"] = action.nextActionId;
    doc["delay"] = action.delay;
    JsonArray conditions = doc.createNestedArray("conditions");
    for (int condition: action.conditions) {
        conditions.add(condition);
    }
    String output;
    serializeJson(doc, output);
    return output;
}

String PreferenceHandler::getActionsJson() {
    DynamicJsonDocument doc(ACTIONS_JSON_CAPACITY);
    for (ActionFlash& action : actions) {
        if (action.id) {
            doc.add(serialized(actionToJson(action)));
        }
    }
    String output;
    serializeJson(doc, output);
    return output;
}

bool PreferenceHandler::removeAction(int id) {
    const int count = sizeof(actions)/sizeof(*actions);
    for (int i=0;i<count;i++) {
        if (actions[i].id == id) {
            actions[i] = {};
            save(PREFERENCES_ACTION);
            return true;
        }
    }
    return false;
}
String PreferenceHandler::addAction(const char* label, int type,const int* conditions,int autoRun, const char* message, int pinC, int valueC, int loopCount,int delay, int nextActionId) {
    ActionFlash newAction = {};
    newAction.id = newId(PREFERENCES_ACTION);
    newAction.type = type;
    strcpy(newAction.label, label);
    memcpy(newAction.conditions, conditions, sizeof(newAction.conditions));
    newAction.autoRun = autoRun;
    strcpy(newAction.mes, message);
    newAction.pinC = pinC;
    newAction.valueC = valueC;
    newAction.loopCount = loopCount;
    newAction.nextActionId = nextActionId;
    newAction.delay = delay;
    actions[firstEmptyActionSlot()] = newAction;
    save(PREFERENCES_ACTION);
    return actionToJson(newAction);
}
String PreferenceHandler::editAction(ActionFlash& action, const char* newLabel, int newType,const int* newConditions,int newAutoRun, const char* newMessage,int newPinC, int newValueC, int newLoopCount,int newDelay, int newNextActionId) {
    bool hasChanged = false;
    if (newConditions && memcmp(action.conditions, newConditions, sizeof(action.conditions)) != 0) {
        memcpy(action.conditions, newConditions, sizeof(action.conditions));
        hasChanged = true;
    }
    if (newAutoRun && action.autoRun != newAutoRun) {
        action.autoRun = newAutoRun;
        hasChanged = true;
    }
    if (newLabel && strcmp(action.label, newLabel) != 0) {
        strcpy(action.label, newLabel);
        hasChanged = true;
    }
    if (newMessage && strcmp(action.mes, newMessage) != 0) {
        strcpy(action.mes, newMessage);
        hasChanged = true;
    }
    if (newType && action.type != newType) {
        action.type = newType;
        hasChanged = true;
    }
    if (newPinC && action.pinC != newPinC) {
        action.pinC = newPinC;
        hasChanged = true;
    }
    if (action.valueC != newValueC) {
        action.valueC = newValueC;
        hasChanged = true;
    }
    if (action.loopCount != newLoopCount) {
        action.loopCount = newLoopCount;
        hasChanged = true;
    }
    if (action.delay != newDelay) {
        action.delay = newDelay;
        hasChanged = true;
    }
    if (action.nextActionId != newNextActionId) {
        action.nextActionId = newNextActionId;
        hasChanged = true;
    }
    if (hasChanged) {
        save(PREFERENCES_ACTION);
    }
    return actionToJson(action);
}

// Condition
int PreferenceHandler::firstEmptyConditionSlot() {
    const int count = sizeof(conditions)/sizeof(*conditions);
    for (int i=0;i<count;i++) {
        if (!conditions[i].id) {
            return i;
        }
    }
    return count-1;
}

String PreferenceHandler::conditionToJson(ConditionFlash& condition) {
    StaticJsonDocument<CONDITION_JSON_CAPACITY> doc;
    doc["id"] = condition.id;
    doc["label"] = condition.label;
    doc["type"] = condition.type;
    doc["pin"] = condition.pin;
    doc["value"] = condition.value;
    String output;
    serializeJson(doc, output);
    return output;
}

String PreferenceHandler::getConditionsJson() {
    DynamicJsonDocument doc(CONDITIONS_JSON_CAPACITY);
    for (ConditionFlash& condition : conditions) {
        if (condition.id) {
            doc.add(serialized(conditionToJson(condition)));
        }
    }
    String output;
    serializeJson(doc, output);
    return output;
}

bool PreferenceHandler::removeCondition(int id) {
    const int count = sizeof(conditions)/sizeof(*conditions);
    for (int i=0;i<count;i++) {
        if (conditions[i].id == id) {
            conditions[i] = {};
            save(PREFERENCES_CONDITION);
            return true;
        }
    }
    return false;
}
String PreferenceHandler::addCondition(const char* label, int type, int pin, int value) {
    ConditionFlash newCondition = {};
    newCondition.id = newId(PREFERENCES_CONDITION);
    newCondition.type = type;
    strcpy(newCondition.label, label);
    newCondition.pin = pin;
    newCondition.value = value;
    conditions[firstEmptyConditionSlot()] = newCondition;
    save(PREFERENCES_CONDITION);
    return conditionToJson(newCondition);
}
String PreferenceHandler::editCondition(ConditionFlash& condition, int newType,const char* newLabel, int newPin, int newValue) {
    bool hasChanged = false;
    if (newLabel && strcmp(condition.label, newLabel) != 0) {
        strcpy(condition.label, newLabel);
        hasChanged = true;
    }
    if (newType && condition.type != newType) {
        condition.type = newType;
        hasChanged = true;
    }
    if (newPin && condition.pin != newPin) {
        condition.pin = newPin;
        hasChanged = true;
    }
    if (condition.value != newValue) {
        condition.value = newValue;
        hasChanged = true;
    }
    if (hasChanged) {
        save(PREFERENCES_CONDITION);
    }
    return conditionToJson(condition);
}