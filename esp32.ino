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

#define MAX_QUEUED_ACTIONS 10

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

// Debounce delay
long lastDebounceTime = 0;
int debounceDelay = 50;

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
  if (millis() > debounceDelay + lastDebounceTime) {
    bool couldRunAction = false;
    for (GpioFlash& gpio : preferencehandler->gpios) {
      if (gpio.pin) {
        int newState = digitalRead(gpio.pin);
        if (gpio.state != newState) {
          #ifdef __debug
            Serial.printf("Gpio pin %i state changed. Old: %i, new: %i\n",gpio.pin, gpio.state, newState);
          #endif
          gpio.state = newState;
          mqtthandler->publish(gpio.pin);
          // Now check for any runnable actions
          runAllRunnableActions();
          lastDebounceTime = millis();
        }
      }
    }
  }
}

void runAllRunnableActions() {
  for (ActionFlash& action: preferencehandler->actions) {
    if (action.autoRun) {
      runAction(action);
    }
  }
}

void pickUpQueuedActions() {
  for (int i=0; i<MAX_ACTIONS_NUMBER; i++) {
    if (telegramhandler->actionsQueued[i] == 0) break;
    #ifdef __debug
      Serial.printf("Telegram action id queued detected: %i\n",telegramhandler->actionsQueued[i]);
    #endif
    runAction(telegramhandler->actionsQueued[i]);
  }
  for (int i=0; i<MAX_ACTIONS_NUMBER; i++) {
    if (serverhandler->actionsQueued[i] == 0) break;
    #ifdef __debug
      Serial.printf("Server action id queued detected: %i\n",serverhandler->actionsQueued[i]);
    #endif
    runAction(serverhandler->actionsQueued[i]);
  }
  for (int i=0; i<MAX_ACTIONS_NUMBER; i++) {
    if (mqtthandler->actionsQueued[i] == 0) break;
    #ifdef __debug
      Serial.printf("Mqtt action id queued detected: %i\n",mqtthandler->actionsQueued[i]);
    #endif
    runAction(mqtthandler->actionsQueued[i]);
  }
  // Reset queues once actions executed
  memset(telegramhandler->actionsQueued, 0, sizeof(telegramhandler->actionsQueued));
  memset(serverhandler->actionsQueued, 0, sizeof(serverhandler->actionsQueued));
  memset(mqtthandler->actionsQueued, 0, sizeof(mqtthandler->actionsQueued));
}

void runAction(int id) {
  for (ActionFlash& action: preferencehandler->actions) {
    if (action.id == id) {
      runAction(action);
      break;
    }
  }
}

void runAction(ActionFlash& action) {
  #ifdef __debug
    Serial.printf("Checking action: %s\n",action.label);
  #endif
  // Check if all conditions are fullfilled
  bool canRun = true;
  for (int i=0; i<MAX_ACTIONS_NUMBER;i++) {
    // Ignore condition if we don't have a valid math operator, and if the previous condition had a logic operator at the end.
    if (action.conditions[i][1] && ((i>0 && action.conditions[i-1][3])||i==0)) {
      const long value = digitalRead(action.conditions[i][0]);
      bool criteria;
      if (action.conditions[i][1] == 1) {
        criteria = (value == action.conditions[i][2]);
      } else if (action.conditions[i][1] == 2) {
        criteria = (value != action.conditions[i][2]);
      } else if (action.conditions[i][1] == 3) {
        criteria = (value > action.conditions[i][2]);
      } else if (action.conditions[i][1] == 4) {
        criteria = (value < action.conditions[i][2]);
      }
      if (i == 0) {
        canRun = criteria;
      } else if (action.conditions[i-1][3] == 1) {
        canRun &= criteria;
      } else if (action.conditions[i-1][3] == 2) {
        canRun |= criteria;
      } else if (action.conditions[i-1][3] == 3) {
        canRun ^= criteria;
      }
    } else {
      break;
    }
  }
  if (canRun) {
    #ifdef __debug
      Serial.printf("Running with type: %i\n",action.type);
    #endif
    // Run action
    for (int repeat=0; repeat<action.loopCount; repeat++) {
      // Type 1: Add a message to the telegram queue. Telegram handle will pick it up and send it.
      if (action.type == 1 && action.mes) {
        telegramhandler->queueMessage(action.mes);
      // Type 2: Display message on the tft screen.
      } else if (action.type == 2 && action.mes) {
        tft.setCursor(2, 0);
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_WHITE);
        tft.println(action.mes);
      // Type 3: set pin to value
      } else if (action.type == 3) {
        digitalWrite(action.pinC, action.valueC);
      }
      if (action.delay) {
        delay(action.delay);
      }
    }
  }
  // Run next action if we are not trying to do a nauty infinite loop
  if (action.nextActionId && action.nextActionId != action.id) {
    for (ActionFlash& nAction: preferencehandler->actions) {
      if (nAction.id == action.nextActionId) {
        #ifdef __debug
          Serial.printf("Going to next action: %s\n",nAction.label);
        #endif
        runAction(nAction);
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
  bool res;
  res = wm.autoConnect(APName,APPassword);
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
      pickUpQueuedActions();
      vTaskDelay(10);
    }
  }
}
