#include <WiFi.h>
#include <ESPmDNS.h>
#include "time.h"
#include "PreferenceHandler.h"
#include "TaskManager.h"

TaskManager *taskManager;
PreferenceHandler *preferencehandler;


// Notify client esp has just restarted
bool hasJustRestarted = true;

// Debounce inputs delay
long lastDebouncedInputTime = 0;
// Keep tracks of last time for each automation run 
long lastDebounceTimes[MAX_AUTOMATIONS_NUMBER] = {};
// To trigger event based on time, we check all automatisations everyminutes
int lastCheckedTime = 0;
int debounceTimeDelay = DEBOUNCE_TIME_DELAY;
// To trigget async events to the web interface. Avoid server spaming
int lastEvent = 0;

String systemInfos() {
  char infos[256];
  snprintf(infos,256,"\nFree memory: %i\nTelegram bot: %i\nMqtt server: %i",ESP.getFreeHeap(),preferencehandler->health.telegram,preferencehandler->health.mqtt);
  return String(infos);
}

void execOTA(const char* version) {
  const String bin = version+String("/espinstall.ino.bin");
  const String spiffs = version+String("/spiffs.bin");
  const String binPath = String(REPO_PATH)+bin;
  const String spiffsPath = String(REPO_PATH)+spiffs;
  #ifdef __debug
    Serial.printf("[OTA] start SPIFFS download from: %s\n", spiffsPath.c_str());
  #endif
  t_httpUpdate_return ret = httpUpdate.updateSpiffs(client, spiffsPath);
  switch (ret) {
    case HTTP_UPDATE_FAILED:
      Serial.printf("[OTA] Error (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
      if (serverhandler->events.count()>0) {
        serverhandler->events.send(httpUpdate.getLastErrorString().c_str(),"firmwareUpdateError",millis());
      }
      return;

    case HTTP_UPDATE_NO_UPDATES:
      Serial.println(F("[OTA] no updates"));
      if (serverhandler->events.count()>0) {
        serverhandler->events.send("No updates","firmwareUpdateError",millis());
      }
      return;

    case HTTP_UPDATE_OK:
      #ifdef __debug
        Serial.println(F("[OTA] SPIFFS updated"));
      #endif
      Serial.println(F("HTTP_UPDATE_OK"));
      break;
  }
  #ifdef __debug
    Serial.printf("[OTA] start BIN download from: %s\n", binPath.c_str());
  #endif
  ret = httpUpdate.update(client, binPath);
  switch (ret) {
    case HTTP_UPDATE_FAILED:
      Serial.printf("[OTA] Error (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
      if (serverhandler->events.count()>0) {
        serverhandler->events.send(httpUpdate.getLastErrorString().c_str(),"firmwareUpdateError",millis());
      }
      break;

    case HTTP_UPDATE_NO_UPDATES:
      Serial.println(F("[OTA] no updates"));
      if (serverhandler->events.count()>0) {
        serverhandler->events.send("No updates","firmwareUpdateError",millis());
      }
      break;

    case HTTP_UPDATE_OK:
      #ifdef __debug
        Serial.println(F("[OTA] BIN updated"));
      #endif
      ESP.restart();
      break;
  }
}

bool checkAgainstLocalHour(int time, int signType) {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    #ifdef __debug
      Serial.println(F("[MAIN] Failed to obtain time"));
    #endif
    return false;
  }
  int hour = time/100;
  int minutes = time % 100;
  #ifdef __debug
    Serial.printf("[MAIN] Checking hour %i:%i against local time:",hour,minutes);
    Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
  #endif
  if (signType == 1) {
    return timeinfo.tm_hour == hour && minutes == timeinfo.tm_min;
  } else if (signType == 2) {
    return timeinfo.tm_hour != hour && minutes != timeinfo.tm_min;
  } else if (signType == 3) {
    return timeinfo.tm_hour > hour || (timeinfo.tm_hour == hour && timeinfo.tm_min > minutes);
  } else if (signType == 4) {
    return timeinfo.tm_hour < hour || (timeinfo.tm_hour == hour && timeinfo.tm_min < minutes);
  }
}

bool checkAgainstLocalWeekDay(int weekday, int signType) {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println(F("Failed to obtain time"));
    return false;
  }
  if (signType == 1) {
    return timeinfo.tm_wday == weekday;
  } else if (signType == 2) {
    return timeinfo.tm_wday != weekday;
  } else if (signType == 3) {
    return timeinfo.tm_wday > weekday;
  } else if (signType == 4) {
    return timeinfo.tm_wday < weekday;
  }
}

