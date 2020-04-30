#include <WiFiManager.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <TFT_eSPI.h>

#include "ServerHandler.h"
#include "PreferenceHandler.h"
#include "TelegramHandler.h"
#include "MqttHandler.h"

//unmark following line to enable debug mode
#define __debug

#ifndef TFT_DISPOFF
#define TFT_DISPOFF 0x28
#endif

#ifndef TFT_SLPIN
#define TFT_SLPIN 0x10
#endif

#define TFT_MOSI 19
#define TFT_SCLK 18
#define TFT_CS 5
#define TFT_DC 16
#define TFT_RST 23

#define TFT_BL 4 // Display backlight control pin
#define ADC_EN 14
#define ADC_PIN 34

#define MAX_QUEUED_AUTOMATIONS 10

TFT_eSPI tft(135, 240);
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
long lastDebounceInputTime = 0;
int debounceInputDelay = 50;
// Keep tracks of last time for each automation run 
long lastDebounceTimes[MAX_AUTOMATIONS_NUMBER] = {};

int freeMemory;

void tft_init()
{
  tft.init();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(4);
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(0, 0);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(2);

  if (TFT_BL > 0){                          // TFT_BL has been set in the TFT_eSPI library in the User Setup file TTGO_T_Display.h
    pinMode(TFT_BL, OUTPUT);                // Set backlight pin to output mode
    digitalWrite(TFT_BL, TFT_BACKLIGHT_ON); // Turn backlight on. TFT_BACKLIGHT_ON has been set in the TFT_eSPI library in the User Setup file TTGO_T_Display.h
  }

  tft.setSwapBytes(true);
}

void turnOffScreen()
{
  tft.writecommand(TFT_DISPOFF);
  tft.writecommand(TFT_SLPIN);
}

void displayServicesInfo () 
{
  if (millis() > screenRefreshLastTime + delayBetweenScreenUpdate) {
    tft.setCursor(2, 0);
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE);
    tft.println("HTTP server:");
    if (preferencehandler->mqtt.active && preferencehandler->health.mqtt == 0) {
      tft.setTextColor(TFT_BLUE);
      tft.println("Waiting for MQTT\n");
    } else {
      tft.setTextColor(TFT_GREEN);
      tft.println(WiFi.localIP());
    }
    tft.setTextColor(TFT_WHITE);
    tft.print("MQTT: ");
    if (preferencehandler->mqtt.active && preferencehandler->health.mqtt == 0) {
      tft.setTextColor(TFT_BLUE);
      tft.println("conecting...");
    } else if (preferencehandler->mqtt.active && preferencehandler->health.mqtt == 1) {
      tft.setTextColor(TFT_GREEN);
      tft.println("connected");
    } else {
      tft.setTextColor(TFT_RED);
      tft.println("off");
    }
    tft.setTextColor(TFT_WHITE);
    tft.print("Telegram: ");
    if (preferencehandler->health.telegram == 1) {
      tft.setTextColor(TFT_GREEN);
      tft.println("online");
    } else {
      tft.setTextColor(TFT_RED);
      tft.println("offline");
    }
    screenRefreshLastTime = millis();

    #ifdef __debug
      tft.print("\nFree Memory:");
      tft.println(ESP.getFreeHeap(),DEC);
      tft.print("Memory loss:");
      tft.println(freeMemory - ESP.getFreeHeap(),DEC);
      freeMemory = ESP.getFreeHeap();
    #endif
  }
}

void readInputPins() {
  if (millis() > debounceInputDelay + lastDebounceInputTime) {
    for (GpioFlash& gpio : preferencehandler->gpios) {
      if (gpio.pin) {
        int newState = digitalRead(gpio.pin);
        if (gpio.state != newState) {
          #ifdef __debug
            Serial.printf("Gpio pin %i state changed. Old: %i, new: %i\n",gpio.pin, gpio.state, newState);
          #endif
          gpio.state = newState;
          mqtthandler->publish(gpio.pin);
          // Now check for any runnable automations
          runAllRunnableAutomations();
        }
      }
    }
    lastDebounceInputTime = millis();
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
  for (int i=0; i<MAX_AUTOMATIONS_NUMBER;i++) {
    // Ignore condition if we don't have a valid math operator, and if the previous condition had a logic operator at the end.
    if (automation.conditions[i][1] && ((i>0 && automation.conditions[i-1][3])||i==0)) {
      const long value = digitalRead(automation.conditions[i][0]);
      bool criteria;
      if (automation.conditions[i][1] == 1) {
        criteria = (value == automation.conditions[i][2]);
      } else if (automation.conditions[i][1] == 2) {
        criteria = (value != automation.conditions[i][2]);
      } else if (automation.conditions[i][1] == 3) {
        criteria = (value > automation.conditions[i][2]);
      } else if (automation.conditions[i][1] == 4) {
        criteria = (value < automation.conditions[i][2]);
      }
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
      Serial.printf("Running automation id: %i\n",automation.id);
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
              digitalWrite(atoi(automation.actions[i][2]), atoi(automation.actions[i][1]));
          // Send telegram message action
          } else if (type == 2) {
            telegramhandler->queueMessage(automation.actions[i][1]);
          // Display message on screen
          } else if (type == 3) {
            tft.setCursor(2, 0);
            tft.fillScreen(TFT_BLACK);
            tft.setTextColor(TFT_WHITE);
            tft.println(automation.actions[i][1]);
          // Delay
          } else if (type == 4) {
            delay(atoi(automation.actions[i][1]));
          }
        } else {
          break;
        }
      }
    }
  }
  // Run next automation if we are not trying to do a nauty infinite loop
  if (automation.nextAutomationId && automation.nextAutomationId != automation.id) {
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

void setup(void)
{
  Serial.begin(115200);
  tft_init();
  tft.println("Access point set.\nWifi network: ESP32");
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
    // Set input reading on a different thread
    // Note: could maybe replaced by the telegramHandler
    xTaskCreatePinnedToCore(input_loop, "readInputs", 12288, NULL, 0, NULL, 0);
  }
}

void loop(void)
{
  if ( WiFi.status() ==  WL_CONNECTED ) {
    serverhandler->server.handleClient();
    mqtthandler->handle();
    telegramhandler->handle();
    displayServicesInfo();
  } else {
    // wifi down, reconnect here
    WiFi.begin();
    int count = 0;
    while (WiFi.status() != WL_CONNECTED && count < 200 ) 
    {
      delay(500);
      tft.setCursor(2, 0);
      tft.fillScreen(TFT_BLACK);
      tft.printf("Wifi deconnected: attempt %i", count);
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

void input_loop(void *pvParameters)
{
  while(1) {
    if (WiFi.status() ==  WL_CONNECTED) {
      readInputPins();
      pickUpQueuedAutomations();
      vTaskDelay(10);
    }
  }
}
