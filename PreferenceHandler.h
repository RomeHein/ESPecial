#pragma once

#ifndef PreferenceHandler_h
#define PreferenceHandler_h
#include <Arduino.h>
#include <ArduinoJson.h>
#include "Parameters.h"

#define NO_GLOBAL_TWOWIRE
#include <Wire.h>

#define GPIO_JSON_CAPACITY JSON_OBJECT_SIZE(9) + 100 + MAX_LABEL_TEXT_SIZE*2
#define CAMERA_JSON_CAPACITY JSON_OBJECT_SIZE(29) + 100
#define GPIOS_JSON_CAPACITY JSON_ARRAY_SIZE(GPIO_PIN_COUNT) + GPIO_PIN_COUNT*(GPIO_JSON_CAPACITY)
#define I2CSLAVE_JSON_CAPACITY JSON_OBJECT_SIZE(8) + 100 + (MAX_LABEL_TEXT_SIZE + MAX_MESSAGE_TEXT_SIZE)*2
#define I2CSLAVES_JSON_CAPACITY JSON_ARRAY_SIZE(MAX_I2C_SLAVES) + MAX_I2C_SLAVES*(I2CSLAVE_JSON_CAPACITY)
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
    // index 0: action type: 1 is set gpio pin to a value, 2 is sending a message to telegram, 3 is displaying a message on serial, 4 is a delay, 5 micro delay, 6 http request , 7 nested automation
    // index 1: action value OR http method OR automation id
    // index 2: pin to control if type is 1 OR telegram message format with picture if type is 2 OR http address if type is 8
    // index 3: assignement operation type on value: 1 is =, 2 is +=, 3 is -=, 4 is *= OR http body
    char actions[MAX_AUTOMATION_ACTION_NUMBER][4][MAX_MESSAGE_TEXT_SIZE];
    int8_t autoRun; // Automatically play the automation if all conditions are true
    int32_t loopCount; // Number of time to execute the automation before next
    int32_t debounceDelay; // Time before the same automation can be run again
 }  AutomationFlash;

typedef struct
{
    uint8_t pin;
    char label[MAX_LABEL_TEXT_SIZE];
    int8_t mode; // 1 is INPUT, 2 is OUTPUT, 5 is INPUT_PULLUP, -1 is LEDCONTROL, -2 is I2C, -3 is ADC (analog read), -4 is Touch read, -5 is DAC (digital to analog), -100 is blocked (not available in the list of pins)
    uint32_t frequency;
    uint8_t resolution;
    int8_t channel;
    int8_t sclpin; // Only for I2C type
    int16_t state;
    int8_t invert; // Invert status of digital pins only
    int8_t save;
 }  GpioFlash;

 typedef struct
{
    uint8_t id;
    int8_t address; // Slave address
    uint8_t mPin; // Master pin
    char label[MAX_LABEL_TEXT_SIZE];
    uint8_t commands[MAX_I2C_COMMAND_NUMBER]; // Salve's commands
    char data[MAX_MESSAGE_TEXT_SIZE]; // Data to send just after commands
    uint8_t octetRequest = 0; // Number of octet requested when reading, if empty, only write command will be executed.
    uint8_t save;
 }  I2cSlaveFlash;

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

typedef struct
{
    char apSsid[33]; //ssid can be up to 32chars plus null term
    char apPsw[200];
    int8_t staEnable;
    char staSsid[33];
    char staPsw[200];
    char dns[200];
}  WifiFlash;

