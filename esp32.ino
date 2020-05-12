#include <WiFiManager.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "time.h"

#include "ServerHandler.h"
#include "PreferenceHandler.h"
#include "TelegramHandler.h"
#include "MqttHandler.h"

//unmark following line to enable debug mode
#define __debug

#define MAX_QUEUED_AUTOMATIONS 10

ServerHandler *serverhandler;
PreferenceHandler *preferencehandler;
TelegramHandler *telegramhandler;
MqttHandler *mqtthandler;
WiFiClientSecure client;
WiFiClient clientNotSecure;

// ESP32 access point mode
const char *APName = "ESP32";
const char *APPassword = "p@ssword2000";

// Delay between each tft display refresh 
int delayBetweenScreenUpdate = 3000;
long screenRefreshLastTime;

// Debounce inputs delay
long lastDebouncedInputTime = 0;
int debounceInputDelay = 50;
// Keep tracks of last time for each automation run 
long lastDebounceTimes[MAX_AUTOMATIONS_NUMBER] = {};
// To trigger event based on time, we check all automatisations everyminutes
int debounceTimeDelay = 60000;
int lastCheckedTime = 0;

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3600;
const int   daylightOffset_sec = 3600;

String systemInfos() {
  String infos;
  infos = String("\nFree memory:") + ESP.getFreeHeap();
  infos += "\nTelegram bot:" + preferencehandler->health.telegram;
  infos += "\nMqtt server:" + preferencehandler->health.mqtt;
  return infos;
}

bool checkAgainstLocalHour(int time, int signType) {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return false;
  }
  int hour = time/100;
  int minutes = time % 100;
  Serial.printf("Checking hour %i:%i against local time:",hour,minutes);
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
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
    Serial.println("Failed to obtain time");
    return false;
  }
  Serial.printf("Checking weekday %i against local weekday: %i\n",weekday,timeinfo.tm_wday);
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

void readPins() {
  if (millis() > debounceInputDelay + lastDebouncedInputTime) {
    bool gpioStateChanged = false;
    for (GpioFlash& gpio : preferencehandler->gpios) {
      if (gpio.pin) {
        int newState;
        if (gpio.mode>0) {
          newState = digitalRead(gpio.pin);
        } else if (gpio.mode == -1) {
          newState = ledcRead(gpio.channel);
        }
        if (gpio.state != newState) {
          #ifdef __debug
            Serial.printf("Gpio pin %i state changed. Old: %i, new: %i\n",gpio.pin, gpio.state, newState);
          #endif
          preferencehandler->setGpioState(gpio.pin, newState, true);
          mqtthandler->publish(gpio.pin);
          gpioStateChanged = true;
        }
      }
    }
    if (gpioStateChanged) {
      runAllRunnableAutomations();
    }    
    lastDebouncedInputTime = millis();
  }
}

void runAllRunnableAutomations() {
  int i = 0;
  for (AutomationFlash& automation: preferencehandler->automations) {
    i++;
    // Check if the automation has passed the debounceDelay set by user.
    bool isDebounced = (automation.debounceDelay && (millis() > lastDebounceTimes[i] + automation.debounceDelay)) || !automation.debounceDelay;
    if (automation.autoRun && isDebounced) {
      runAutomation(automation);
      lastDebounceTimes[i] = millis();
    }
  }
}

