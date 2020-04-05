#include "TelegramHandler.h"
#include "PreferenceHandler.h"
#include <ArduinoJson.h>

int Bot_mtbs = 1000; //mean time between scan messages
long Bot_lasttime;   //last time messages' scan has been done

void TelegramHandler::begin() {
    if (!bot && preference.telegram.token) {
      #ifdef _debug  
        Serial.println("Telegram: init");
      #endif
      bot = new UniversalTelegramBot(preference.telegram.token, client);
    }
}
void TelegramHandler::handle()
{
    begin();
    if (bot && preference.telegram.token && preference.telegram.active && millis() > Bot_lasttime + Bot_mtbs)  {
      preference.health.telegram = 1;
        int numNewMessages = bot->getUpdates(bot->last_message_received + 1);
        while(numNewMessages) {
          #ifdef _debug  
            Serial.printf("Telegram: got %i new messages\n", numNewMessages);
          #endif
          handleNewMessages(numNewMessages);
          numNewMessages = bot->getUpdates(bot->last_message_received + 1);
        }
        Bot_lasttime = millis();
    } else {
      preference.health.telegram = 0;
    }
}

String TelegramHandler::generateInlineKeyboards() {
  const size_t capacity = JSON_ARRAY_SIZE(1+(GPIO_PIN_COUNT/2)) + GPIO_PIN_COUNT*(JSON_OBJECT_SIZE(2)+100);
  StaticJsonDocument<capacity> doc;
  for (int i = 0; i<GPIO_PIN_COUNT; i++) {
    JsonArray subArray = doc.createNestedArray();
    if (preference.gpios[i].pin && preference.gpios[i].mode == OUTPUT) {
        JsonObject object = subArray.createNestedObject();
        char text[50];
        snprintf(text, sizeof(text), "%s %s", preference.gpios[i].state ? "🔆" : "🌙", preference.gpios[i].label);
        object["text"] = text;
        object["callback_data"] = preference.gpios[i].pin;
    }
    do
    {
      i++;
    } while (i<GPIO_PIN_COUNT && !preference.gpios[i].pin && preference.gpios[i].mode != OUTPUT);
    if (preference.gpios[i].pin && preference.gpios[i].mode == OUTPUT) {
      JsonObject object = subArray.createNestedObject();
      char text[50];
      snprintf(text, sizeof(text), "%s %s", preference.gpios[i].state ? "🔆" : "🌙", preference.gpios[i].label);
      object["text"] = text;
      object["callback_data"] = preference.gpios[i].pin;
    }
  }
  String output;
  serializeJson(doc, output);
  return output;
}

void TelegramHandler::handleNewMessages(int numNewMessages) {

  for (int i = 0; i < numNewMessages; i++) {

    // Inline buttons with callbacks when pressed will raise a callback_query message
    if (bot->messages[i].type == "callback_query") {
      preference.setGpioState(atoi(bot->messages[i].text.c_str()), -1);
      bot->sendMessageWithInlineKeyboard(bot->messages[i].chat_id, "Gpios available in output mode", "", generateInlineKeyboards(), bot->messages[i].message_id);
    } else {
      String chat_id = String(bot->messages[i].chat_id);
      String text = bot->messages[i].text;

      String from_name = bot->messages[i].from_name;
      if (from_name == "") from_name = "Guest";

      if (text == "/cmd") {
        bot->sendMessageWithInlineKeyboard(chat_id, "Gpios available in output mode", "", generateInlineKeyboards());
      }

      if (text == "/start") {
        String welcome = "Welcome to your ESP32 telegram bot, " + from_name + ".\n";
        welcome += "Here you'll be able to control states of your esp32 gpios.\n\n";
        welcome += "/cmd : returns an inline keyboard to control gpios set in output mode\n";
        welcome += "/st : returns gpios' state in input mode\n";

        bot->sendMessage(chat_id, welcome, "Markdown");
      }
    }
  }
}

