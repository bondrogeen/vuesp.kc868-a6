#include "./device.h"

#include "../config/ConfigStore.h"

UnixTime stamp(0);

Buffer myBuffer = {KEY_BUFFER};

Device device = {KEY_DEVICE, 0, 0, 255, 100, 1760640900, 2, "text"};

uint32_t lastTimeDevice = 0;
uint32_t lastLoopInput = 0;
uint32_t deviceInputOld = 0;

struct PortDevice {
  uint8_t isPressed;
  uint8_t count;
  uint8_t value;
  uint8_t valueOld;
  uint32_t pressStart;
};

PortDevice portsDevice[PORTS_LEN] = {
    {0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0},
};

void onWsEventDevice(void* arg, uint8_t* data, size_t len, uint32_t clientId, uint8_t task) {
  AwsFrameInfo* info = (AwsFrameInfo*)arg;
  if (task == KEY_DEVICE && info->len == sizeof(device)) {
    memcpy(&device, data, sizeof(device));
  }
}

void onSendDevice() {
  wsSendAll((uint8_t*)&device, sizeof(device));
}

void getInput() {
  Wire.requestFrom(ADDRESS_INPUT, 1);
  while (Wire.available()) {
    device.input = Wire.read();
  }
}

void getOutput() {
  Wire.requestFrom(ADDRESS_OUTPUT, 1);
  while (Wire.available()) {
    device.output = Wire.read();
  }
}

void setOutput() {
  Wire.beginTransmission(ADDRESS_OUTPUT);
  Wire.write(device.output);
  Wire.endTransmission();
}

uint8_t bcdToDec(uint8_t val) {
  return ((val / 0x10) * 10) + (val % 0x10);
}

uint8_t decToBcd(uint8_t val) {
  return ((val / 10) * 0x10) + (val % 10);
}

void setDate(uint32_t unixTime) {
  stamp.getDateTime(unixTime);
  Wire.beginTransmission(ADDRESS_RTC);
  Wire.write(0);
  Wire.write(decToBcd(stamp.second));
  Wire.write(decToBcd(stamp.minute));
  Wire.write(decToBcd(stamp.hour));
  Wire.write(decToBcd(stamp.dayOfWeek));
  Wire.write(decToBcd(stamp.day));
  Wire.write(decToBcd(stamp.month));
  Wire.write(decToBcd(stamp.year - 2000));
  Wire.endTransmission(true);
}

uint32_t getDate() {
  Wire.beginTransmission(ADDRESS_RTC);
  Wire.write(0);
  Wire.endTransmission(true);

  Wire.requestFrom(ADDRESS_RTC, 7);
  if (Wire.available() < 7) return device.now;

  const uint8_t second = bcdToDec(Wire.read());
  const uint8_t minute = bcdToDec(Wire.read());
  const uint8_t hour = bcdToDec(Wire.read() & 0b111111);
  const uint8_t weekDay = bcdToDec(Wire.read());
  const uint8_t monthDay = bcdToDec(Wire.read());
  const uint8_t month = bcdToDec(Wire.read());
  const uint8_t year = bcdToDec(Wire.read());

  (void)weekDay;
  stamp.setDateTime(2000 + year, month, monthDay, hour, minute, second);
  device.now = stamp.getUnix();
  return device.now;
}

void getGPIO() {
  getInput();
  onSendDevice();
}

void getData() {
  getInput();
  getOutput();
  getDate();
}

void loopInput(uint32_t now) {
  for (int i = 0; i < PORTS_LEN; i++) {
    const uint32_t time = now - portsDevice[i].pressStart;

    if (time > REPEAT_START_TIME) {
      if (portsDevice[i].isPressed && portsDevice[i].count == 1) {
        scriptRunner.emitEvent("port_r", 2, i, portsDevice[i].value);
      } else {
        portsDevice[i].count = 0;
      }
    }
    if (time > LONG_PRESS_TIME && time < REPEAT_START_TIME) {
      if (!portsDevice[i].isPressed && portsDevice[i].count == 1) {
        scriptRunner.emitEvent("port_l", 2, i, portsDevice[i].value);
        portsDevice[i].count = 0;
      }
    }
    if (time > CLICK_WINDOW) {
      if (!portsDevice[i].isPressed && portsDevice[i].count) {
        scriptRunner.emitEvent("port_c", 3, i, portsDevice[i].value, portsDevice[i].count);
        portsDevice[i].count = 0;
      }
    }
  }
}

void findPort() {
  for (uint8_t i = 0; i < PORTS_LEN; i++) {
    const uint8_t valueNew = (device.input & (1 << i)) != 0;
    if (portsDevice[i].value != valueNew) {
      portsDevice[i].isPressed = portsDevice[i].valueOld != valueNew;
      if (portsDevice[i].isPressed) {
        portsDevice[i].count++;
        portsDevice[i].pressStart = millis();
      }
      portsDevice[i].value = valueNew;
    }
  }
}

// GPIO13 is the board's interrupt input; it mirrors PCF8574 inputs.
void deviceGPIO(Port* port, uint8_t type) {
  (void)type;
  if (port->gpio == GPIO_IO13 && port->value == 0) {
    getInput();
    findPort();
    onSendDevice();
  }
}

