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
        Serial.println("[PREF] init");
    #endif
    // Init gpios preferences
    {
        size_t schLen = preferences.getBytes(PREFERENCES_GPIOS, NULL, NULL);
        char buffer[schLen];
        preferences.getBytes(PREFERENCES_GPIOS, buffer, schLen);
        memcpy(gpios, buffer, schLen);
    }

    // Init I2C slaces preferences
    {
        size_t schLen = preferences.getBytes(PREFERENCES_I2C_SLAVE, NULL, NULL);
        char buffer[schLen];
        preferences.getBytes(PREFERENCES_I2C_SLAVE, buffer, schLen);
        memcpy(i2cSlaves, buffer, schLen);
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

    // Init wifi preferences
    {
        size_t schLen = preferences.getBytes(PREFERENCES_WIFI, NULL, NULL);
        char buffer[schLen];
        preferences.getBytes(PREFERENCES_WIFI, buffer, schLen);
        memcpy(&wifi, buffer, schLen);
        // Wifi settings are empty, fill with default ap settings
        if (strlen(wifi.apSsid) == 0) {
            Serial.println("here");
            strcpy(wifi.apSsid, "ESP32");
            strcpy(wifi.apPsw,"p@ssword2000");
            strcpy(wifi.apDns,"especial");
        }
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
    }else if (strcmp(preference, PREFERENCES_I2C_SLAVE) == 0) {
        preferences.putBytes(PREFERENCES_I2C_SLAVE, &i2cSlaves, sizeof(i2cSlaves));
    }else if (strcmp(preference, PREFERENCES_AUTOMATION) == 0) {
        // Note: saving a string instead of bytes. 
        // The struct for AutomationFlash does not seem to work properly with the putBytes. 
        // Need further investigation
        preferences.putString(PREFERENCES_AUTOMATION, getAutomationsJson());
    }else if (strcmp(preference, PREFERENCES_MQTT) == 0) {
        preferences.putBytes(PREFERENCES_MQTT, &mqtt, sizeof(mqtt));
    }else if (strcmp(preference, PREFERENCES_TELEGRAM) == 0) {
        preferences.putBytes(PREFERENCES_TELEGRAM, &telegram, sizeof(telegram));
    }else if (strcmp(preference, PREFERENCES_WIFI) == 0) {
        preferences.putBytes(PREFERENCES_WIFI, &wifi, sizeof(wifi));
    }
    preferences.end();
}

// Returns the first empty slot of a given array
int PreferenceHandler::firstEmptySlot(char* preference) {
    int i = 0;
    if (strcmp(preference,PREFERENCES_AUTOMATION) == 0) {
        while(i<MAX_AUTOMATIONS_NUMBER && automations[i].id) {
            i++;
        }
    } else if (strcmp(preference,PREFERENCES_I2C_SLAVE)==0) {
        while(i<MAX_I2C_SLAVES && i2cSlaves[i].id) {
            i++;
        }
    }
    return i;
}

