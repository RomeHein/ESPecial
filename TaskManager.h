#pragma once

#ifndef TaskManager_h
#define TaskManager_h
#include <Arduino.h>
#include "PreferenceHandler.h"
#include <H4.h>

class TaskManager
{
private:  
    PreferenceHandler &preference;
public:
    ~TaskManager() {};
    H4 h4 = {115200};
    void begin();
    void getFirmwareList();
};
#endif