#pragma once

#ifndef ServerHandler_h
#define ServerHandler_h
#include <Arduino.h>
#include <WebServer.h>
#include <ArduinoJson.h>

typedef struct
{
     int8_t pin;
     char label[50];
     int8_t mode;
     int8_t state;
 }  GpioFlash;
 
class ServerHandler
{
private:
    int firstEmptyGpioSlot();
    void initGpios();
    void getGpios();
    void saveGpios();
    void setGpioState(int pin, int value);
    void handleAvailableGpios();
    void handleSetGpioState();
    void handleGetGpioState();
    void handleRoot();
    void handleGpioEdit();
    void handleGpioRemove();
    void handleGpioNew();
    void handleUpload();
    void handleUpdate();
    void install();
    void handleNotFound();
    void handleClearSettings();
public:
    WebServer server = {80};
    GpioFlash gpios[GPIO_PIN_COUNT]; 
    void begin();
};
#endif