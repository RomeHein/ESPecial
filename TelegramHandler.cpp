#include "TelegramHandler.h"
#include <ArduinoJson.h>
#include "esp_camera.h"

camera_fb_t *fb = NULL;
bool isMoreDataAvailable();
byte *getNextBuffer();
int getNextBufferLen();
bool dataAvailable = false;

void TelegramHandler::begin() {
    if (!isInit && preference.telegram.active && preference.telegram.token) {
      #ifdef __debug  
        Serial.println("[TELEGRAM] init");
      #endif
      bot = new UniversalTelegramBot(preference.telegram.token, client);
      isInit = true;
    }
}
void TelegramHandler::handle()
{
    begin();
    if (isInit && preference.telegram.token && preference.telegram.active)  {
      preference.health.telegram = 1;
      int numNewMessages = bot->getUpdates(bot->last_message_received + 1);
      while(numNewMessages) {
        #ifdef __debug  
          Serial.printf("[TELEGRAM] got %i new messages\n", numNewMessages);
        #endif
        handleNewMessages(numNewMessages);
        numNewMessages = bot->getUpdates(bot->last_message_received + 1);
      }
      // Empty messages queued
      sendQueuedMessages();
    } else if (!isInit || ! preference.telegram.token || !preference.telegram.active) {
      preference.health.telegram = 0;
    }
}

String TelegramHandler::generateButtonFormat(GpioFlash& gpio) {
  const size_t capacity = JSON_OBJECT_SIZE(2) + 200;
  DynamicJsonDocument doc(capacity);
  char text[50];
  snprintf(text, sizeof(text), "%s %s", gpio.state ? "🌙": "🔆", gpio.label);
  doc["text"] = text;
  char callback[50];
  snprintf(callback, sizeof(callback), "g-%i", gpio.pin);
  doc["callback_data"] = callback;
  String output;
  serializeJson(doc, output);
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
  return output;
}

String TelegramHandler::generateInlineKeyboardsForGpios(bool inputMode) {
  const size_t capacity = JSON_ARRAY_SIZE(GPIO_PIN_COUNT) + GPIO_PIN_COUNT*(JSON_OBJECT_SIZE(2) + 200);
  DynamicJsonDocument doc(capacity);
  // Dummy code to create 2 buttons per row in telegram api format
  for (int i = 0; i<GPIO_PIN_COUNT; i++) {
    JsonArray subArray = doc.createNestedArray();
    if (preference.gpios[i].pin && ((preference.gpios[i].mode == OUTPUT && !inputMode)||(preference.gpios[i].mode == INPUT && inputMode))) {
      subArray.add(serialized(generateButtonFormat(preference.gpios[i])));
    }
    do
    {
      i++;
    } while (i<GPIO_PIN_COUNT && ((preference.gpios[i].mode != OUTPUT && !inputMode)||(preference.gpios[i].mode != INPUT && inputMode)));
    if (preference.gpios[i].pin && ((preference.gpios[i].mode == OUTPUT && !inputMode)||(preference.gpios[i].mode == INPUT && inputMode))) {
      subArray.add(serialized(generateButtonFormat(preference.gpios[i])));
    }
  }
  String output;
  serializeJson(doc, output);
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
    if (i<MAX_AUTOMATIONS_NUMBER && preference.automations[i].id) {
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
      strncpy(id_c, cmd+2, strnlen(cmd,MAX_MESSAGE_TEXT_SIZE)-1);
      int id = atoi(id_c);
      #ifdef __debug  
        Serial.printf("[TELEGRAM] command: %c, id: %i\n",cmd[0], id);
      #endif
      Task newTask = {};
      // 'g' is a command for gpios
      if (cmd[0] == 'g') {
        newTask.id = 2;
        strcpy(newTask.label, "set");
        newTask.pin = id;
        newTask.value = -1;
        preference.setGpioState(id, -1);
        bot->sendMessageWithInlineKeyboard(bot->messages[i].chat_id, "Gpios available in output mode", "", generateInlineKeyboardsForGpios(), bot->messages[i].message_id);
      // 'a' is a command for automations
      } else if (cmd[0] == 'a') {
        newTask.id = 1;
        newTask.value = id;
      }
      if (newTask.id) {
        taskCallback(newTask);
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

void TelegramHandler::sendMessage(const char* message, bool withPicture) {
  bool didSentMessage = false;
  // Send message to all registered users
    for (int j=0; j< MAX_TELEGRAM_USERS_NUMBER; j++) {
      // Check if we have a chatId corresponding to a telegram user
      if (preference.telegram.users[j] && preference.telegram.chatIds[j]) {
        #ifdef __debug  
          Serial.printf("[TELEGRAM] sending message: %s\n", message);
        #endif
        if (strcmp(message, "") > 0) {
          bot->sendMessage(String(preference.telegram.chatIds[j]), message);
        }
        if (withPicture) {
          sendPictureFromCameraToChat(preference.telegram.chatIds[j]);
        }
        didSentMessage = true;
      } 
    }
    if (!didSentMessage) {
    #ifdef __debug  
      Serial.println("[TELEGRAM] could not send messages: %s\nReason: no chatids defined, send a message first to the bot before using it.");
    #endif
  }
}

void TelegramHandler::sendPictureFromCameraToChat(int chat_id) {
  // Take Picture with Camera
  fb = esp_camera_fb_get();
  if (!fb)
  {
    Serial.println("Camera capture failed");
    return;
  }
  dataAvailable = true;
  Serial.println("Sending");
  bot->sendPhotoByBinary(String(chat_id), "image/jpeg", fb->len,
                        isMoreDataAvailable, nullptr,
                        getNextBuffer, getNextBufferLen);

  #ifdef __debug  
    Serial.printf("[TELEGRAM] sending picture from camera\n");
  #endif
  esp_camera_fb_return(fb);
}

bool isMoreDataAvailable()
{
  if (dataAvailable) {
    dataAvailable = false;
    return true;
  }
  return false;
}

byte *getNextBuffer()
{
  if (fb) {
    return fb->buf;
  }
  return nullptr;
}

int getNextBufferLen()
{
  if (fb) {
    return fb->len;
  }
  return 0;
}

