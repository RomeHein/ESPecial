#include <WiFiManager.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <TFT_eSPI.h>
#include <Button2.h>

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
#define BUTTON_1 35
#define BUTTON_2 0
#define LED 2

TFT_eSPI tft(135, 240);
Button2 btn1(BUTTON_1);
Button2 btn2(BUTTON_2);
ServerHandler *serverhandler;
PreferenceHandler *preferencehandler;
TelegramHandler *telegramhandler;
MqttHandler *mqtthandler;
WiFiClientSecure client;
WiFiClient clientNotSecure;

// ESP32 access point mode
const char *APName = "ESP32";
const char *APPassword = "p@ssword2000";

// The button cursor indicates which pin is selected to display on the tft screen. -1 means we'll display system informations
int buttonCursor = -1;

// Delay between each tft display refresh 
int delayBetweenScreenUpdate = 3000;
long screenRefreshLastTime;

void button_init()
{
  btn1.setLongClickHandler([](Button2 &b) {
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(1, 0);
    tft.print("Turned off all");
    switchAllGpioState(false);
  });
  btn2.setLongClickHandler([](Button2 &b) {
    // Go back to info screen;
    buttonCursor = -1;
    displayServicesInfo();
  });
  btn1.setPressedHandler([](Button2 &b) {
    do {
      if (buttonCursor < GPIO_PIN_COUNT - 1) {
        buttonCursor++;
        if (preferencehandler->gpios[buttonCursor].pin) {
          displayGpioState(preferencehandler->gpios[buttonCursor]);
        }
      } else {
        buttonCursor = -1;
      }
    } while (buttonCursor != -1 && !preferencehandler->gpios[buttonCursor].pin);
      
    #ifdef __debug
      Serial.printf("Cursor: %i\n",buttonCursor);
    #endif
  });

  btn2.setPressedHandler([](Button2 &b) {
    if (buttonCursor > -1) {
      switchSelectedGpioState();
      displayGpioState(preferencehandler->gpios[buttonCursor]);
    }
  });
}

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

  if (TFT_BL > 0)
  {                                         // TFT_BL has been set in the TFT_eSPI library in the User Setup file TTGO_T_Display.h
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
void displayGpioState(GpioFlash& gpio)
{
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(1, 0);
  tft.printf("%s\nPin %i\nActual state: %i",gpio.label,gpio.pin,digitalRead(gpio.pin));
}

void displayServicesInfo () 
{
  if (buttonCursor == -1 && millis() > screenRefreshLastTime + delayBetweenScreenUpdate) {
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
  }
}

void switchSelectedGpioState()
{
  int currentstate = digitalRead(preferencehandler->gpios[buttonCursor].pin);
  digitalWrite(preferencehandler->gpios[buttonCursor].pin, currentstate == 0 ? 1 : 0);
}

void switchAllGpioState(bool on)
{
  for (GpioFlash& gpio : preferencehandler->gpios) {
    digitalWrite(gpio.pin, on);
  }
}

void readInputPins() {
  for (GpioFlash& gpio : preferencehandler->gpios) {
    if (gpio.pin) {
      int newState = digitalRead(gpio.pin);
      if (gpio.state != newState) {
        #ifdef __debug
          Serial.printf("Gpio pin %i state changed. Old: %i, new: %i\n",gpio.pin, gpio.state, newState);
	      #endif
        gpio.state = newState;
        mqtthandler->publish(gpio.pin);
      }
    }
  }
}

void setup(void)
{
  pinMode(LED, OUTPUT);
  digitalWrite(LED, 0);
  Serial.begin(115200);

  tft_init();
  
  WiFi.mode(WIFI_STA);
  WiFiManager wm;
  tft.println("Access point set.\nWifi network: ESP32");
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
    // Handle buttons on core 0, core 1 pretty full with server,mqtt and telegram...
    button_init();
    xTaskCreatePinnedToCore(button_loop, "buttons", 4096, NULL, 0, NULL, 0);
  }
}

void loop(void)
{
  if ( WiFi.status() ==  WL_CONNECTED ) 
  {
    serverhandler->server.handleClient();
    telegramhandler->handle();
    mqtthandler->handle();
    readInputPins();
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

void button_loop(void *pvParameters)
{
  while(1) {
    if ( WiFi.status() ==  WL_CONNECTED ) {
      btn1.loop();
      btn2.loop();
    }
    delay(50);
  }
}
