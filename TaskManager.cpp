#include "TaskManager.h"
#include <HTTPClient.h>
#include <HTTPUpdate.h>

void TaskManager::begin() {
    // Set all handlers.
    serverhandler = new ServerHandler(*preference, TaskManager::executeTask);
    serverhandler->begin();
    telegramhandler = new TelegramHandler(*preference, client, TaskManager::executeTask);
    mqtthandler = new MqttHandler(*preference, clientNotSecure, TaskManager::executeTask);

    h4.every(20,[](){ readPins(); })
    h4.queueFunction([](){ getFirmwareList()});
    h4.every(20,[](){ telegramhandler.handle(); })
    h4.every(30,[](){ mqtthandler.handle(); })
}

void TaskManager::executeTask(Task& task) {
    h4.queueFunction([](){
        if (task.id == 1) {
            runAutomation(task.value)});
        } else if (task.id == 2) {
            // Do not persist gpio state here, we want the main loop event (readPins function) to catch the change and trigger its own logic
            preference.setGpioState(task.pin, task.value);
        }
    }
}

void TaskManager::getFirmwareList() {
  HTTPClient http;
  const String firmwarelistPath = String(REPO_PATH) + "list.json";
  #ifdef __debug
    Serial.printf("[MAIN] Retrieving firmware list from %s\n",firmwarelistPath.c_str());
  #endif
  http.begin(client,firmwarelistPath.c_str());
  int httpResponseCode = http.GET();
  if (httpResponseCode>0) {
    #ifdef __debug
      Serial.println(F("[MAIN] firmware list retrieved"));
    #endif
    if (serverhandler->events.count()>0) {
        h4.queueFunction([](){ 
            serverhandler->events.send(http.getString().c_str(),"firmwareList",millis());
        });
    }
  }
  else {
    Serial.printf("[MAIN] Could not get firmware list: %s\n", http.errorToString(httpResponseCode).c_str());
  }
  http.end();
}

void TaskManager::readPins() {
    bool gpioStateChanged = false;
    for (GpioFlash& gpio : preference->gpios) {
      if (gpio.pin && gpio.mode != -100) {
        int newState = preference->getGpioState(gpio.pin);
        if (gpio.state != newState) {
          #ifdef __debug
            Serial.printf("[PIN] Gpio pin %i state changed. Old: %i, new: %i\n",gpio.pin, gpio.state, newState);
          #endif
          // Save to preferences if allowed
          preference->setGpioState(gpio.pin, newState, true);
          // Notifiy mqtt clients
          h4.queueFunction([](){ 
            mqtthandler->publish(gpio.pin);
          })
          // Notifiy web interface with format "pinNumber-state"
          h4.queueFunction([](){ 
            char eventMessage[10];
            snprintf(eventMessage,10,"%d-%d",gpio.pin,newState);
            serverhandler->events.send(eventMessage,"pin",millis());
            #ifdef __debug
                Serial.printf("[EVENT] Sent GPIO event for pin %i. Old: %i, new: %i\n",gpio.pin, gpio.state, newState);
            #endif
          });
          gpioStateChanged = true;
        }
      }
    }
    if (gpioStateChanged) {
      runTriggeredEventAutomations(false);
    }
}

void runTriggeredEventAutomations(bool onlyTimeConditionned) {
  int i = 0;
  for (AutomationFlash& automation: preference->automations) {
    i++;
    // Check if the automation has passed the debounceDelay set by user.
    bool isDebounced = (automation.debounceDelay && (millis() > lastDebounceTimes[i] + automation.debounceDelay)) || !automation.debounceDelay;
    bool canRun = !onlyTimeConditionned;
    if (onlyTimeConditionned) {
      // Look for any time condition, break as soon as we found one
      for (int i=0; i<MAX_AUTOMATIONS_CONDITIONS_NUMBER; i++) {
        // Check if the condition is a time event instead of a pin number
        if (automation.conditions[i][0]<0) {
          canRun = true;
          break;
        }
      }
    }
    if (automation.autoRun && isDebounced && canRun) {
      runAutomation(automation);
      lastDebounceTimes[i] = millis();
    }
  }
}