typedef struct 
{
    int8_t model; //1 : WROVER KIT, 2: ESP EYE, 3: M5STACK PSRAM, 4: M5STACK WIDE, 5: AI THINKER
    bool faceRecognitionEnable = false;
    bool isEnrolling = false;
    uint8_t framsize;
    uint8_t quality;
    char brightness;
    char contrast;
    char saturation;
    char sharpness;
    uint8_t specialEffect;
    uint8_t wbMode;
    uint8_t awb; // whitebal
    uint8_t awbGain; // 
    uint8_t aec; // exposure control
    uint8_t aec2;
    uint8_t denoise;
    char aeLevel;
    uint8_t aecValue;
    uint8_t agc; // gain control
    uint8_t agcGain;
    uint8_t gainceiling;
    uint8_t bpc;
    uint8_t wpc;
    uint8_t rawGma;
    uint8_t lenc;
    uint8_t hmirror;
    uint8_t vflip;
    uint8_t dcw;
    uint8_t colorbar;
} CameraFlash;

 
class PreferenceHandler
{
private:
    void initGpios();
    int firstEmptySlot(const char *preference);
    int newId(const char *preference);
    String gpioToJson(GpioFlash& gpio);
    String slaveToJson(I2cSlaveFlash& slave);
    void setAutomationsFromJson(const char* j);
    String automationToJson(AutomationFlash& a);
    TwoWire *i2cHandlers[GPIO_PIN_COUNT];
    bool attach(GpioFlash& gpio);
    bool detach(GpioFlash& gpio);
    int touchTempValues[GPIO_PIN_COUNT] = {};
    long lastTouchDebounceTimes[GPIO_PIN_COUNT] = {};
    uint8_t cameraPinsConfig[5][16] = {
        {4,5,18,19,36,39,34,35,21,22,25,23,26,27,-1,-1}, // CAMERA_MODEL_WROVER_KIT 
        {34,13,14,35,39,38,37,36,4,25,5,27,18,23,-1,-1}, // CAMERA_MODEL_ESP_EYE
        {32,35,34,5,39,18,36,19,27,21,22,26,25,23,-1,15}, // CAMERA_MODEL_M5STACK_PSRAM
        {32,35,34,5,39,18,36,19,27,21,25,26,22,23,-1,15}, // CAMERA_MODEL_M5STACK_WIDE
        {5,18,19,21,36,39,34,35,0,22,25,23,26,27,32,-1} // CAMERA_MODEL_AI_THINKER
    };
public:
    void begin();
    void clear();
    void save(const char* preference);
    int touchSensor(int pin);
    HealthCode health;
    // I2C
    I2cSlaveFlash i2cSlaves[MAX_I2C_SLAVES];
    String scan(GpioFlash& gpio);
    bool removeSlave(int id);
    String addSlave(int address, int mPin, const char* label, const uint8_t* commands,const char* newData, int octetRequest = 0, int save = 0);
    String editSlave(I2cSlaveFlash& slave, const char* label, const uint8_t* commands,const char* newData, int octetRequest, int save);
    String sendSlaveCommands(int id);
    String getI2cSlavesJson();
    // Gpio
    GpioFlash gpios[GPIO_PIN_COUNT];
    bool removeGpio(int pin);
    String addGpio(int pin, const char* label, int mode,int sclpin = PIN_NOT_ATTACHED, int frequency = 50, int resolution = 16, int channel = CHANNEL_NOT_ATTACHED, int save = 0, int invert = 0);
    String editGpio(int oldPin, int newPin,const char* newLabel, int newMode = 0,int newSclPin = PIN_NOT_ATTACHED, int newFrequency = 50, int newResolution = 16, int newChannel = CHANNEL_NOT_ATTACHED, int newSave = 0, int newInvert = 0);
    void setGpioState(int pin, int value = -1, bool persist = false);
    int getGpioState(int pin);
    String getGpiosJson();
    // Automation
    AutomationFlash automations[MAX_AUTOMATIONS_NUMBER];
    String getAutomationsJson();
    bool removeAutomation(int id);
    String addAutomation(const char* label, int autoRun,const int16_t conditions[MAX_AUTOMATIONS_CONDITIONS_NUMBER][4],char actions[MAX_AUTOMATION_ACTION_NUMBER][4][MAX_MESSAGE_TEXT_SIZE],int loopCount = 0, int32_t debounceDelay = 0);
    String editAutomation(AutomationFlash& automation, const char* newLabel,int newAutoRun,const int16_t newConditions[MAX_AUTOMATIONS_CONDITIONS_NUMBER][4],char newActions[MAX_AUTOMATION_ACTION_NUMBER][4][MAX_MESSAGE_TEXT_SIZE], int newLoopCount, int32_t newDebounceDelay);
    // Mqtt
    MqttFlash mqtt;
    bool editMqtt(JsonObject &json);
    // Telegram
    TelegramFlash telegram;
    bool editTelegram(const char* token,const int* newUsers,int active);
    // Wifi
    WifiFlash wifi;
    bool editWifi(const char* dns, const char* apSsid, const char* apPsw, int staEnable,const char* staSsid, const char* staPsw);
    // Camera
    CameraFlash camera;
    void createCamera(const int model);
    void removeCamera();
    bool initCamera(const int model);
    String getCameraJson();
    void saveCameraSettings();
    bool setCameraVar(const char* var, int value);
};
#endif