static char displayBuffer[65] = "5";

bool dataProvider(const char* id, DataKind kind, DataValue& value, bool write) {
#if VUESP_LOG_LEVEL >= 3
  Serial.print(id);
  Serial.print(write ? "WRITE" : "READ");
  Serial.print(" kind=");
  Serial.println(kind);
#endif

  if (strcmp(id, "$display") == 0) {
    if (write) {
      if (kind == KIND_STRING) {
        char buf[65];
        uint8_t len = value.stringVal.len;
        if (len > 64) len = 64;
        strncpy(buf, (char*)value.stringVal.data, len);
        buf[len] = '\0';
        strcpy(displayBuffer, buf);
#if VUESP_LOG_LEVEL >= 3
        Serial.print("DISPLAY: ");
        Serial.println(buf);
#endif
      } else if (kind == KIND_INT) {
        snprintf(displayBuffer, sizeof(displayBuffer), "%d", value.intVal);
      } else if (kind == KIND_UINT) {
        snprintf(displayBuffer, sizeof(displayBuffer), "%u", value.uintVal);
      } else if (kind == KIND_FLOAT) {
        snprintf(displayBuffer, sizeof(displayBuffer), "%.2f", value.floatVal);
      }
      return true;
    }

    if (kind == KIND_STRING) {
      value.stringVal.data = (uint8_t*)displayBuffer;
      value.stringVal.len = strlen(displayBuffer);
    } else if (kind == KIND_INT) {
      value.intVal = atoi(displayBuffer);
    } else if (kind == KIND_UINT) {
      value.uintVal = (uint32_t)atoi(displayBuffer);
    } else if (kind == KIND_FLOAT) {
      value.floatVal = atof(displayBuffer);
    }
    return true;
  }
  return false;
}

bool httpHandler(uint8_t paramCount, const Value* params, Value& result, void* userData) {
  if (paramCount < 2) return false;

  result.type = VAL_INT;
  result.intVal = 200;
  return true;
}

bool inputHandler(uint8_t paramCount, const Value* params, Value& result, void* userData) {
  if (paramCount != 1) return false;
  const uint32_t pin = params[0].uintVal;
  if (pin >= 1 && pin < 7) {
    result.type = VAL_UINT;
    result.intVal = bitRead(device.input, pin - 1);
    return true;
  }
  return false;
}

bool outputHandler(uint8_t paramCount, const Value* params, Value& result, void* userData) {
  if (paramCount < 1) return false;
  const uint32_t pin = params[0].uintVal;
  if (pin >= 6) return false;

  if (paramCount >= 2) {
    const uint32_t value = params[1].uintVal;
    if (value) {
      bitSet(device.output, pin);
    } else {
      bitClear(device.output, pin);
    }
    setOutput();
    onSendDevice();
  }

  result.type = VAL_UINT;
  result.intVal = bitRead(device.output, pin);
  return true;
}

void setupDevice() {
  scriptRunner.setDataProvider(dataProvider);
  scriptRunner.registerFunction("http", httpHandler);
  scriptRunner.registerFunction("input", inputHandler);
  scriptRunner.registerFunction("output", outputHandler);

  Wire.begin(GPIO_SDA, GPIO_SCL);
  setOutput();
  getDate();
  getOutput();
  getGPIO();

  for (uint8_t i = 0; i < PORTS_LEN; i++) {
    const uint8_t valueNew = (device.input & (1 << i)) != 0;
    portsDevice[i].value = valueNew;
    portsDevice[i].valueOld = valueNew;
    portsDevice[i].count = 0;
    portsDevice[i].isPressed = 0;
  }
}

void setupFirstDevice() {
  const TConfigStatus status =
      configStore.read("device", &device, sizeof(device), CONFIG_DEVICE_VERSION);
  if (status == TConfigStatus::Ok) {
    device.command = 0;
  }
#if VUESP_LOG_LEVEL >= 1
  else if (status != TConfigStatus::NotFound) {
    Serial.printf("[config] device read: %d\n", (int)status);
  }
#endif
}

void loopDevice(uint32_t now) {
  if (now - lastLoopInput > REPEAT_INTERVAL) {
    lastLoopInput = now;
    loopInput(now);
  }

  if (now - lastTimeDevice > 10000) {
    lastTimeDevice = now;
    getData();
    onSendDevice();
  }

  if (tasks[KEY_BUFFER]) {
    wsSendAll((uint8_t*)&myBuffer, sizeof(myBuffer));
    tasks[KEY_BUFFER] = 0;
  }

  if (tasks[KEY_DEVICE]) {
    if (device.command == 1) {
      setDate(device.now);
    } else if (device.command == 2) {
      setOutput();
    } else if (device.command == 4 || device.command == CMD_SAVE) {
      const TConfigStatus status =
          configStore.write("device", &device, sizeof(device), CONFIG_DEVICE_VERSION);
#if VUESP_LOG_LEVEL >= 1
      if (status != TConfigStatus::Ok) {
        Serial.printf("[config] device write: %d\n", (int)status);
      }
#endif
    } else {
      getData();
    }

    device.command = 0;
    tasks[KEY_DEVICE] = 0;
    onSendDevice();
  }
}
