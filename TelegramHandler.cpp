#include "TelegramHandler.h"
#include "PreferenceHandler.h"
#include <ArduinoJson.h>

//unmark following line to enable debug mode
#define __debug

int Bot_mtbs = 1000; //mean time between scan messages
long Bot_lasttime;   //last time messages' scan has been done

void TelegramHandler::begin() {
    if (!isInit && preference.telegram.active && preference.telegram.token) {
      #ifdef __debug  
        Serial.println("Telegram: init");
      #endif
      bot = new UniversalTelegramBot(preference.telegram.token, client);
      isInit = true;
    }
}
void TelegramHandler::handle()
{
    begin();
    if (isInit && preference.telegram.token && preference.telegram.active && millis() > Bot_lasttime + Bot_mtbs)  {
      preference.health.telegram = 1;
      int numNewMessages = bot->getUpdates(bot->last_message_received + 1);
      while(numNewMessages) {
        #ifdef __debug  
          Serial.printf("Telegram: got %i new messages\n", numNewMessages);
        #endif
        handleNewMessages(numNewMessages);
        numNewMessages = bot->getUpdates(bot->last_message_received + 1);
      }
      // Empty messages queued
      sendQueuedMessages();
      Bot_lasttime = millis();
    } else if (!isInit || ! preference.telegram.token || !preference.telegram.active) {
      preference.health.telegram = 0;
    }
}

String TelegramHandler::generateButtonFormat(GpioFlash& gpio) {
  const size_t capacity = JSON_OBJECT_SIZE(2) + 200;
  DynamicJsonDocument doc(capacity);
  char text[50];
  snprintf(text, sizeof(text), "%s %s", gpio.state ? "🔆" : "🌙", gpio.label);
  doc["text"] = text;
  char callback[50];
  snprintf(callback, sizeof(callback), "g-%i", gpio.pin);
  doc["callback_data"] = callback;
  String output;
  serializeJson(doc, output);
  Serial.printf("Inline keyboard: %s\n",output.c_str());
  return output;
}

String TelegramHandler::generateButtonFormat(AutomationFlash& a) {
  const size_t capacity = JSON_OBJECT_SIZE(2) + 200;
  DynamicJsonDocument doc(capacity);
  doc["text"] = a.label;
  char callback[50];
  snprintf(callback, sizeof(callback), "a-%i", a.id);
  doc["callback_data"] = callback;
  String output;
  serializeJson(doc, output);
  Serial.printf("Inline keyboard: %s\n",output.c_str());
  return output;
}

String TelegramHandler::generateInlineKeyboardsForGpios(bool inputMode) {
  const size_t capacity = JSON_ARRAY_SIZE(GPIO_PIN_COUNT) + GPIO_PIN_COUNT*(JSON_OBJECT_SIZE(2) + 200);
  DynamicJsonDocument doc(capacity);
  // Dummy code to create 2 buttons per row in telegram api format
  for (int i = 0; i<GPIO_PIN_COUNT; i++) {
    JsonArray subArray = doc.createNestedArray();
    Serial.printf("Gpio index %i pin %i, mode %i\n",i,preference.gpios[i].pin,preference.gpios[i].mode);
    if (preference.gpios[i].pin && ((preference.gpios[i].mode == OUTPUT && !inputMode)||(preference.gpios[i].mode == INPUT && inputMode))) {
        subArray.add(serialized(generateButtonFormat(preference.gpios[i])));
    }
    do
    {
      i++;
    } while (i<GPIO_PIN_COUNT && ((preference.gpios[i].mode != OUTPUT && !inputMode)||(preference.gpios[i].mode != INPUT && inputMode)));
    Serial.printf("Gpio index %i pin %i, mode %i\n",i,preference.gpios[i].pin,preference.gpios[i].mode);
    if (preference.gpios[i].pin && ((preference.gpios[i].mode == OUTPUT && !inputMode)||(preference.gpios[i].mode == INPUT && inputMode))) {
        subArray.add(serialized(generateButtonFormat(preference.gpios[i])));
    }
  }
  String output;
  serializeJson(doc, output);
  Serial.printf("Inline keyboard: %s\n",output.c_str());
  return output;
}

String TelegramHandler::generateInlineKeyboardsForAutomations() {
  const size_t capacity = JSON_ARRAY_SIZE(1+(MAX_AUTOMATIONS_NUMBER/2)) + MAX_AUTOMATIONS_NUMBER*(JSON_OBJECT_SIZE(2)+100);
  DynamicJsonDocument doc(capacity);
  for (int i = 0; i<MAX_AUTOMATIONS_NUMBER; i++) {
    JsonArray subArray = doc.createNestedArray();
    if (preference.automations[i].id) {
        subArray.add(serialized(generateButtonFormat(preference.automations[i])));
    }
    do
    {
      i++;
    } while (i<MAX_AUTOMATIONS_NUMBER && !preference.automations[i].id);
    if (preference.automations[i].id && i<MAX_AUTOMATIONS_NUMBER) {
        subArray.add(serialized(generateButtonFormat(preference.automations[i])));
    }
  }
  String output;
  serializeJson(doc, output);
  return output;
}

