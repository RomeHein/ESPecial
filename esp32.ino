#include <WiFiManager.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Update.h>
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

// Notify client esp has just restarted
bool hasJustRestarted = true;

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

// Repo info for updates
const String repoPath = "https://raw.githubusercontent.com/RomeHein/ESPecial/v0.2/versions/";

String systemInfos() {
  String infos;
  infos = String("\nFree memory:") + ESP.getFreeHeap();
  infos += "\nTelegram bot:" + preferencehandler->health.telegram;
  infos += "\nMqtt server:" + preferencehandler->health.mqtt;
  return infos;
}

String getFirmwareList() {
  HTTPClient http;
  const String firmwarelistPath = repoPath + "list.json";
  http.begin(client,firmwarelistPath.c_str()) ;
  int httpResponseCode = http.GET();
  if (httpResponseCode>0) {
    Serial.print("[MAIN] Retrieve firmware list ");
    return http.getString();
  }
  else {
    Serial.printf("[MAIN] Could not get firmware list: %s\n", http.errorToString(httpResponseCode).c_str());
  }
  http.end();
}

// Utility to extract header value from headers
String getHeaderValue(String header, String headerName) {
  return header.substring(strlen(headerName.c_str()));
}

void execOTA(double version) {
  char* error;
  long contentLength = 0;
  bool isValidContentType = false;  
  const String bin = version+String("/especial.ino.bin");
  const String binPath = repoPath+bin;
  Serial.println("[OTA] Download start");
  if (client.connect(binPath.c_str(), 80)) {
    Serial.println("[OTA] Connection ok. Fetching bin");
    // Get the contents of the bin file
    client.print(String("GET ") + bin + " HTTP/1.1\r\n" +
                 "Host: " + repoPath + "\r\n" +
                 "Cache-Control: no-cache\r\n" +
                 "Connection: close\r\n\r\n");

    unsigned long timeout = millis();
    while (client.available() == 0) {
      if (millis() - timeout > 5000) {
        error = "Client Timeout";
        client.stop();
        return;
      }
    }
    // Once the response is available,
    // check stuff
    while (client.available()) {
      String line = client.readStringUntil('\n');
      // remove space, to check if the line is end of headers
      line.trim();

      // if the the line is empty, this is end of headers
      // break the while and feed the remaining `client` to the Update.writeStream();
      if (!line.length()) {
        break; // get the OTA started
      }

      // Check if the HTTP Response is 200
      // else break and Exit Update
      if (line.startsWith("HTTP/1.1")) {
        if (line.indexOf("200") < 0) {
          error = "Non 200 status code from server.";
          break;
        }
      }

      // extract headers here
      if (line.startsWith("Content-Length: ")) {
        contentLength = atol((getHeaderValue(line, "Content-Length: ")).c_str());
        Serial.println("Got " + String(contentLength) + " bytes from server");
      }
      if (line.startsWith("Content-Type: ")) {
        String contentType = getHeaderValue(line, "Content-Type: ");
        Serial.println("Got " + contentType + " payload.");
        if (contentType == "application/octet-stream") {
          isValidContentType = true;
        }
      }
    }
  } else {
    error = "Connection to host failed. Please check your setup";
  }

  // Notify client we have successfully downloaded new firmware
  if (!error) {
    if (serverhandler->events.count()>0) {
      serverhandler->events.send("Download success","firmwareDownloaded",millis());
    }
  }

  if (contentLength && isValidContentType) {
    // Check if there is enough to OTA Update
    bool canBegin = Update.begin(contentLength);

    if (canBegin) {
      Serial.println("[OTA] Starting");
      size_t written = Update.writeStream(client);

      if (written == contentLength) {
        Serial.println("Written : " + String(written) + " successfully");
      } else {
        Serial.println("Written only : " + String(written) + "/" + String(contentLength));
      }

      if (Update.end()) {
        Serial.println("[OTA] Done");
        if (Update.isFinished()) {
          Serial.println("[OTA] Rebooting.");
          ESP.restart();
        } else {
          error = "Update not finished. Something went wrong!";
        }
      } else {
        sprintf(error, "%d", Update.getError());
      }
    } else {
      error = "not enough space to begin OTA";
      client.flush();
    }
  } else {
    error = "no content in response";
    client.flush();
  }
  // If error notify any client
  if (error) {
    const String errorFormat = "[OTA] Error: " + String(error);
    Serial.println(errorFormat.c_str());
    if (serverhandler->events.count()>0) {
      serverhandler->events.send(errorFormat.c_str(),"firmwareUpdateError",millis());
    }
  }
}

