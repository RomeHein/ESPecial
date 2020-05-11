#pragma once

#ifndef PreferenceHandler_h
#define PreferenceHandler_h
#include <Arduino.h>

#define NO_GLOBAL_TWOWIRE
#include <Wire.h>

#define PREFERENCES_NAME "esp32-api"
#define PREFERENCES_GPIOS "gpios"
#define PREFERENCES_AUTOMATION "automation"
#define PREFERENCES_MQTT "mqtt"
#define PREFERENCES_TELEGRAM "telegram"

#define CHANNEL_NOT_ATTACHED -1
#define PIN_NOT_ATTACHED -1
#define MAX_DIGITALS_CHANNEL 16 // Maximum channel number for analog pins
// Yes only 20 to limit memeory usage
#define MAX_AUTOMATIONS_NUMBER 10 // Maximum automations number that can be set in the system
#define MAX_AUTOMATIONS_CONDITIONS_NUMBER 5 // Maximum number of conditions in a given automation
#define MAX_AUTOMATION_ACTION_NUMBER 5 // Maximum number of actions in a given automation
#define MAX_TELEGRAM_USERS_NUMBER 10 // Maximum user number that can user telegram bot

// Max text sizes
#define MAX_LABEL_TEXT_SIZE 50
#define MAX_MESSAGE_TEXT_SIZE 100 // Usually used when we want to display a text message, or send a text to telegram for instance

#define GPIO_JSON_CAPACITY JSON_OBJECT_SIZE(9) + 100 + 150
#define GPIOS_JSON_CAPACITY JSON_ARRAY_SIZE(GPIO_PIN_COUNT) + GPIO_PIN_COUNT*(GPIO_JSON_CAPACITY)
#define AUTOMATION_JSON_CAPACITY JSON_ARRAY_SIZE(MAX_AUTOMATIONS_CONDITIONS_NUMBER+MAX_AUTOMATION_ACTION_NUMBER)+ MAX_AUTOMATIONS_CONDITIONS_NUMBER*JSON_ARRAY_SIZE(4) + MAX_AUTOMATION_ACTION_NUMBER*JSON_ARRAY_SIZE(3) + MAX_AUTOMATION_ACTION_NUMBER*300 + JSON_OBJECT_SIZE(8) + 150
#define AUTOMATIONS_JSON_CAPACITY JSON_ARRAY_SIZE(MAX_AUTOMATIONS_NUMBER) + MAX_AUTOMATIONS_NUMBER*(AUTOMATION_JSON_CAPACITY)

// 0 for inactive, 1 for all good, -1 for error
typedef struct
{
    int8_t telegram = 0; 
    int8_t mqtt = 0;
    int8_t api = 0;
 }  HealthCode;

typedef struct
{
    uint8_t id; // automation id
    char label[MAX_LABEL_TEXT_SIZE];
    // Array of conditions to check before executing the action. 
    // Each condition is represented by an array of int. 
    // index 0: the gpio pin, 
    // index 1: operator type: 1 is =, 2 is !=, 3 is >, 4 is <
    // index 2: value
    // index 3: logic oprator type to associate with the next condition. If null, no other conditions will be read. 1 is AND, 2 is OR, 3 is XOR
    int16_t conditions[MAX_AUTOMATIONS_CONDITIONS_NUMBER][4]; 

    // Array of actions to execute once conditions are fullfilled
    // Each action is represented by an array of char
    // index 0: action type: 1 is set gpio pin to a value, 2 is sending a message to telegram, 3 is displaying a message on the tft screen, 4 is a delay
    // index 1: action value
    // index 2: pin to control if type is 1
    // index 3: assignement operation type on value: 1 is =, 2 is +=, 3 is -=, 4 is *=
    char actions[MAX_AUTOMATION_ACTION_NUMBER][4][MAX_MESSAGE_TEXT_SIZE];
    int8_t autoRun; // Automatically play the automation if all conditions are true
    int8_t loopCount; // Number of time to execute the automation before next
    int32_t debounceDelay; // Time before the same automation can be run again
    uint8_t nextAutomationId; // next Automation id
 }  AutomationFlash;

typedef struct
{
    uint8_t pin;
    char label[MAX_LABEL_TEXT_SIZE];
    int8_t mode; // 1 is INPUT, 2 is OUTPUL, 5 is INPUT_PULLUP, -1 is DIGITAL, -2 is I2C
    uint32_t frequency;
    uint8_t resolution;
    int8_t channel;
    int8_t sclpin; // Only for I2C type  -> need to instantiate another GpioFlash at the same type, with I2C mode too, but no sclpin, so we know it's for the clock. Tout ceux qui n'ont pas de sclpin ne sont pas instantié et ne sont pas montrés dans l'interface 
    int16_t state;
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
    int32_t chatIds[MAX_TELEGRAM_USERS_NUMBER]; // Keep in memory the chatId of each users.
    int32_t users[MAX_TELEGRAM_USERS_NUMBER];
}  TelegramFlash;
 
class PreferenceHandler
{
private:
    void initGpios();
    int firstEmptyGpioSlot();
    int firstEmptyAutomationSlot();
    int firstEmptyConditionSlot();
    int newId(char *preference);
    String gpioToJson(GpioFlash& gpio);
    void setAutomationsFromJson(const char* j);
    String automationToJson(AutomationFlash& a);
    TwoWire *i2cHandlers[GPIO_PIN_COUNT];
    bool attach(GpioFlash& gpio);
    bool detach(GpioFlash& gpio);
public:
    void begin();
    void clear();
    void save(char* preference);
    HealthCode health;
    // i2c
    String scan(GpioFlash& gpio);
    // Gpio
    GpioFlash gpios[GPIO_PIN_COUNT];
    bool removeGpio(int pin);
    String addGpio(int pin, const char* label, int mode,int sclpin = PIN_NOT_ATTACHED, int frequency = 50, int resolution = 16, int channel = CHANNEL_NOT_ATTACHED, int save = 0);
    String editGpio(int oldPin, int newPin,const char* newLabel, int newMode = 0,int newSclPin = PIN_NOT_ATTACHED, int newFrequency = 50, int newResolution = 16, int newChannel = CHANNEL_NOT_ATTACHED, int save = 0);
    void setGpioState(int pin, int value = -1, bool persist = false);
    String getGpiosJson();
    // Automation
    AutomationFlash automations[MAX_AUTOMATIONS_NUMBER];
    String getAutomationsJson();
    bool removeAutomation(int id);
    String addAutomation(const char* label, int autoRun,const int16_t conditions[MAX_AUTOMATIONS_CONDITIONS_NUMBER][4],char actions[MAX_AUTOMATION_ACTION_NUMBER][4][MAX_MESSAGE_TEXT_SIZE],int loopCount = 0, int32_t debounceDelay = 0, int nextAutomationId = 0);
    String editAutomation(AutomationFlash& automation, const char* newLabel,int newAutoRun,const int16_t newConditions[MAX_AUTOMATIONS_CONDITIONS_NUMBER][4],char newActions[MAX_AUTOMATION_ACTION_NUMBER][4][MAX_MESSAGE_TEXT_SIZE], int newLoopCount, int32_t newDebounceDelay, int newNextAutomationId);
    // Mqtt
    MqttFlash mqtt;
    bool editMqtt(int newActive, const char* newFn, const char* newHost,int newPort, const char* newUser, const char* newPassword, const char* newTopic);
    // Telegram
    TelegramFlash telegram;
    bool editTelegram(const char* token,const int* newUsers,int active);
};
#endif