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

  if (TFT_BL > 0){                                         // TFT_BL has been set in the TFT_eSPI library in the User Setup file TTGO_T_Display.h
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
          for (ConditionFlash& condition: preferencehandler->conditions) {
            if (condition.pin == gpio.pin) {
              #ifdef __debug
                Serial.printf("Impacted condition detected: %s\n",condition.label);
              #endif
              couldRunAction = true;
            }
          }
          lastDebounceTime = millis();
        }
      }
    }
    if (couldRunAction) {
      runAllRunnableActions();
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

void runAction(ActionFlash& action) {
  #ifdef __debug
    Serial.printf("Checking action: %s\n",action.label);
  #endif
  // Check if all conditions are fullfilled
  bool canRun = true;
  for (int conditionId: action.conditions) {
    if (conditionId) {
      for (ConditionFlash& condition: preferencehandler->conditions) {
        if (condition.id == conditionId) {
          const long value = digitalRead(condition.pin);
          const bool criteria = (condition.type == 1 && value == condition.value) || (condition.type == 2 && value > condition.value) || (condition.type == 3 && value < condition.value);
          canRun &= criteria;
          #ifdef __debug
            Serial.printf("Condition: %s result: %u\n",condition.label, criteria);
          #endif
          break;
        }
        if (!canRun) {
          break;
        }
      } 
    }
  }
  if (canRun) {
    #ifdef __debug
      Serial.printf("Running with type: %i\n",action.type);
    #endif
    // Run action
    for (int repeat=0; repeat<action.loopCount; repeat++) {
      if (action.type == 1 && action.mes) {
        telegramhandler->queueMessage(action.mes);
      } else if (action.type == 2) {

      } else if (action.type == 3) {
        digitalWrite(action.pinC, action.valueC);
      }
      if (action.delay) {
        delay(action.delay);
      }
    }
  }
  // Run next action if we are not trying to do an infinite loop
  if (canRun && action.nextActionId && action.nextActionId != action.id) {
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
        // ESP.restart();
  } else {
    preferencehandler = new PreferenceHandler();
    preferencehandler->begin();
    serverhandler = new ServerHandler(*preferencehandler);
    serverhandler->begin();
    telegramhandler = new TelegramHandler(*preferencehandler, client);
    mqtthandler = new MqttHandler(*preferencehandler, clientNotSecure);

    xTaskCreatePinnedToCore(input_loop, "readInputs", 12288, NULL, 0, NULL, 0);
  }
}

void loop(void)
{
  if ( WiFi.status() ==  WL_CONNECTED ) 
  {
    serverhandler->server.handleClient();
    mqtthandler->handle();
    telegramhandler->handle();
    displayServicesInfo();
  } else
  {
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
      vTaskDelay(10);
    }
  }
}