void TelegramHandler::handleNewMessages(int numNewMessages) {

  for (int i = 0; i < numNewMessages; i++) {
    bool authorised = false;
    int userId = atoi(bot->messages[i].from_id.c_str());
    // Check first if the user sending the message is part of the authorised list
    for (int id: preference.telegram.users) {
      if (id == userId) {
        authorised = true;
        break;
      }
    }
    // Inline buttons with callbacks when pressed will raise a callback_query message
    if (bot->messages[i].type == "callback_query") {
      const char *cmd = bot->messages[i].text.c_str();
      char id_c[5];
      strncpy(id_c, cmd+2, strlen(cmd)-1);
      int id = atoi(id_c);
      #ifdef __debug  
        Serial.printf("Telegram: command: %c, id: %i\n",cmd[0], id);
      #endif
      // 'g' is a command for gpios
      if (cmd[0] == 'g') {
        // We set the persist flag to false, to allow the mainloop to pick up new changes and react accordingly
        preference.setGpioState(id, -1);
        bot->sendMessageWithInlineKeyboard(bot->messages[i].chat_id, "Gpios available in output mode", "", generateInlineKeyboardsForGpios(), bot->messages[i].message_id);
      // 'a' is a command for automations
      } else {
        // queue automation id, to be picked up by esp32.ino script
        for (int i=0; i<MAX_AUTOMATIONS_NUMBER; i++) {
          if (automationsQueued[i] == 0) {
            automationsQueued[i] = id;
            break;
          }
        }
      }
    } else {
      String chat_id = String(bot->messages[i].chat_id);
      String text = bot->messages[i].text;
      String from_name = bot->messages[i].from_name;
      if (from_name == "") from_name = "Guest";

      if (authorised && text == "/out") {
        bot->sendMessageWithInlineKeyboard(chat_id, "Gpios available in output mode", "", generateInlineKeyboardsForGpios());
      } else if (authorised && text == "/in") {
        bot->sendMessageWithInlineKeyboard(chat_id, "Gpios available in input mode", "", generateInlineKeyboardsForGpios(true));
      } else if (authorised && text == "/auto") {
        bot->sendMessageWithInlineKeyboard(chat_id, "Automations list", "", generateInlineKeyboardsForAutomations());
      }else if (text == "/start") {
        char welcome[512];
        snprintf(welcome,512,"Welcome to your ESP32 telegram bot, %s.\nHere you'll be able to control your ESP32.\nFirst, add your telegram id to the authorised list (in settings panel) to restrict your bot access:\n %s.\n\n/out : returns an inline keyboard to control gpios set in output mode\n/in : returns gpios' state in input mode\n/analog : returns gpios' attached to channels (in analogue mode)\n/auto : returns an inline keyboard to trigger automations\n",from_name,bot->messages[i].from_id);
        bot->sendMessage(chat_id, welcome, "Markdown");
      } else {
        char mes[256];
        snprintf(mes,256,"⛔️You are not an authorised user.\nPlease add your telegram id to the user list in setting panel:\n%s",bot->messages[i].from_id);
        bot->sendMessage(chat_id, mes);
      }

      // We save here the current chat_id to the corresponding userId, in case we want to send message later on
      int intchatId = atoi(chat_id.c_str());
      for (int i=0; i<MAX_TELEGRAM_USERS_NUMBER; i++ ) {
        if (preference.telegram.users[i] == userId && preference.telegram.chatIds[i] != intchatId) {
          preference.telegram.chatIds[i] = intchatId;
          preference.save(PREFERENCES_TELEGRAM);
          break;
        }
      }
    }
  }
}

void TelegramHandler::queueMessage(const char* message) {
  if (isInit && preference.telegram.token && preference.telegram.active) {
    #ifdef __debug  
      Serial.printf("Telegram: queued message: %s\n",message);
    #endif
    if (lastMessageQueuedPosition<MAX_QUEUED_MESSAGE_NUMBER) {
      strcpy(messagesQueue[lastMessageQueuedPosition],message);
      lastMessageQueuedPosition++;
    } else {
      #ifdef __debug  
        Serial.printf("Telegram: reach message queued maximum: %i\n",MAX_QUEUED_MESSAGE_NUMBER);
      #endif
    }
  }
}

void TelegramHandler::sendQueuedMessages() {
  bool didSentMessage = false;
  for (int i=0; i<lastMessageQueuedPosition; i++) {
    // Send queued message to all registered users
    for (int i=0; i< MAX_TELEGRAM_USERS_NUMBER; i++) {
      // Check if we have a chatId corresponding to a telegram user
      if (preference.telegram.users[i] && preference.telegram.chatIds[i]) {
        #ifdef __debug  
          Serial.printf("Telegram: sending message: %s\n", messagesQueue[i]);
        #endif
        bot->sendMessage(String(preference.telegram.chatIds[i]), messagesQueue[i]);
        didSentMessage = true;
      } 
    }
  }
  if (!didSentMessage && lastMessageQueuedPosition != 0) {
    #ifdef __debug  
      Serial.println("Telegram: could not send messages: %s\nReason: no chatids defined, send a message first to the bot before using it.");
    #endif
  }
  // Empty queue
  if (didSentMessage) {
    Serial.print("Telegram: empty queued messages\n");
    memset(messagesQueue, 0, sizeof(messagesQueue));
    lastMessageQueuedPosition = 0; 
  }
}

