# vuesp.kc868-a6

KC868-A6: 6 входов и 6 выходов.

Проект собран на базе [Vuesp](https://github.com/bondrogeen/vuesp) (шаблон `5f4e625`): прошивка **5.0.14** PlatformIO + web UI **4.1.2** на Vue 3 и `vuesp-components@5.2.1`.

## Документация

Подробное описание платы, распиновка, схема, ScriptRunner и ограничения — в [DOCS.md](./DOCS.md).

## Особенности

- 6 входов и 6 реле через PCF8574 по I2C (`0x22`, `0x24`).
- RTC DS3231 (`0x68`), 4 ADC и 2 DAC, RS485 (MAX13487EESA), RS232 (SP3232EEN) с веб-терминалом, SPI-интерфейс LoRa SX1278/nRF24L01.
- Функции ScriptRunner `input(pin)`, `output(pin, value)`, `var()` и `modbus()`.
- Modbus RTU slave/master на RS485 (MAX13487EESA, TX27/RX14, без DE).
- Настройки устройства и портов — в `ConfigStore` (`/tmp/config.bin`), протокол — `src/const/protocol.h`.
- Диагностика загрузки и loop-метрик, текущий шаблон web UI.

## Плата и окружение

- PlatformIO env: `esp32`
- Прошивка: `platformio.ini`
- Web UI: `vue/`
- Файловая система: LittleFS, веб-сборка попадает в `data/www`

## Сборка

```bash
cd vue
npm ci
npm run protocol
npm run build-web
```

Полная сборка (web + прошивка):

```bash
cd vue
npm run build
```

Либо отдельно:

```bash
~/.platformio/penv/bin/pio run
~/.platformio/penv/bin/pio run -t buildfs
```

## Возможности

- локальный веб-интерфейс без облака;
- настройка Wi-Fi, безопасности, GPIO и файловой системы;
- dashboard с плитками устройств;
- сценарии и автоматизация;
- обновление прошивки и файловой системы через веб-интерфейс.

## CI

GitHub Actions собирает web UI, прошивку и LittleFS для указанных env, запускает тесты ScriptRunner и публикует артефакты в релиз при пуше тега.

## Репозиторий

<https://github.com/bondrogeen/vuesp.kc868-a6>