void h4setup() {

  taskManager = new TaskManager();
  taskManager->begin();

  preferencehandler = new PreferenceHandler();
  preferencehandler->begin();

  if (preferencehandler->wifi.staEnable && preferencehandler->wifi.staSsid) {
    #ifdef __debug
      Serial.printf("[WIFI] Station mode detected. SSID: %s PSW: %s\n",preferencehandler->wifi.staSsid, preferencehandler->wifi.staPsw);
    #endif
    WiFi.mode(WIFI_STA);
    WiFi.begin(preferencehandler->wifi.staSsid, preferencehandler->wifi.staPsw);
    int connectionAttempt = 0;
    while (WiFi.status() != WL_CONNECTED && connectionAttempt<20) {
        delay(500);
        #ifdef __debug
          Serial.println("[WIFI] .");
        #endif
        connectionAttempt++;
    }
  }

  if (WiFi.status() != WL_CONNECTED) {
    WiFi.mode(WIFI_AP);
    #ifdef __debug
      Serial.println("[WIFI] Failed to connect in station mode. Starting access point");
    #endif
    WiFi.softAP(preferencehandler->wifi.apSsid, preferencehandler->wifi.apPsw);
    Serial.print("[WIFI] AP IP address: ");
    Serial.println(WiFi.softAPIP());
  } else {
    #ifdef __debug
      Serial.print("[WIFI] connected. ");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
    #endif
    //init and get the time
    configTime(gmtOffset_sec, daylightOffset_sec, NTP_SERVER);
    // Get local time
    struct tm timeinfo;
    if(!getLocalTime(&timeinfo)){
      Serial.println(F("[SETUP] Failed to obtain time"));
    } else {
      // This avoid having to check getLocalTime in the loop, saving some complexity.
      debounceTimeDelay -= timeinfo.tm_sec; 
      lastCheckedTime = millis();
    }
  }

  // Configure DNS
  if (MDNS.begin(preferencehandler->wifi.dns)) {
    #ifdef __debug
      Serial.printf("[SETUP] Web interface available on: %s or ",preferencehandler->wifi.dns);
    #endif
  }
}

void loopOld(void) {
  // Reload firmware list
  if (serverhandler->shouldReloadFirmwareList && serverhandler->events.count()>0) {
    serverhandler->shouldReloadFirmwareList = false;
    getFirmwareList();
  }
  // Check restart flag from server
  if (serverhandler->shouldRestart) {
    serverhandler->shouldRestart = false;
    ESP.restart();
  }
  // Check if OTA has been requested from server
  if (strcmp(serverhandler->shouldOTAFirmwareVersion,"")>0) {
    // Reset shouldOTAFirmwareVersion to avoid multiple OTA from main loop
    #ifdef __debug
      Serial.printf("[OTA] version %s detected\n", serverhandler->shouldOTAFirmwareVersion);
    #endif
    char version[10];
    strcpy(version,serverhandler->shouldOTAFirmwareVersion);
    strcpy(serverhandler->shouldOTAFirmwareVersion,"");
    execOTA(version);
  }
  // Notify clients esp has restarted
  if (hasJustRestarted && serverhandler->events.count()>0) {
    serverhandler->events.send("rebooted","shouldRefresh",millis());
    hasJustRestarted = false;
  }
  // The following handlers can only work in STA mode
  if (WiFi.status() ==  WL_CONNECTED) {
    mqtthandler->handle();
    telegramhandler->handle();
  } else if (WiFi.getMode() != WIFI_AP) {
    // wifi down, reconnect here
    WiFi.begin();
    int count = 0;
    while (WiFi.status() != WL_CONNECTED && count < 200 ) 
    {
      delay(500);
      ++count;
      #ifdef __debug
        Serial.printf("[MAIN_LOOP] Wifi deconnected: attempt %i/200\n", count);
      #endif
      if (count == 200) {
        Serial.println(F("[MAIN_LOOP] Failed to reconnect, restarting now."));
        ESP.restart();
      }
    }
  }
}

void automation_loop(void *pvParameters) {
  while(1) {
    readPins();
    pickUpQueuedAutomations();
    if (millis() > debounceTimeDelay + lastCheckedTime) {
      // If this is the first time this run, we are now sure it will run every minutes, so set back debounceTimeDelay to 60s.
      if (debounceTimeDelay != 60000) {
        debounceTimeDelay = 60000;
      }
      // Run only time scheduled automations
      #ifdef __debug
        Serial.println(F("[AUTO_LOOP] Checking time scheduled automations"));
        Serial.println(systemInfos());
      #endif
      runTriggeredEventAutomations(true);
      lastCheckedTime = millis();
    }
    vTaskDelay(10);
  }
}
