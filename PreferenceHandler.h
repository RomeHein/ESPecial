#pragma once

#ifndef PreferenceHandler_h
#define PreferenceHandler_h
#include <Arduino.h>

//unmark following line to enable debug mode
#define _debug

// 0 for inactive, 1 for all good, -1 for error
typedef struct
{
    int8_t telegram = 0; 
    int8_t mqtt = 0;
    int8_t api = 0;
 }  HealthCode;

typedef struct
{
    int8_t pin;
    char label[50];
    int8_t mode;
    int8_t state;
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
}  TelegramFlash;
 
class PreferenceHandler
{
private:
    void initGpios();
    int firstEmptyGpioSlot();
public:
    void begin();
    void clear();
    void save(char* preference);
    HealthCode health;
    // Gpio
    GpioFlash gpios[GPIO_PIN_COUNT];
    bool removeGpio(int pin);
    bool addGpio(int pin,const char* label, int mode);
    bool editGpio(GpioFlash& gpio, int newPin,const char* newLabel, int newMode = 0);
    void setGpioState(int pin, int value);
    String getGpiosJson();
    // Mqtt
    MqttFlash mqtt;
    bool editMqtt(int newActive, const char* newFn, const char* newHost,int newPort, const char* newUser, const char* newPassword, const char* newTopic);
    // Telegram
    TelegramFlash telegram;
    bool editTelegram(const char* token,int active);
};
#endif