void pickUpQueuedAutomations() {
  for (int i=0; i<MAX_AUTOMATIONS_NUMBER; i++) {
    if (telegramhandler->automationsQueued[i] == 0) break;
    #ifdef __debug
      Serial.printf("Telegram automation id queued detected: %i\n",telegramhandler->automationsQueued[i]);
    #endif
    runAutomation(telegramhandler->automationsQueued[i]);
  }
  for (int i=0; i<MAX_AUTOMATIONS_NUMBER; i++) {
    if (serverhandler->automationsQueued[i] == 0) break;
    #ifdef __debug
      Serial.printf("Server automation id queued detected: %i\n",serverhandler->automationsQueued[i]);
    #endif
    runAutomation(serverhandler->automationsQueued[i]);
  }
  for (int i=0; i<MAX_AUTOMATIONS_NUMBER; i++) {
    if (mqtthandler->automationsQueued[i] == 0) break;
    #ifdef __debug
      Serial.printf("Mqtt automation id queued detected: %i\n",mqtthandler->automationsQueued[i]);
    #endif
    runAutomation(mqtthandler->automationsQueued[i]);
  }
  // Reset queues once automations executed
  memset(telegramhandler->automationsQueued, 0, sizeof(telegramhandler->automationsQueued));
  memset(serverhandler->automationsQueued, 0, sizeof(serverhandler->automationsQueued));
  memset(mqtthandler->automationsQueued, 0, sizeof(mqtthandler->automationsQueued));
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
    Serial.printf("Checking automation: %s\n",automation.label);
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
        const int16_t value =  gpio.mode>0 ? digitalRead(gpio.pin) : ledcRead(gpio.channel);
        if (automation.conditions[i][1] == 1) {
          criteria = (value == automation.conditions[i][2]);
        } else if (automation.conditions[i][1] == 2) {
          criteria = (value != automation.conditions[i][2]);
        } else if (automation.conditions[i][1] == 3) {
          criteria = (value > automation.conditions[i][2]);
        } else if (automation.conditions[i][1] == 4) {
          criteria = (value < automation.conditions[i][2]);
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
    #ifdef __debug
      Serial.printf("Running automation: %s\n",automation.label);
    #endif
    // Run automation
    for (int repeat=0; repeat<automation.loopCount; repeat++) {
      for (int i=0;i<MAX_AUTOMATIONS_NUMBER;i++) {
        if (automation.actions[i][0] && strlen(automation.actions[i][0])!=0) {
          #ifdef __debug
            Serial.printf("Running action type: %s\n",automation.actions[i][0]);
          #endif
          const int type = atoi(automation.actions[i][0]);
          // We deal with a gpio action
          if (type == 1 && automation.actions[i][2] && strlen(automation.actions[i][2])!=0) {
            int pin = atoi(automation.actions[i][2]);
            int16_t value = atoi(automation.actions[i][1]);
            int8_t assignmentType = atoi(automation.actions[i][3]);
            int16_t newValue = value;
            GpioFlash& gpio = preferencehandler->gpios[pin];
            int16_t currentValue = (gpio.mode>0 ? digitalRead(pin) : ledcRead(gpio.channel));
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
            } else {
              ledcWrite(gpio.channel, newValue);
            }
          // Send telegram message action
          } else if (type == 2) {
            String value = String(automation.actions[i][1]);
            parseActionString(value);
            telegramhandler->queueMessage(value.c_str());
          // Delay type action
          } else if (type == 3) {
            delay(atoi(automation.actions[i][1]));
          // Http request
          } else if (type == 4) {
            String value = String(automation.actions[i][1]);
            parseActionString(value);
            Serial.print(value.c_str());
          // Http request
          } else if (type == 5) {
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
                Serial.print("[Action HTTP] HTTP Response code: ");
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
        } else {
          break;
        }
      }
    }
  }
  // Run next automation if we are not trying to do a nauty infinite loop
  if (canRun && automation.nextAutomationId && automation.nextAutomationId != automation.id) {
    for (AutomationFlash& nAutomation: preferencehandler->automations) {
      if (nAutomation.id == automation.nextAutomationId) {
        #ifdef __debug
          Serial.printf("Going to next automation: %s\n",nAutomation.label);
        #endif
        runAutomation(nAutomation);
        break;
      }
    }
  }
}

// Iterate through a string to find special command like `${pinNumber}` or `${info}` and replace them with the appropriate value
void parseActionString(String& toParse) {
  toParse.replace("${info}", systemInfos());
  addPinValueToActionString(toParse, 0);
}

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
        int state;
        GpioFlash& gpio = preferencehandler->gpios[pinNumber];
        if (gpio.pin == pinNumber) {
            if (gpio.mode>0) {
                state = digitalRead(pinNumber);
            } else {
                state = ledcRead(gpio.channel);
            }
        }
        String subStringToReplace = String("${") + pinNumber + '}';
        toParse.replace(subStringToReplace, String(state));
      }
    }
  }
  if (foundIndex) {
    addPinValueToActionString(toParse,foundIndex);
  }
}

void setup(void)
{
  Serial.begin(115200);
  Serial.println("Access point set.\nWifi network: ESP32");
  WiFi.mode(WIFI_STA);
  WiFiManager wm;
  wm.setConnectTimeout(10);
  bool res = wm.autoConnect(APName,APPassword);
  if(!res) {
        Serial.println("Failed to connect");
  } else {
    // Set all handlers.
    preferencehandler = new PreferenceHandler();
    preferencehandler->begin();
    serverhandler = new ServerHandler(*preferencehandler);
    serverhandler->begin();
    telegramhandler = new TelegramHandler(*preferencehandler, client);
    mqtthandler = new MqttHandler(*preferencehandler, clientNotSecure);
     //init and get the time
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    // Set input reading on a different thread
    xTaskCreatePinnedToCore(input_loop, "automation", 12288, NULL, 0, NULL, 0);

    struct tm timeinfo;
    if(!getLocalTime(&timeinfo)){
      Serial.println("Failed to obtain time");
    } else {
      // This avoid having to check getLocalTime in the loop, saving some complexity.
      debounceTimeDelay -= timeinfo.tm_sec; 
      lastCheckedTime = millis();
    }
  }
}

void loop(void) {
  if ( WiFi.status() ==  WL_CONNECTED ) {
    serverhandler->server.handleClient();
    mqtthandler->handle();
    telegramhandler->handle();
  } else {
    // wifi down, reconnect here
    WiFi.begin();
    int count = 0;
    while (WiFi.status() != WL_CONNECTED && count < 200 ) 
    {
      delay(500);
      ++count;
      #ifdef __debug
        Serial.printf("Wifi deconnected: attempt %i\n", count);
      #endif
      if (count == 200) {
        Serial.println("Failed to reconnect, restarting now.");
        ESP.restart();
      } 
    }
  }
}

void input_loop(void *pvParameters) {
  while(1) {
    if (WiFi.status() ==  WL_CONNECTED) {
      readPins();
      pickUpQueuedAutomations();
      if (millis() > debounceTimeDelay + lastCheckedTime) {
        // If this is the first time this run, we are now sure it will run every minutes, so set back debounceTimeDelay to 60s.
        if (debounceTimeDelay != 60000) {
          debounceTimeDelay = 60000;
        }
        runAllRunnableAutomations();
        lastCheckedTime = millis();
      }
      vTaskDelay(10);
    }
  }
}
