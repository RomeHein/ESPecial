#include <WiFiManager.h>
#include <WiFi.h>
#include <WiFiClient.h>

#include <TFT_eSPI.h>
#include <Button2.h>
#include "ServerHandler.h"
#include "PreferenceHandler.h"
#include "TelegramHandler.h"

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

const char *APName = "ESP32";
const char *APPassword = "p@ssword2000";

int buttonCursor = 0;

void button_init()
{
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  btn1.setLongClickHandler([](Button2 &b) {
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(1, 0);
    tft.print("Turned off all");
    switchAllGpioState(false);
  });
  btn2.setLongClickHandler([](Button2 &b) {
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(1, 0);
    tft.print("Turned on all");
    switchAllGpioState(true);
  });
  btn1.setPressedHandler([](Button2 &b) {
    if (buttonCursor < sizeof(preferencehandler->gpios)/sizeof(*preferencehandler->gpios) - 1)
    {
      buttonCursor++;
    }
    else
    {
      buttonCursor = 0;
    }
    displayGpioState(preferencehandler->gpios[buttonCursor]);
  });

  btn2.setPressedHandler([](Button2 &b) {
    switchSelectedGpioState();
    displayGpioState(preferencehandler->gpios[buttonCursor]);
  });
}

void button_loop()
{
  btn1.loop();
  btn2.loop();
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
  tft.setCursor(1, 0);
  tft.println(gpio.label);
  tft.print("Pin ");
  tft.println(gpio.pin);
  tft.print("Actual state:");
  tft.print(digitalRead(gpio.pin));
}

void server_init () 
{
  tft.setCursor(2, 0);
  tft.fillScreen(TFT_BLACK);
  tft.println("HTTP server started on:");
  tft.println(WiFi.localIP());
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
        if (gpio.pin && gpio.mode != OUTPUT) {
            gpio.state = digitalRead(gpio.pin);
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
  tft.println("Access point set. Please connect to ESP32 wifi network");
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
    telegramhandler = new TelegramHandler(*preferencehandler);
    button_init();
    server_init();
  }
}

void loop(void)
{
  if ( WiFi.status() ==  WL_CONNECTED ) 
  {
    button_loop();
    serverhandler->server.handleClient();
    readInputPins();
    telegramhandler->handle();
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
      if (count == 200) {
        Serial.println("Failed to reconnect, restarting now.");
        ESP.restart();
      } 
    }
  }
}
