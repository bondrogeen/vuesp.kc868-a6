#include "./serialSetup.h"

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#elif defined(ESP32)
#include <WiFi.h>
#endif

#include <string.h>

#include "../eeprom/eeprom.h"
#include "../init/init.h"
#include "../utils/text.h"

static char serialLine[201];
static uint16_t serialLen = 0;

static void printWifiScan(Print& out) {
  out.println(F("@VUESP:SCAN:BEGIN"));

  auto oldMode = WiFi.getMode();
  WiFi.mode(WIFI_AP_STA);

  int count = WiFi.scanNetworks();
  if (count < 0) {
    out.println(F("@VUESP:SCAN:ERROR"));
  } else {
    for (int i = 0; i < count; i++) {
      char ssid[33];
      WiFi.SSID(i).toCharArray(ssid, sizeof(ssid));
      textSanitizeSsid(ssid);
      int secure = WiFi.encryptionType(i) != 0 ? 1 : 0;
      out.printf("@VUESP:WIFI:%s\t%d\t%d\n", ssid, WiFi.RSSI(i), secure);
    }
  }

  WiFi.scanDelete();
  WiFi.mode(oldMode);
  out.println(F("@VUESP:SCAN:END"));
}

static void setWifi(const char* command, Print& out) {
  const char* first = strchr(command, '\t');
  if (!first) {
    out.println(F("@VUESP:SET:ERROR"));
    return;
  }

  const char* second = strchr(first + 1, '\t');
  const char* commandEnd = command + strlen(command);

  char ssid[32];
  char pass[32];
  if (!textCopyTrimmed(ssid, sizeof(ssid), first + 1, second ? second : commandEnd) ||
      !textCopyRaw(pass, sizeof(pass), second ? second + 1 : commandEnd, commandEnd) || !ssid[0]) {
    out.println(F("@VUESP:SET:ERROR"));
    return;
  }

  memset(settings.wifiSsid, 0, sizeof(settings.wifiSsid));
  memset(settings.wifiPass, 0, sizeof(settings.wifiPass));
  strncpy(settings.wifiSsid, ssid, sizeof(settings.wifiSsid) - 1);
  strncpy(settings.wifiPass, pass, sizeof(settings.wifiPass) - 1);
  settings.wifiMode = WIFI_STA;
  settings.wifiDhcp = 1;

  if (!saveSettings(settings)) {
    out.println(F("@VUESP:SET:ERROR"));
    return;
  }

  out.println(F("@VUESP:SET:OK"));
  delay(250);
  reboot();
}

void handleVuespCommand(char* command, Print& out) {
  textTrim(command);
  if (strncmp(command, "VUESP:", 6) != 0) return;

  if (strcmp(command, "VUESP:PING") == 0) {
    out.println(F("@VUESP:PONG"));
  } else if (strcmp(command, "VUESP:INFO") == 0) {
    out.printf("@VUESP:INFO:%s,%u,%08X,%s,%s\n", DEF_DEVICE_NAME, (unsigned)infoFS.board, (unsigned)infoFS.id,
          WiFi.localIP().toString().c_str(), WiFi.softAPIP().toString().c_str());
  } else if (strcmp(command, "VUESP:SCAN") == 0) {
    printWifiScan(out);
  } else if (strncmp(command, "VUESP:SET\t", 10) == 0) {
    setWifi(command, out);
  }
}

void loopSerialSetup() {
  while (Serial.available() > 0) {
    char value = static_cast<char>(Serial.read());
    if (value == '\n' || value == '\r') {
      if (serialLen) {
        serialLine[serialLen] = '\0';
        handleVuespCommand(serialLine, Serial);
        serialLen = 0;
      }
    } else if (serialLen < sizeof(serialLine) - 1) {
      serialLine[serialLen++] = value;
    }
  }
}
