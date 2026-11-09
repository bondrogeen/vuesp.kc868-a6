# vuesp.kc868-a6

KC868-A6: 6 входов и 6 выходов.

Проект собран на базе шаблона [Vuesp](https://github.com/bondrogeen/vuesp): прошивка PlatformIO + web UI на Vue 3 и `vuesp-components`.

## Документация

Подробное описание платы, распиновка, схема, ScriptRunner и ограничения — в [DOCS.md](./DOCS.md).

## Особенности

- 6 входов и 6 реле через PCF8574 по I2C (`0x22`, `0x24`).
- RTC DS3231 (`0x68`), 4 ADC и 2 DAC.
- Функции ScriptRunner `input(pin)` и `output(pin, value)`.

## Плата и окружение

- PlatformIO env: `esp32`
- Прошивка: `platformio.ini`
- Web UI: `vue/`
- Файловая система: LittleFS, веб-сборка попадает в `data/www`

## Сборка

```bash
cd vue
npm ci
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
