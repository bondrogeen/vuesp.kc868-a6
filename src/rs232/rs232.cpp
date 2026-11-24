#include "./rs232.h"

#include "../config/ConfigStore.h"
#include "../device/device.h"
#include "../setup/serialSetup.h"
#include "../tasks/tasks.h"
#include "../webserver/webserver.h"

Rs232 rs232 = {KEY_RS232, CMD_GET, 0, 1, 9600, 8, 0, 1, 0, 0, 0, 0, 0, {}, {}};

static bool uartStarted = false;
static uint8_t rxBuffer[RS232_BUFFER];
static uint16_t rxLen = 0;
static uint32_t lastRxMs = 0;

static uint32_t serialConfig() {
  if (rs232.parity == 1) return (rs232.stopBits == 2) ? SERIAL_8E2 : SERIAL_8E1;
  if (rs232.parity == 2) return (rs232.stopBits == 2) ? SERIAL_8O2 : SERIAL_8O1;
  return (rs232.stopBits == 2) ? SERIAL_8N2 : SERIAL_8N1;
}

static bool validBaud(uint32_t baud) {
  switch (baud) {
    case 1200:
    case 2400:
    case 4800:
    case 9600:
    case 19200:
    case 38400:
    case 57600:
    case 115200:
      return true;
    default:
      return false;
  }
}

static void sanitize() {
  if (!validBaud(rs232.baud)) rs232.baud = 9600;
  if (rs232.parity > 2) rs232.parity = 0;
  if (rs232.stopBits != 1 && rs232.stopBits != 2) rs232.stopBits = 1;
  rs232.dataBits = 8;
  if (rs232.enabled > 1) rs232.enabled = 1;
  rs232.flags &= 0x07;
  rs232.rxLen = 0;
  rs232.txLen = 0;
}

static void saveConfig() {
  rs232.key = KEY_RS232;
  const TConfigStatus status =
      configStore.write("rs232", &rs232, sizeof(rs232), CONFIG_RS232_VERSION);
#if VUESP_LOG_LEVEL >= 1
  if (status != TConfigStatus::Ok) {
    Serial.printf("[config] rs232 write: %d\n", (int)status);
  }
#endif
}

static void startUart() {
  if (uartStarted) Serial2.end();
  // Do not call pinMode() on the UART RX pin after Serial2.begin():
  // it overrides the GPIO matrix route and RX stops working.
  Serial2.begin(rs232.baud, serialConfig(), RS232_RX_PIN, RS232_TX_PIN);
  uartStarted = true;
  rs232.status = 1;
  rxLen = 0;
  lastRxMs = millis();
}

static void stopUart() {
  if (uartStarted) Serial2.end();
  uartStarted = false;
  rs232.status = 0;
  rxLen = 0;
}

static void sendRs232() {
  if (wsClientCount() == 0) return;
  wsSendAll((uint8_t*)&rs232, sizeof(rs232));
}

static uint16_t read16(const uint8_t* data) {
  return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

static uint32_t read32(const uint8_t* data) {
  return (uint32_t)data[0] | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16) |
         ((uint32_t)data[3] << 24);
}

void onWsEventRs232(void* arg, uint8_t* data, size_t len, uint32_t clientId, uint8_t task) {
  if (!arg || !data || task != KEY_RS232) return;
  AwsFrameInfo* info = (AwsFrameInfo*)arg;
  if (info->len != sizeof(rs232) || len < sizeof(rs232)) return;

  const uint8_t command = data[1];

  if (command == CMD_SET) {
    rs232.enabled = data[3] ? 1 : 0;
    rs232.baud = read32(data + 4);
    rs232.dataBits = data[8];
    rs232.parity = data[9];
    rs232.stopBits = data[10];
    rs232.flags = data[11];
  } else if (command == CMD_WRITE) {
    uint16_t txLen = read16(data + 22);
    if (txLen > sizeof(rs232.tx)) txLen = sizeof(rs232.tx);
    rs232.txLen = txLen;
    memcpy(rs232.tx, data + 152, txLen);
  }

  rs232.command = command;
  tasks[KEY_RS232] = 1;
}

// Serial commands on the RS232 port: while a line starts with the VUESP: prefix
// the device answers it on the same port, everything else stays transparent.
static char vuespLine[64];
static uint8_t vuespLen = 0;
static bool vuespCandidate = true;

static void feedVuespCommand(uint8_t value) {
  if (value == '\r' || value == '\n') {
    if (vuespCandidate && vuespLen > 6) {
      vuespLine[vuespLen] = '\0';
      handleVuespCommand(vuespLine, Serial2);
    }
    vuespLen = 0;
    vuespCandidate = true;
    return;
  }

  if (!vuespCandidate) return;
  if (vuespLen >= sizeof(vuespLine) - 1) {
    vuespCandidate = false;
    vuespLen = 0;
    return;
  }

  vuespLine[vuespLen++] = (char)value;
  if (vuespLen <= 6 && strncmp(vuespLine, "VUESP:", vuespLen) != 0) vuespCandidate = false;
}

void setupRs232() {
  const TConfigStatus status =
      configStore.read("rs232", &rs232, sizeof(rs232), CONFIG_RS232_VERSION);
  if (status != TConfigStatus::Ok) {
    rs232.key = KEY_RS232;
    rs232.command = CMD_GET;
    rs232.status = 0;
    rs232.enabled = 1;
    rs232.baud = 9600;
    rs232.dataBits = 8;
    rs232.parity = 0;
    rs232.stopBits = 1;
    rs232.flags = 0;
    rs232.rxCount = 0;
    rs232.txCount = 0;
  }
  sanitize();
  if (rs232.enabled) {
    startUart();
  } else {
    stopUart();
  }
}

void loopRs232(uint32_t now) {
  if (uartStarted) {
    uint8_t chunk = 0;
    while (Serial2.available() > 0 && chunk < 64) {
      const int value = Serial2.read();
      if (value < 0) break;
      feedVuespCommand((uint8_t)value);
      if (rxLen < sizeof(rxBuffer)) {
        rxBuffer[rxLen++] = (uint8_t)value;
      }
      lastRxMs = now;
      chunk++;
    }

    if (rxLen > 0 && (now - lastRxMs >= 50 || rxLen >= sizeof(rxBuffer))) {
      memcpy(rs232.rx, rxBuffer, rxLen);
      rs232.rxLen = rxLen;
      rs232.rxCount += rxLen;
      rxLen = 0;
      rs232.command = CMD_READ;
      sendRs232();
    }
  }

  if (!tasks[KEY_RS232]) return;
  tasks[KEY_RS232] = 0;

  switch (rs232.command) {
    case CMD_SET:
      sanitize();
      saveConfig();
      if (rs232.enabled) {
        startUart();
      } else {
        stopUart();
      }
      break;
    case CMD_RESTORE:
      rs232.enabled = 1;
      rs232.baud = 9600;
      rs232.parity = 0;
      rs232.stopBits = 1;
      rs232.flags = 0;
      sanitize();
      saveConfig();
      startUart();
      break;
    case CMD_WRITE:
      if (uartStarted && rs232.txLen > 0) {
        const uint16_t length = rs232.txLen;
        Serial2.write(rs232.tx, length);
        Serial2.flush();
        rs232.txCount += length;
      }
      rs232.txLen = 0;
      break;
    default:
      break;
  }

  rs232.status = uartStarted ? 1 : 0;
  sendRs232();
}