bool checkAgainstLocalHour(int time, int signType) {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    #ifdef __debug
      Serial.println("[MAIN] Failed to obtain time");
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
    Serial.println("Failed to obtain time");
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
      runTriggeredEventAutomations(false);
    }    
    lastDebouncedInputTime = millis();
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
      Serial.printf("[AUTOMATION] Running automation: %s\n",automation.label);
    #endif
    // Run automation
    for (int repeat=0; repeat<automation.loopCount; repeat++) {
      for (int i=0;i<MAX_AUTOMATIONS_NUMBER;i++) {
        if (automation.actions[i][0] && strlen(automation.actions[i][0])!=0) {
          #ifdef __debug
            Serial.printf("[ACTION] Running action type: %s\n",automation.actions[i][0]);
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
            Serial.println(value.c_str());
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
          // Nested automation
          } else if (type == 6) {
            int nestedAutomationId = atoi(automation.actions[i][1]);
            for (AutomationFlash& nAutomation: preferencehandler->automations) {
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
  Serial.println(F("[SETUP] Access point set.\nWifi network: ESP32"));
  WiFi.mode(WIFI_STA);
  WiFiManager wm;
  wm.setConnectTimeout(10);
  bool res = wm.autoConnect(APName,APPassword);
  if(!res) {
        Serial.println(F("[SETUP] Failed to connect"));
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
    xTaskCreatePinnedToCore(automation_loop, "automation", 12288, NULL, 0, NULL, 0);
    // Get local time
    struct tm timeinfo;
    if(!getLocalTime(&timeinfo)){
      Serial.println(F("[SETUP] Failed to obtain time"));
    } else {
      // This avoid having to check getLocalTime in the loop, saving some complexity.
      debounceTimeDelay -= timeinfo.tm_sec; 
      lastCheckedTime = millis();
    }
  }
}

void loop(void) {
  // Reload firmware list
  if (serverhandler->shouldReloadFirmwareList) {
    serverhandler->shouldReloadFirmwareList = false;
    serverhandler->events.send(getFirmwareList().c_str(),"firmwareList",millis());
  }
  // Check restart flag from server
  if (serverhandler->shouldRestart) {
    serverhandler->shouldRestart = false;
    ESP.restart();
  }
  // Check if OTA has been requested from server
  if (serverhandler->shouldOTAFirmwareVersion>0) {
    // Reset shouldOTAFirmwareVersion to avoid multiple OTA from main loop
    const double version = serverhandler->shouldOTAFirmwareVersion;
    serverhandler->shouldOTAFirmwareVersion = 0;
    execOTA(version);
  }
  // Notify clients esp has restarted
  if (hasJustRestarted && serverhandler->events.count()>0) {
    serverhandler->events.send("rebooted","shouldRefresh",millis());
    hasJustRestarted = false;
  }
  if ( WiFi.status() ==  WL_CONNECTED ) {
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
        Serial.printf("[MAIN_LOOP] Wifi deconnected: attempt %i\n", count);
      #endif
      if (count == 200) {
        Serial.println(F("[MAIN_LOOP] Failed to reconnect, restarting now."));
        ESP.restart();
      } 
    }
  }
}

void automation_loop(void *pvParameters) {
  while(1) {
    if (WiFi.status() ==  WL_CONNECTED) {
      readPins();
      pickUpQueuedAutomations();
      if (millis() > debounceTimeDelay + lastCheckedTime) {
        // If this is the first time this run, we are now sure it will run every minutes, so set back debounceTimeDelay to 60s.
        if (debounceTimeDelay != 60000) {
          debounceTimeDelay = 60000;
        }
        // Run only time scheduled automations
        #ifdef __debug
          Serial.println(F("[AUTO_LOOP] Checking time scheduled automations"));
        #endif
        runTriggeredEventAutomations(true);
        lastCheckedTime = millis();
      }
      vTaskDelay(10);
    }
  }
}
