#pragma once

#ifndef Parameters_h
#define Parameters_h

#define __debug
#define FIRMWARE_VERSION 0.8

// Debounce Times
#define DEBOUNCE_INPUT_DELAY 20 //
#define DEBOUNCE_TIME_DELAY 60000 //
#define DEBOUNCE_EVENT_DELAY 400 //
#define TIMEOUT_WIFI_CONNECTION 5000
#define TOUCH_TIME_INTERVAL 80 // 
//------------------ Main.ino
#define MAX_QUEUED_AUTOMATIONS 10
// Repo info for updates
#define REPO_PATH "https://raw.githubusercontent.com/RomeHein/ESPecial/master/versions/"
#define NTP_SERVER "pool.ntp.org"
#define gmtOffset_sec 3600
#define daylightOffset_sec 3600

//------------------ PreferenceHandler
#define PREFERENCES_NAME "esp32-api"
#define PREFERENCES_GPIOS "gpios"
#define PREFERENCES_I2C_SLAVE "i2c-slaves"
#define PREFERENCES_AUTOMATION "automation"
#define PREFERENCES_MQTT "mqtt"
#define PREFERENCES_TELEGRAM "telegram"
#define PREFERENCES_WIFI "wifi"
#define PREFERENCES_CAMERA "camera"

#define CHANNEL_NOT_ATTACHED -1
#define PIN_NOT_ATTACHED -1
#define MAX_I2C_SLAVES 10 // Maximum number of I2c slaves in overall
#define MAX_DIGITALS_CHANNEL 16 // Maximum channel number for analog pins
// Yes only 20 to limit memeory usage
#define MAX_AUTOMATIONS_NUMBER 10 // Maximum automations number that can be set in the system
#define MAX_AUTOMATIONS_CONDITIONS_NUMBER 5 // Maximum number of conditions in a given automation
#define MAX_AUTOMATION_ACTION_NUMBER 5 // Maximum number of actions in a given automation
#define MAX_TELEGRAM_USERS_NUMBER 10 // Maximum user number that can user telegram bot

// Max text sizes
#define MAX_LABEL_TEXT_SIZE 50
#define MAX_I2C_COMMAND_NUMBER 10 // Maximum number of alphanumeric commands per write session in I2C
#define MAX_MESSAGE_TEXT_SIZE 100 // Usually used when we want to display a text message, or send a text to telegram for instance

#define TOUCH_VARIATION_ALLOWED 3 // This define the variation needed by an I/O reading before triggering an event in the main loop. This is used for TouchRead.
//------------------ ServerHandler
#define idParamName "id"
#define pinParamName "pin"
#define valueParamName "value"
#define modelParamName "model"

//------------------ MQTT Parameters
#define RETRY_ATTEMPT 5
#define TOPIC_MAX_SIZE 256

//------------------ telegram
#define MAX_QUEUED_MESSAGE_NUMBER 5
#endif