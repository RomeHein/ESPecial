#pragma once

#ifndef PreferenceHandler_h
#define PreferenceHandler_h
#include <Arduino.h>

#define PREFERENCES_NAME "esp32-api"
#define PREFERENCES_GPIOS "gpios"
#define PREFERENCES_ACTION "action"
#define PREFERENCES_CONDITION "condition"
#define PREFERENCES_MQTT "mqtt"
#define PREFERENCES_TELEGRAM "telegram"

// Yes only 20 to limit memeory usage
#define MAX_ACTIONS_NUMBER 20 // Maximum actions number that can be set in the system
#define MAX_ACTIONS_CONDITIONS_NUMBER 5 // Maximum number of conditions in a given action
#define MAX_CONDITIONS_NUMBER 20 // Maximum conditions number that can be set in the system

#define GPIO_JSON_CAPACITY JSON_OBJECT_SIZE(5) + 80 + 150
#define GPIOS_JSON_CAPACITY JSON_ARRAY_SIZE(GPIO_PIN_COUNT) + GPIO_PIN_COUNT*(GPIO_JSON_CAPACITY)
#define ACTION_JSON_CAPACITY JSON_ARRAY_SIZE(MAX_ACTIONS_CONDITIONS_NUMBER) + JSON_OBJECT_SIZE(11) + 260 + 400
#define ACTIONS_JSON_CAPACITY JSON_ARRAY_SIZE(MAX_ACTIONS_NUMBER) + MAX_ACTIONS_NUMBER*(ACTION_JSON_CAPACITY)
#define CONDITION_JSON_CAPACITY JSON_OBJECT_SIZE(5) + 64 + 50
#define CONDITIONS_JSON_CAPACITY JSON_ARRAY_SIZE(MAX_CONDITIONS_NUMBER) + MAX_CONDITIONS_NUMBER*(CONDITION_JSON_CAPACITY)

// 0 for inactive, 1 for all good, -1 for error
typedef struct
{
    int8_t telegram = 0; 
    int8_t mqtt = 0;
    int8_t api = 0;
 }  HealthCode;

typedef struct
{
    uint8_t id; // action id
    char label[50];
    int8_t type; //type:  1- send Telegram Notification, 2- display message on tft screen, 3- set Pin to value
    int8_t conditions[MAX_ACTIONS_CONDITIONS_NUMBER]; // array of conditions id to check before executing the action.
    int8_t autoRun; // Automatically play the action if all conditions are true
    char mes[150]; // message to send to telegram or screen
    uint8_t pinC; // pin to Control
    uint16_t valueC; // What value to send to pinC
    int8_t loopCount; // Number of time to execute the action before next
    uint32_t delay; // Delay in ms between each loop
    uint8_t nextActionId; // next Action id
 }  ActionFlash;

 typedef struct
 {
    uint8_t id;
    char label[50];
    int8_t type; // 1- Pin value is equal to, 2- greater than, 3- less than
    uint8_t pin;
    uint16_t value;
 } ConditionFlash;

typedef struct
{
    uint8_t pin;
    char label[50];
    int8_t mode;
    int8_t state;
    int8_t save;
 }  GpioFlash;

typedef struct
{
    int8_t active;
    char fn[100];
    char host[200];
    int16_t port;
    char user[100];
    char password[100];
    char topic[100];
 }  MqttFlash;

typedef struct
{
    char token[200];
    int8_t active;
    char currentChatId[50];
}  TelegramFlash;
 
class PreferenceHandler
{
private:
    void initGpios();
    int firstEmptyGpioSlot();
    int firstEmptyActionSlot();
    int firstEmptyConditionSlot();
    int newId(char *preference);
    String gpioToJson(GpioFlash& gpio);
    void setActionsFromJson(const char* json);
    String actionToJson(ActionFlash& action);
    String conditionToJson(ConditionFlash& condition);
public:
    void begin();
    void clear();
    void save(char* preference);
    HealthCode health;
    // Gpio
    GpioFlash gpios[GPIO_PIN_COUNT];
    bool removeGpio(int pin);
    String addGpio(int pin, const char* label, int mode, int save = 0);
    String editGpio(GpioFlash& gpio, int newPin,const char* newLabel, int newMode = 0, int save = 0);
    void setGpioState(int pin, int value);
    String getGpiosJson();
    // Action
    ActionFlash actions[MAX_ACTIONS_NUMBER];
    String getActionsJson();
    bool removeAction(int id);
    String addAction(const char* label, int type,const int* conditions,int autoRun, const char* message= "", int pinC = -1, int valueC = 0, int loopCount = 0,int delay = 0, int nextActionId = 0);
    String editAction(ActionFlash& action, const char* newLabel, int newType,const int* newConditions, int newActionRun, const char* newMessage, int newPinC, int newValueC, int newLoopCount,int newDelay, int newNextActionId);
    // Condition
    ConditionFlash conditions[MAX_CONDITIONS_NUMBER];
    String getConditionsJson();
    bool removeCondition(int id);
    String addCondition(const char* label, int type, int pin, int value = 0);
    String editCondition(ConditionFlash& condition, int newType,const char* newLabel, int newPin, int newValue = 0);
    // Mqtt
    MqttFlash mqtt;
    bool editMqtt(int newActive, const char* newFn, const char* newHost,int newPort, const char* newUser, const char* newPassword, const char* newTopic);
    // Telegram
    TelegramFlash telegram;
    bool editTelegram(const char* token,const char* newChatId,int active);
};
#endif