void TaskManager::runAutomation(int id) {
  int i = 0;
  for (AutomationFlash& automation: preference->automations) {
    i++;
    // Check if the automation has passed the debounceDelay set by user.
    bool isDebounced = (automation.debounceDelay && (millis() > lastDebounceTimes[i] + automation.debounceDelay)) || !automation.debounceDelay;
    if (automation.id == id && isDebounced) {
      runAutomation(automation);
      lastDebounceTimes[i] = millis();
      break;
    }
  }
}

void runAutomation(AutomationFlash& automation) {
  #ifdef __debug
    Serial.printf("[AUTOMATION] Checking automation: %s\n",automation.label);
  #endif
  // Check if all conditions are fullfilled
  bool canRun = true;
  for (int i=0; i<MAX_AUTOMATIONS_CONDITIONS_NUMBER;i++) {
    // Ignore condition if we don't have a valid math operator, and if the previous condition had a logic operator at the end.
    if (automation.conditions[i][1] && ((i>0 && automation.conditions[i-1][3])||i==0)) {
      bool criteria;
      // Condition base on pin value
      if (automation.conditions[i][0]>-1) {
        GpioFlash& gpio = preference->gpios[automation.conditions[i][0]];
        if (automation.conditions[i][1] == 1) {
          criteria = (gpio.state == automation.conditions[i][2]);
        } else if (automation.conditions[i][1] == 2) {
          criteria = (gpio.state != automation.conditions[i][2]);
        } else if (automation.conditions[i][1] == 3) {
          criteria = (gpio.state > automation.conditions[i][2]);
        } else if (automation.conditions[i][1] == 4) {
          criteria = (gpio.state < automation.conditions[i][2]);
        }
      // Condition base on hour
      } else if (automation.conditions[i][0]==-1) {
        // The time is an int, but represent the hour in 24H format. So 07:32 will be represent by 732. 23:12 by 2312.
        criteria = checkAgainstLocalHour(automation.conditions[i][2], automation.conditions[i][1]);
      // Condition base on week day
      } else if (automation.conditions[i][0]==-2) {
        // Sunday starts at 0, saturday is 6
        criteria = checkAgainstLocalWeekDay(automation.conditions[i][2], automation.conditions[i][1]);
      }
      // Concatanate the previous condition result based on th assignement type operator.
      if (i == 0) {
        canRun = criteria;
      } else if (automation.conditions[i-1][3] == 1) {
        canRun &= criteria;
      } else if (automation.conditions[i-1][3] == 2) {
        canRun |= criteria;
      } else if (automation.conditions[i-1][3] == 3) {
        canRun ^= criteria;
      }
    } else {
      break;
    }
  }
  if (canRun) {
    // Notify clients automation is running
    if (serverhandler->events.count()>0) {
      char eventMessage[10];
      snprintf(eventMessage,10,"%d-1",automation.id);
      serverhandler->events.send(eventMessage,"automation",millis());
    }
    #ifdef __debug
      Serial.printf("[AUTOMATION] Running automation: %s\n",automation.label);
    #endif
    // Run automation
    for (int repeat=0; repeat<automation.loopCount; repeat++) {
      for (int i=0;i<MAX_AUTOMATIONS_NUMBER;i++) {
        if (automation.actions[i][0] && strnlen(automation.actions[i][0],MAX_MESSAGE_TEXT_SIZE)!=0) {
          #ifdef __debug
            Serial.printf("[ACTION] Running action type: %s\n",automation.actions[i][0]);
          #endif
          const int type = atoi(automation.actions[i][0]);
          // We deal with a gpio action
          if (type == 1 && automation.actions[i][2] && strnlen(automation.actions[i][2],MAX_MESSAGE_TEXT_SIZE)!=0) {
            int pin = atoi(automation.actions[i][2]);
            String valueCommand = String(automation.actions[i][1]);
            // Check if the value passed contains a pin command. In that case, the function will replace the pin command with its value
            addPinValueToActionString(valueCommand,0);
            int16_t value = valueCommand.toInt();
            int8_t assignmentType = atoi(automation.actions[i][3]);
            int16_t newValue = value;
            GpioFlash& gpio = preference->gpios[pin];
            int16_t currentValue = preference->getGpioState(pin);
            // Value assignement depending on the type of operator choosen
            if (assignmentType == 2) {
              newValue =  currentValue + value;
            } else if (assignmentType == 3) {
              newValue = currentValue - value;
            } else if (assignmentType == 4) {
              newValue = currentValue * value;
            }
            if (gpio.mode>0) {
              digitalWrite(pin, newValue);
            } else if (gpio.mode == -1) {
              ledcWrite(gpio.channel, newValue);
            }
            // Notifiy web clients
            if (serverhandler->events.count()>0) {
              char eventMessage[10];
              snprintf(eventMessage,10,"%d-%d",pin,newValue);
              serverhandler->events.send(eventMessage,"pin",millis());
            }
          // Send telegram message action
          } else if (type == 2) {
            String value = String(automation.actions[i][1]);
            parseActionString(value);
            h4.queueFunction([](){ 
                telegramhandler->sendMessage(value.c_str(), automation.actions[i][2]);
            });
          // Send telegram picture action
          } else if (type == 3) {
            String value = String(automation.actions[i][1]);
            parseActionString(value);
            Serial.println(value.c_str());
          // Delay type action
          } else if (type == 4) {
            // TODO: find a better solution to handle delay
            vTaskDelay(atoi(automation.actions[i][1]));
          // Micro Delay type action
          } else if (type == 5) {
            delayMicroseconds(atoi(automation.actions[i][1]));
          // Http request
          } else if (type == 6) {
            HTTPClient http;
            http.begin(client, automation.actions[i][2]);
            int httpResponseCode;
            if (atoi(automation.actions[i][1]) == 1) {
              httpResponseCode = http.GET();
            } else if (atoi(automation.actions[i][1]) == 2) {
              http.addHeader("Content-Type", "application/json");
              String value = String(automation.actions[i][3]);
              parseActionString(value);
              httpResponseCode = http.POST(value);
            }
            #ifdef __debug
              if (httpResponseCode>0) {
                Serial.print("[ACTION HTTP] HTTP Response code: ");
                Serial.println(httpResponseCode);
                String payload = http.getString();
                Serial.println(payload);
              }
              else {
                Serial.printf("[ACTION HTTP] GET failed, error: %s\n", http.errorToString(httpResponseCode).c_str());
              }
            #endif
            http.end();
          // Nested automation
          } else if (type == 7) {
            int nestedAutomationId = atoi(automation.actions[i][1]);
            for (AutomationFlash& nAutomation: preference->automations) {
              if (nAutomation.id == nestedAutomationId) {
                #ifdef __debug
                  Serial.printf("[ACTION] Going to nested automation: %s\n",nAutomation.label);
                #endif
                runAutomation(nAutomation);
                break;
              }
            }
          }
        } else {
          break;
        }
      }
    }
  }
  // Notify clients automation is done
  if (serverhandler->events.count()>0) {
    char eventMessage[10];
    snprintf(eventMessage,10,"%d-0",automation.id);
    serverhandler->events.send(eventMessage,"automation",millis());
  }
}

// Iterate through a string to find special command like `${pinNumber}` or `${info}` and replace them with the appropriate value
void parseActionString(String& toParse) {
  toParse.replace("${info}", systemInfos());
  addPinValueToActionString(toParse, 0);
}

// Detect and replace the pin number with its actual value
void addPinValueToActionString(String& toParse, int fromIndex) {
  int size = toParse.length();
  int foundIndex = 0;
  for (int i=fromIndex; i < size; i++) {
    // Check if we have a command
    if (toParse[i] == '$' && i<size-1 && toParse[i+1] == '{') {
      int pinNumber = -1;
      if (i<(size-3) && toParse[i+3] == '}') {
        pinNumber = toParse.substring(i+2).toInt();
        foundIndex = i+3;
      } else if (i<(size-4) && toParse[i+4] == '}') {
        pinNumber = toParse.substring(i+2,i+4).toInt();
        foundIndex = i+4;
      }
      if (pinNumber != -1) {
        GpioFlash& gpio = preference->gpios[pinNumber];
        String subStringToReplace = String("${") + pinNumber + '}';
        toParse.replace(subStringToReplace, String(gpio.state));
      }
    }
  }
  if (foundIndex) {
    addPinValueToActionString(toParse,foundIndex);
  }
}