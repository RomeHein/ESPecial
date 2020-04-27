#include "TelegramHandler.h"
#include "PreferenceHandler.h"
#include <ArduinoJson.h>

//unmark following line to enable debug mode
#define __debug

int Bot_mtbs = 1000; //mean time between scan messages
long Bot_lasttime;   //last time messages' scan has been done

void TelegramHandler::begin() {
    if (!isInit && preference.telegram.token) {
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
      for (int i=0; i<lastMessageQueuedPosition; i++) {
        #ifdef __debug  
          Serial.printf("Telegram: sending message: %s\n", messagesQueue[i]);
        #endif
        bot->sendMessage(preference.telegram.currentChatId, messagesQueue[i]);
      }
      memset(messagesQueue, 0, sizeof(messagesQueue));
      lastMessageQueuedPosition = 0;
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
  return output;
}

String TelegramHandler::generateButtonFormat(ActionFlash& action) {
  const size_t capacity = JSON_OBJECT_SIZE(2) + 200;
  DynamicJsonDocument doc(capacity);
  doc["text"] = action.label;
  char callback[50];
  snprintf(callback, sizeof(callback), "a-%i", action.id);
  doc["callback_data"] = callback;
  String output;
  serializeJson(doc, output);
  return output;
}

String TelegramHandler::generateInlineKeyboardsForGpios() {
  const size_t capacity = JSON_ARRAY_SIZE(1+(GPIO_PIN_COUNT/2)) + GPIO_PIN_COUNT*(JSON_OBJECT_SIZE(2)+100);
  DynamicJsonDocument doc(capacity);
  for (int i = 0; i<GPIO_PIN_COUNT; i++) {
    JsonArray subArray = doc.createNestedArray();
    if (preference.gpios[i].pin && preference.gpios[i].mode == OUTPUT) {
        subArray.add(serialized(generateButtonFormat(preference.gpios[i])));
    }
    do
    {
      i++;
    } while (i<GPIO_PIN_COUNT && !preference.gpios[i].pin && preference.gpios[i].mode != OUTPUT);
    if (preference.gpios[i].pin && preference.gpios[i].mode == OUTPUT) {
        subArray.add(serialized(generateButtonFormat(preference.gpios[i])));
    }
  }
  String output;
  serializeJson(doc, output);
  return output;
}

String TelegramHandler::generateInlineKeyboardsForActions() {
  const size_t capacity = JSON_ARRAY_SIZE(1+(MAX_ACTIONS_NUMBER/2)) + MAX_ACTIONS_NUMBER*(JSON_OBJECT_SIZE(2)+100);
  DynamicJsonDocument doc(capacity);
  for (int i = 0; i<MAX_ACTIONS_NUMBER; i++) {
    JsonArray subArray = doc.createNestedArray();
    if (preference.actions[i].id) {
        subArray.add(serialized(generateButtonFormat(preference.actions[i])));
    }
    do
    {
      i++;
    } while (i<MAX_ACTIONS_NUMBER && !preference.actions[i].id);
    if (preference.actions[i].id && i<MAX_ACTIONS_NUMBER) {
        subArray.add(serialized(generateButtonFormat(preference.actions[i])));
    }
  }
  String output;
  serializeJson(doc, output);
  return output;
}

void TelegramHandler::handleNewMessages(int numNewMessages) {

  for (int i = 0; i < numNewMessages; i++) {
    bool authorised = false;
    for (int userId: preference.telegram.users) {
      if (userId == atoi(bot->messages[i].from_id.c_str())) {
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
        preference.setGpioState(id);
        bot->sendMessageWithInlineKeyboard(bot->messages[i].chat_id, "Gpios available in output mode", "", generateInlineKeyboardsForGpios(), bot->messages[i].message_id);
      // 'a' is a command for actions
      } else {
        // queue action id, to be picked up by esp32.ino script
        for (int i=0; i<MAX_ACTIONS_NUMBER; i++) {
          if (actionsQueued[i] == 0) {
            actionsQueued[i] = id;
            break;
          }
        }
      }
    } else {
      String chat_id = String(bot->messages[i].chat_id);
      String text = bot->messages[i].text;
      String from_name = bot->messages[i].from_name;
      if (from_name == "") from_name = "Guest";

      if (authorised && text == "/gpios") {
        bot->sendMessageWithInlineKeyboard(chat_id, "Gpios available in output mode", "", generateInlineKeyboardsForGpios());
      } else if (authorised && text == "/actions") {
        bot->sendMessageWithInlineKeyboard(chat_id, "Actions list", "", generateInlineKeyboardsForActions());
      }else if (text == "/start") {
        char welcome[512];
        snprintf(welcome,512,"Welcome to your ESP32 telegram bot, %s.\nHere you'll be able to control your ESP32.\nFirst, add your telegram id to the authorised list (in settings panel) to restrict your bot access:\n %s.\n\n/gpios : returns an inline keyboard to control gpios set in output mode\n/state : returns gpios' state in input mode\n/actions : returns an inline keyboard to trigger actions\n",from_name,bot->messages[i].from_id);
        bot->sendMessage(chat_id, welcome, "Markdown");
      } else {
        char mes[256];
        snprintf(mes,256,"⛔️You are not an authorised user.\nPlease add your telegram id to the user list in setting panel:\n%s",bot->messages[i].from_id);
        bot->sendMessage(chat_id, mes);
      }

      // We save here the current chat_id in case we want to send message later on
      strcpy(preference.telegram.currentChatId, chat_id.c_str());
      preference.save(PREFERENCES_TELEGRAM);
    }
  }
}

void TelegramHandler::queueMessage(const char* message) {
  if (isInit && preference.telegram.token && preference.telegram.active && preference.telegram.currentChatId) {
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
  } else {
    #ifdef __debug  
      Serial.println("Telegram: could not send message, check chatId, token or active state.");
    #endif
  }
}