// Return the highest id of an array, + 1
int PreferenceHandler::newId(char *preference) {
    int newId = 1;
    if (strcmp(preference,PREFERENCES_AUTOMATION) == 0) {
        for (AutomationFlash& automation: automations) {
            if (automation.id>=newId) {
                newId = automation.id + 1;
            }
        }
    } else if (strcmp(preference, PREFERENCES_I2C_SLAVE) == 0) {
        for (I2cSlaveFlash& slave: i2cSlaves) {
            if (slave.id>=newId) {
                newId = slave.id + 1;
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
            attach(gpio);
            // Only write saved state if we have analog mode
            if (gpio.mode == -1) {
                ledcWrite(gpio.channel, gpio.state);
            } else if (gpio.mode > 0) {
                digitalWrite(gpio.pin, gpio.state);
            }
        } else {
            detach(gpio);
        }
    }
}

bool PreferenceHandler::attach(GpioFlash& gpio) {
    // Attach digital
    if (gpio.mode >0) {
        pinMode(gpio.pin, gpio.mode);
    // Attach led control
    } else if (gpio.mode == -1) {
        if(gpio.channel != CHANNEL_NOT_ATTACHED && gpio.channel<MAX_DIGITALS_CHANNEL) {
            int frequency = 50;
            if (gpio.frequency) {
                frequency = gpio.frequency;
            }
            int resolution = 16;
            if (gpio.resolution) {
                frequency = gpio.resolution;
            }
            #ifdef __debug
                Serial.printf("Preferences: attaching pin %i to channel %i\n",gpio.pin, gpio.channel);
            #endif
            ledcSetup(gpio.channel, frequency, resolution); // channel X, 50 Hz, 16-bit depth
            ledcAttachPin(gpio.pin, gpio.channel);
            return true;
        }
    // Attach I2C
    } else if (gpio.mode == -2 && gpio.frequency && gpio.sclpin) {
        if (i2cHandlers[gpio.pin]) {
            delete i2cHandlers[gpio.pin];
        }
        #ifdef __debug
            Serial.printf("Preferences: attaching SDA pin %i and SCL pin %i with frequency %i\n",gpio.pin, gpio.sclpin,gpio.frequency);
        #endif
        i2cHandlers[gpio.pin] = new TwoWire(gpio.pin);
        i2cHandlers[gpio.pin]->begin(gpio.pin,gpio.sclpin,gpio.frequency);
        return true;
    } else if (gpio.mode == -3) {
        adcAttachPin(gpio.pin);
    }
    
    return false;
}

bool PreferenceHandler::detach(GpioFlash& gpio) {
    if (gpio.mode > 0) {
        gpio_reset_pin(gpio_num_t(gpio.pin));
    // Detach analog
    } else if (gpio.mode == -1) {
        if (gpio.channel == CHANNEL_NOT_ATTACHED) {
            return false;
        }
        #ifdef __debug
            Serial.printf("Preferences: detaching channel %i from pin %i\n", gpio.channel,gpio.pin);
        #endif
        ledcDetachPin(gpio.pin);
    // Detach i2c. Releasing Wire instance should call the destroy from Wire and therfor free the pin
    } else if (gpio.mode == -2 && gpio.frequency && gpio.sclpin) {
        delete i2cHandlers[gpio.pin];
        return true;
    }
    
    return true;
}

String PreferenceHandler::scan(GpioFlash& gpio){
    DynamicJsonDocument doc(JSON_ARRAY_SIZE(127) + 127*20);
    if (gpio.mode == -2 && gpio.sclpin && gpio.frequency && i2cHandlers[gpio.pin]) {
        #ifdef __debug
            Serial.printf("Preferences: Scanning I2C Addresses on Channel %i",gpio.pin);
        #endif
        
        byte error, address;
        for(address = 1; address < 127; address++ ) {
            i2cHandlers[gpio.pin]->beginTransmission(address);
            error = i2cHandlers[gpio.pin]->endTransmission();
            if (error == 0) {
                char currentAddress[10];
                if (address<16) {
                    snprintf(currentAddress, 10, "0x0%x\0", address);
                } else {
                    snprintf(currentAddress, 10, "0x%x\0", address);
                }
                doc.add(currentAddress);
            } else if (error==4) {
                #ifdef __debug
                    Serial.print("Unknow error at address 0x");
                    if (address<16) {
                        Serial.print("0");
                    }
                    Serial.println(address,HEX);
                #endif
            }    
        }
    }
    String output;
    serializeJson(doc, output);
    return output;
}

String PreferenceHandler::addSlave(int address, int mPin, const char* label,const uint8_t* commands,const char* data, int octetRequest, int s) {
    I2cSlaveFlash newSlave = {};
    newSlave.id = newId(PREFERENCES_I2C_SLAVE);
    newSlave.address = address;
    newSlave.mPin = mPin;
    strcpy(newSlave.label, label);
    memcpy(newSlave.commands, commands, sizeof(newSlave.commands));
    if (data) {
        strcpy(newSlave.data, data);
    }
    newSlave.octetRequest = octetRequest;
    newSlave.save = s;
    i2cSlaves[firstEmptySlot(PREFERENCES_I2C_SLAVE)] = newSlave;
    save(PREFERENCES_I2C_SLAVE);
    return slaveToJson(newSlave);
}

String PreferenceHandler::editSlave(I2cSlaveFlash& slave, const char* newLabel, const uint8_t* newCommands,const char* newData, int newOctetRequest, int newSave) {
    bool hasChanged = false;
    if (newLabel && strcmp(slave.label, newLabel) != 0) {
        strcpy(slave.label, newLabel);
        hasChanged = true;
    }
    if (newCommands && slave.commands != newCommands) {
        memcpy(slave.commands, newCommands, sizeof(slave.commands));
        hasChanged = true;
    }
    if (newData && slave.data != newData) {
        if (newData) {
            strcpy(slave.data, newData);
        } else {
            strcpy(slave.data, "");
        }
        hasChanged = true;
    }
    if (slave.octetRequest != newOctetRequest) {
        slave.octetRequest = newOctetRequest;
        hasChanged = true;
    }
    if (slave.save != newSave) {
        slave.save = newSave;
        hasChanged = true;
    }
    if (hasChanged) {
        save(PREFERENCES_I2C_SLAVE);
    }
    return slaveToJson(slave);
}

bool PreferenceHandler::removeSlave(int id) {
    for (int i=0; i<MAX_I2C_SLAVES; i++) {
        if (i2cSlaves[i].id == id) {
            i2cSlaves[i] = {};
            save(PREFERENCES_I2C_SLAVE);
            return true;
        }
    }
    return false;
}

String PreferenceHandler::sendSlaveCommands(int id) {
    for (I2cSlaveFlash& slave: i2cSlaves) {
        if (slave.id == id) {
            // Request read command
            bool read = (int)slave.octetRequest>0;
            Wire.beginTransmission(slave.address);
            for (uint8_t command: slave.commands) {
                if (command) {
                    Serial.printf("Send command %d",command);
                    Wire.write(command);
                }
            }
            if (slave.data) {
                for (char& buffer: slave.data) {
                    if (buffer) {
                        Wire.write(buffer);
                    }
                }
            }
            Wire.endTransmission(!read);
            // Request read bytes
            if (read) {
                Wire.requestFrom((int)slave.address, (int)slave.octetRequest);
                // Check if available octets match user parameters. If not, return empty string
                if (slave.octetRequest <= Wire.available()) {
                    DynamicJsonDocument doc(JSON_ARRAY_SIZE(slave.octetRequest));
                    for (int j=0; j<slave.octetRequest; j++) {
                        doc.add(Wire.read());
                    }
                    String output;
                    serializeJson(doc, output);
                    return output;
                }
            }
        }
    }
    return String();
}

String PreferenceHandler::slaveToJson(I2cSlaveFlash& slave) {
    StaticJsonDocument<I2CSLAVE_JSON_CAPACITY> doc;
    doc["id"] = slave.id;
    doc["address"] = slave.address;
    doc["mPin"] = slave.mPin;
    doc["label"] = slave.label;
    JsonArray commands = doc.createNestedArray("commands");
    for (int i = 0; i<MAX_I2C_COMMAND_NUMBER; i++) {
        if (slave.commands[i]) {
            commands.add(slave.commands[i]);
        }
    }
    doc["octetRequest"] = slave.octetRequest;
    doc["save"] = slave.save;
    String output;
    serializeJson(doc, output);
    return output;
}

String PreferenceHandler::getI2cSlavesJson() {
    DynamicJsonDocument doc(I2CSLAVES_JSON_CAPACITY);
    for (I2cSlaveFlash& slave : i2cSlaves) {
        if (slave.id) {
            doc.add(serialized(slaveToJson(slave)));
        }
    }
    String output;
    serializeJson(doc, output);
    return output;
}

// GPIOS

bool PreferenceHandler::removeGpio(int pin) {
    if (gpios[pin].mode<0) {
        detach(gpios[pin]);
    } 
    gpios[pin] = {};
    save(PREFERENCES_GPIOS);
    return true;
}
String PreferenceHandler::addGpio(int pin,const char* label, int mode,int sclpin, int frequency, int resolution, int channel, int saveState, int invertState) {
    GpioFlash newGpio = {};
    newGpio.pin = pin;
    strcpy(newGpio.label, label);
    newGpio.mode = mode;
    newGpio.sclpin = sclpin;
    newGpio.channel = channel;
    newGpio.frequency = frequency;
    newGpio.resolution = resolution;
    newGpio.save = saveState;
    newGpio.invert = invertState;
    gpios[pin] = newGpio;
    attach(newGpio);
    // If we don't save state, default state to 0
    if (newGpio.mode > 0) {
        newGpio.state = saveState ? digitalRead(pin) : 0;
    } else if (newGpio.mode == -1) {
        newGpio.state = ledcRead(newGpio.channel);
    }
    save(PREFERENCES_GPIOS);
    return gpioToJson(newGpio);
}
String PreferenceHandler::editGpio(int oldPin, int newPin,const char* newLabel, int newMode, int newSclPin, int newFrequency, int newResolution, int newChannel, int newSave, int newInvert) {
    bool hasChanged = false;
    GpioFlash& gpio = gpios[oldPin];
    if (newMode && gpio.mode != newMode) {
        // Detach i2C/analog if we are swtiching to a digital mode etc
        if ((gpio.mode<0 && newMode>0) || (gpio.mode != -1 && newMode==-2) || (gpio.mode != -2 && newMode==-1)) {
            detach(gpio);
        }
        // Set new mode
        attach(gpio);
        // If we are in analog mode, read the current state
        if (gpio.mode == -1) {
            gpio.state = ledcRead(gpio.channel);
        } else if (gpio.mode > 0) {
            gpio.state = gpio.save ? digitalRead(gpio.pin) : 0;
        }
        gpio.mode = newMode;
        hasChanged = true;
    }
    if (newChannel && gpio.channel != newChannel) {
        // Only do the reattach process if we don't have at the same time a new SDA pin, in that case, wait the newPin condition to reattach
        if (gpio.mode==-1 && !newPin) detach(gpio);
        gpio.channel = newChannel;
        if (gpio.mode==-1 && !newPin) attach(gpio);
        hasChanged = true;
    }
    if (newSclPin && gpio.sclpin != newSclPin) {
        // Only do the reattach process if we don't have at the same time a new SDA pin, in that case, wait the newPin condition to reattach
        if (!newPin) detach(gpio);
        gpio.sclpin = newSclPin;
        if (!newPin) attach(gpio);
        hasChanged = true;
    }
    if (newResolution && gpio.resolution != newResolution) {
        gpio.resolution = newResolution;
        hasChanged = true;
    }
    if (newFrequency && gpio.frequency != newFrequency) {
        gpio.frequency = newFrequency;
        hasChanged = true;
    }
    if (newLabel && strcmp(gpio.label, newLabel) != 0) {
        strcpy(gpio.label, newLabel);
        hasChanged = true;
    }
    if (gpio.save != newSave) {
        gpio.save = newSave;
        hasChanged = true;
    }
    if (gpio.invert != newInvert) {
        gpio.invert = newInvert;
        hasChanged = true;
    }
    if (newPin && gpio.pin != newPin) {
        detach(gpio);
        memcpy(&gpios[newPin], &gpio, sizeof(gpio));
        gpios[newPin].pin = newPin;
        gpio = {};
        // Reattach now
        attach(gpios[newPin]);
        hasChanged = true;
    }
    if (hasChanged) {
        save(PREFERENCES_GPIOS);
        GpioFlash& newGpio = newPin ? gpios[newPin] : gpio;
        // In case we don't want to save state, we still want to get the right value.
        if (newGpio.mode>0) {
            newGpio.state = digitalRead(newGpio.pin);
        } else {
            newGpio.state = ledcRead(newGpio.channel);
        }
    }
    return gpioToJson(newPin ? gpios[newPin] : gpio);
}

void PreferenceHandler::setGpioState(int pin, int value, bool persist) {
    GpioFlash& gpio = gpios[pin];
    if (gpio.pin == pin && value != gpio.state) {
        // Value -1 means reverse the state
        int newValue = (value == -1 ? !gpio.state : value);
        // If gpio is in output mode
        if (gpio.mode==2) {
            digitalWrite(pin, newValue);
        } else if (gpio.mode==-1 && gpio.channel!=CHANNEL_NOT_ATTACHED) {
            gpio.state = value;
            ledcWrite(gpio.channel, newValue);
        }
        // Persist state in flash only if we have digital input in output or analog mode
        if (persist) {
            gpio.state = newValue;
            // Don't save in flash when pin set in input mode
            if ((gpio.mode == 2 || gpio.mode == -1) && gpio.save) {
                save(PREFERENCES_GPIOS);
            }
        }
        return;
    }
}

String PreferenceHandler::gpioToJson(GpioFlash& gpio) {
    StaticJsonDocument<GPIO_JSON_CAPACITY> doc;
    doc["pin"] = gpio.pin;
    doc["label"] = gpio.label;
    doc["mode"] = gpio.mode;
    doc["sclpin"] = gpio.sclpin;
    doc["frequency"] = gpio.frequency;
    doc["resolution"] = gpio.resolution;
    doc["channel"] = gpio.channel;
    doc["state"] = gpio.state;
    doc["invert"] = gpio.invert;
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

bool PreferenceHandler::editTelegram(const char* newToken,const int* newUsers, int newActive) {
    bool hasChanged = false;
    if (newToken && strcmp(telegram.token, newToken) != 0) {
        strcpy(telegram.token, newToken);
        hasChanged = true;
    }
    if (telegram.active != newActive) {
        telegram.active = newActive;
        hasChanged = true;
    }
    if (newUsers && memcmp(telegram.users, newUsers, sizeof(telegram.users)) != 0) {
        for (int i=0; i<MAX_TELEGRAM_USERS_NUMBER;i++) {
            if (telegram.chatIds[i] && !newUsers[i]) {
                telegram.chatIds[i] = 0;
            }
        }
        memcpy(telegram.users, newUsers, sizeof(telegram.users));
        hasChanged = true;
    }
    if (hasChanged) {
        save(PREFERENCES_TELEGRAM);
    }
    return hasChanged;
}

bool PreferenceHandler:: editWifi(const char* apSsid, const char* apPsw, int staEnable,const char* staSsid, const char* staPsw) {
    bool hasChanged = false;
    if (apSsid && strcmp(wifi.apSsid, apSsid) != 0) {
        strcpy(wifi.apSsid, apSsid);
        hasChanged = true;
    }
    if (apPsw && strcmp(wifi.apPsw, apPsw) != 0) {
        strcpy(wifi.apPsw, apPsw);
        hasChanged = true;
    }
    if (wifi.staEnable != staEnable) {
        wifi.staEnable = staEnable;
        hasChanged = true;
    }
    if (staSsid && strcmp(wifi.staSsid, staSsid) != 0) {
        strcpy(wifi.staSsid, staSsid);
        hasChanged = true;
    }
    if (staPsw && strcmp(wifi.staPsw, staPsw) != 0) {
        strcpy(wifi.staPsw, staPsw);
        hasChanged = true;
    }
    if (hasChanged) {
        save(PREFERENCES_WIFI);
    }
    return hasChanged;
}
// Automation

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
        newAutomation.debounceDelay = a["debounceDelay"].as<int>();
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

        char actions[MAX_AUTOMATION_ACTION_NUMBER][4][100] = {};
        int l = 0;
        for(JsonArray action: a["actions"].as<JsonArray>()) {
            char actionToFlash[4][100];
            for (int k = 0; k<4; k++) {
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
    doc["debounceDelay"] = a.debounceDelay;
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
        if (a.actions[i][0] && strnlen(a.actions[i][0],MAX_MESSAGE_TEXT_SIZE)!=0) { // Check if the action has a type, in that case, it's define, so add it
            JsonArray action = actions.createNestedArray();
            for (int j=0; j<4; j++) {
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
String PreferenceHandler::addAutomation(const char* label,int autoRun,const int16_t conditions[MAX_AUTOMATIONS_CONDITIONS_NUMBER][4],char actions[MAX_AUTOMATION_ACTION_NUMBER][4][100], int loopCount,int debounceDelay) {
    AutomationFlash newAutomation = {};
    newAutomation.id = newId(PREFERENCES_AUTOMATION);
    strcpy(newAutomation.label, label);
    memcpy(newAutomation.conditions, conditions, sizeof(newAutomation.conditions));
    memcpy(newAutomation.actions, actions, sizeof(newAutomation.actions));
    newAutomation.autoRun = autoRun;
    newAutomation.loopCount = loopCount;
    newAutomation.debounceDelay = debounceDelay;
    automations[firstEmptySlot(PREFERENCES_AUTOMATION)] = newAutomation;
    save(PREFERENCES_AUTOMATION);
    return automationToJson(newAutomation);
}
String PreferenceHandler::editAutomation(AutomationFlash& automation, const char* newLabel, int newAutoRun,const int16_t newConditions[MAX_AUTOMATIONS_NUMBER][4],char newActions[MAX_AUTOMATION_ACTION_NUMBER][4][100], int newLoopCount,int newDebounceDelay) {
    bool hasChanged = false;
    if (newConditions && memcmp(automation.conditions, newConditions, sizeof(automation.conditions)) != 0) {
        memcpy(automation.conditions, newConditions, sizeof(automation.conditions));
        hasChanged = true;
    }
    if (newActions && memcmp(automation.actions, newActions, sizeof(automation.actions)) != 0) {
        memcpy(automation.actions, newActions, sizeof(automation.actions));
        hasChanged = true;
    }
    if (automation.autoRun != newAutoRun) {
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
    if (automation.debounceDelay != newDebounceDelay) {
        automation.debounceDelay = newDebounceDelay;
        hasChanged = true;
    }
    if (hasChanged) {
        save(PREFERENCES_AUTOMATION);
    }
    return automationToJson(automation);
}