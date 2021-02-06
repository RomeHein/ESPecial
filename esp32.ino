#include <WiFi.h>
#include <ESPmDNS.h>
#include "time.h"
#include <Arduino.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include "PreferenceHandler.h"
#include "ServerHandler.h"
#include "TelegramHandler.h"
#include "MqttHandler.h"
#include <H4.h>

PreferenceHandler *preferencehandler;
WiFiClientSecure client;
WiFiClient clientNotSecure;
ServerHandler *serverhandler;
TelegramHandler *telegramhandler;
MqttHandler *mqtthandler;
H4 h4(115200);

// Keep tracks of last time for each automation run 
long lastDebounceTimes[MAX_AUTOMATIONS_NUMBER] = {};

void h4setup() {
  preferencehandler = new PreferenceHandler();
  preferencehandler->begin();

  // Set all handlers.
  serverhandler = new ServerHandler(*preferencehandler, executeTask);
  telegramhandler = new TelegramHandler(*preferencehandler, client, executeTask);
  mqtthandler = new MqttHandler(*preferencehandler, clientNotSecure, executeTask);

  h4.queueFunction([]() {serverhandler->begin();});
  h4.every(DEBOUNCE_INPUT_DELAY,[](){ readPins(); });
  h4.every(DEBOUNCE_TELEGRAM_DELAY,[](){ telegramhandler->handle(); });
  h4.every(DEBOUNCE_TIME_DELAY,[](){ 
      // Run only time scheduled automations
      runTriggeredEventAutomations(true); 
  });
  h4.every(30000,[](){ 
      #ifdef __debug
          Serial.println(systemInfos());
      #endif
  });

  if (preferencehandler->wifi.staEnable && preferencehandler->wifi.staSsid) {
    #ifdef __debug
      Serial.printf("[WIFI] Station mode detected. SSID: %s PSW: %s\n",preferencehandler->wifi.staSsid, preferencehandler->wifi.staPsw);
    #endif
    WiFi.mode(WIFI_STA);
    WiFi.begin(preferencehandler->wifi.staSsid, preferencehandler->wifi.staPsw);
    int connectionAttempt = 0;
    while (WiFi.status() != WL_CONNECTED && connectionAttempt<20) {
        delay(500);
        #ifdef __debug
          Serial.println("[WIFI] .");
        #endif
        connectionAttempt++;
    }
  }

  if (WiFi.status() != WL_CONNECTED) {
    WiFi.mode(WIFI_AP);
    #ifdef __debug
      Serial.println("[WIFI] Failed to connect in station mode. Starting access point");
    #endif
    WiFi.softAP(preferencehandler->wifi.apSsid, preferencehandler->wifi.apPsw);
    Serial.print("[WIFI] AP IP address: ");
    Serial.println(WiFi.softAPIP());
  } else {
    #ifdef __debug
      Serial.print("[WIFI] connected. ");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
    #endif
    //init and get the time
    configTime(gmtOffset_sec, daylightOffset_sec, NTP_SERVER);
    // Get local time
    struct tm timeinfo;
    if(!getLocalTime(&timeinfo)){
      Serial.println(F("[SETUP] Failed to obtain time"));
    }
  }

  // Configure DNS
  if (MDNS.begin(preferencehandler->wifi.dns)) {
    #ifdef __debug
      Serial.printf("[SETUP] Web interface available on: %s or ",preferencehandler->wifi.dns);
    #endif
  }
}

void h4UserLoop() {
  // The following handlers can only work in STA mode
  if (WiFi.status() ==  WL_CONNECTED) {
    mqtthandler->handle();
  } else if (WiFi.getMode() != WIFI_AP) {
    // wifi down, reconnect here
    WiFi.begin();
    static long prev=0;
    int count = 0;
    if (millis() - prev > 500 && count < 200 ) 
    {
      ++count;
      #ifdef __debug
        Serial.printf("[MAIN_LOOP] Wifi deconnected: attempt %i/200\n", count);
      #endif
      if (count == 200) {
        Serial.println(F("[MAIN_LOOP] Failed to reconnect, restarting now."));
        ESP.restart();
      }
    }
  }
}

