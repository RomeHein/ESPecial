#pragma once

#ifndef PreferenceHandler_h
#define PreferenceHandler_h
#include <Arduino.h>

typedef struct
{
     int8_t pin;
     char label[50];
     int8_t mode;
     int8_t state;
 }  GpioFlash;

typedef struct
{
     char host[200];
     int8_t port;
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
    // Gpio
    GpioFlash gpios[GPIO_PIN_COUNT];
    bool removeGpio(int pin);
    bool addGpio(int pin,const char* label, int mode);
    bool editGpio(GpioFlash& gpio, int newPin,const char* newLabel, int newMode = 0);
    void setGpioState(int pin, int value);
    // Mqtt
    MqttFlash mqtt;
    bool editMqtt(const char* newHost,int newPort, const char* newUser, const char* newPassword, const char* newTopic);
    // Telegram
    TelegramFlash telegram;
    bool editTelegram(const char* token,int active);
};
#endif