void onReboot(){
  // Notify clients esp has restarted
  if (serverhandler->events.count()>0) {
    serverhandler->events.send("rebooted","shouldRefresh",millis());
  }
}

void executeTask(Task& task) {
  Serial.printf("jumped to task handler type: %i\n",task.type);
  h4.queueFunction([task](){
      Serial.println("Executing task\n");
      if (task.type == 1) {
          runAutomation(task.value);
      } else if (task.type == 2) {
          // Do not persist gpio state here, we want the main loop event (readPins function) to catch the change and trigger its own logic
          preferencehandler->setGpioState(task.pin, task.value);
      } else if (task.type == 3) {
          if (strcmp(task.label,"reloadlist") == 0) {
              getFirmwareList();
          } else if (strcmp(task.label,"update") == 0 && strcmp(task.version,"")>0) {
              execOTA(task.version);
          }
      } else if (task.type == 4) {
          if (strcmp(task.label,"restart") == 0) {
              ESP.restart();
          }
      }
  });
}

void getFirmwareList() {
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
        h4.queueFunction([&http](){ 
            serverhandler->events.send(http.getString().c_str(),"firmwareList",millis());
        });
    }
  }
  else {
    Serial.printf("[MAIN] Could not get firmware list: %s\n", http.errorToString(httpResponseCode).c_str());
  }
  http.end();
}

void execOTA(const char* version) {
  const String bin = version+String("/espinstall.ino.bin");
  const String spiffs = version+String("/spiffs.bin");
  const String binPath = String(REPO_PATH)+bin;
  const String spiffsPath = String(REPO_PATH)+spiffs;
  #ifdef __debug
    Serial.printf("[OTA] start SPIFFS download from: %s\n", spiffsPath.c_str());
  #endif
  t_httpUpdate_return ret = httpUpdate.updateSpiffs(client, spiffsPath);
  switch (ret) {
    case HTTP_UPDATE_FAILED:
      Serial.printf("[OTA] Error (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
      if (serverhandler->events.count()>0) {
          h4.queueFunction([](){ 
            serverhandler->events.send(httpUpdate.getLastErrorString().c_str(),"firmwareUpdateError",millis());
          });
      }
      return;

    case HTTP_UPDATE_NO_UPDATES:
      Serial.println(F("[OTA] no updates"));
      if (serverhandler->events.count()>0) {
          h4.queueFunction([](){ 
            serverhandler->events.send("No updates","firmwareUpdateError",millis());
          });
      }
      return;

    case HTTP_UPDATE_OK:
      #ifdef __debug
        Serial.println(F("[OTA] SPIFFS updated"));
      #endif
      Serial.println(F("HTTP_UPDATE_OK"));
      break;
  }
  #ifdef __debug
    Serial.printf("[OTA] start BIN download from: %s\n", binPath.c_str());
  #endif
  ret = httpUpdate.update(client, binPath);
  switch (ret) {
    case HTTP_UPDATE_FAILED:
      Serial.printf("[OTA] Error (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
      if (serverhandler->events.count()>0) {
          h4.queueFunction([](){ 
            serverhandler->events.send(httpUpdate.getLastErrorString().c_str(),"firmwareUpdateError",millis());
          });
      }
      break;

    case HTTP_UPDATE_NO_UPDATES:
      Serial.println(F("[OTA] no updates"));
      if (serverhandler->events.count()>0) {
          h4.queueFunction([](){ 
            serverhandler->events.send("No updates","firmwareUpdateError",millis());
          });
      }
      break;

    case HTTP_UPDATE_OK:
      #ifdef __debug
        Serial.println(F("[OTA] BIN updated"));
      #endif
      h4.queueFunction([](){ 
        ESP.restart();
        });
      break;
  }
}

void readPins() {
    bool gpioStateChanged = false;
    for (GpioFlash& gpio : preferencehandler->gpios) {
      if (gpio.pin && gpio.mode != -100) {
        int newState = preferencehandler->getGpioState(gpio.pin);
        if (gpio.state != newState) {
          #ifdef __debug
            Serial.printf("[PIN] Gpio pin %i state changed. Old: %i, new: %i\n",gpio.pin, gpio.state, newState);
          #endif
          // Save to preferences if allowed
          preferencehandler->setGpioState(gpio.pin, newState, true);
          // Notifiy mqtt clients
          h4.queueFunction([&gpio](){ 
            mqtthandler->publish(gpio.pin);
          });
          // Notifiy web interface with format "pinNumber-state"
          h4.queueFunction([&gpio,newState](){ 
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
  for (AutomationFlash& automation: preferencehandler->automations) {
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


void runAutomation(int id) {
  int i = 0;
  for (AutomationFlash& automation: preferencehandler->automations) {
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
        GpioFlash& gpio = preferencehandler->gpios[automation.conditions[i][0]];
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
            GpioFlash& gpio = preferencehandler->gpios[pin];
            int16_t currentValue = preferencehandler->getGpioState(pin);
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
            h4.queueFunction([&automation,i](){ 
                String value = String(automation.actions[i][1]);
                parseActionString(value);
                telegramhandler->sendMessage(value.c_str(), automation.actions[i][2]);
            });
          // Serial print
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
              h4.queueFunction([&automation,i](){ 
                sendRequest(atoi(automation.actions[i][1]),automation.actions[i][2], automation.actions[i][3]);
              });
          // Nested automation
          } else if (type == 7) {
            int nestedAutomationId = atoi(automation.actions[i][1]);
            for (AutomationFlash& nAutomation: preferencehandler->automations) {
              if (nAutomation.id == nestedAutomationId) {
                #ifdef __debug
                  Serial.printf("[ACTION] Going to nested automation: %s\n",nAutomation.label);
                #endif
                h4.queueFunction([&nAutomation](){ 
                    runAutomation(nAutomation);
                });
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

void sendRequest(int type, char* host, char* content) {
  HTTPClient http;
  http.begin(client, host);
  int httpResponseCode;
  // get type
  if (type == 1) {
      httpResponseCode = http.GET();
  // post type
  } else if (type == 2) {
      http.addHeader("Content-Type", "application/json");
      String value = String(content);
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
        GpioFlash& gpio = preferencehandler->gpios[pinNumber];
        String subStringToReplace = String("${") + pinNumber + '}';
        toParse.replace(subStringToReplace, String(gpio.state));
      }
    }
  }
  if (foundIndex) {
    addPinValueToActionString(toParse,foundIndex);
  }
}

String systemInfos() {
  char infos[256];
  snprintf(infos,256,"\nFree memory: %i\nTelegram bot: %i\nMqtt server: %i",ESP.getFreeHeap(),preferencehandler->health.telegram,preferencehandler->health.mqtt);
  return String(infos);
}

bool checkAgainstLocalHour(int time, int signType) {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    #ifdef __debug
      Serial.println(F("[MAIN] Failed to obtain time"));
    #endif
    return false;
  }
  int hour = time/100;
  int minutes = time % 100;
  #ifdef __debug
    Serial.printf("[MAIN] Checking hour %i:%i against local time:",hour,minutes);
    Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
  #endif
  if (signType == 1) {
    return timeinfo.tm_hour == hour && minutes == timeinfo.tm_min;
  } else if (signType == 2) {
    return timeinfo.tm_hour != hour && minutes != timeinfo.tm_min;
  } else if (signType == 3) {
    return timeinfo.tm_hour > hour || (timeinfo.tm_hour == hour && timeinfo.tm_min > minutes);
  } else if (signType == 4) {
    return timeinfo.tm_hour < hour || (timeinfo.tm_hour == hour && timeinfo.tm_min < minutes);
  }
}

bool checkAgainstLocalWeekDay(int weekday, int signType) {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println(F("Failed to obtain time"));
    return false;
  }
  if (signType == 1) {
    return timeinfo.tm_wday == weekday;
  } else if (signType == 2) {
    return timeinfo.tm_wday != weekday;
  } else if (signType == 3) {
    return timeinfo.tm_wday > weekday;
  } else if (signType == 4) {
    return timeinfo.tm_wday < weekday;
  }
}
