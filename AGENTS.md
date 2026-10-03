# AGENTS.md

Правила, контекст и гайдлайны для AI-агентов, работающих с этим репозиторием.
Документ самодостаточен: агент должен выполнить задачу, прочитав только его и код.

## Project

XiaoZhi — прошивка голосового ассистента на ESP-IDF (C/C++), поддерживающая множество чипов, плат, дисплеев, аудиоустройств и сетевых транспортов. При сборке выбирается ровно одна реализация платы.

Репозиторий — форк `ZDarow/xiaozhi-esp32`, адаптированный под Центральную Россию (эндпоинты, поддержка, зеркала). Текущая ветка: `feature/russia-adaptation`.

Рекомендуется ESP-IDF v6.1. Минимальная поддерживаемая версия SDK — ESP-IDF v6.0.1. IDF 5.x не поддерживается.

## Technology Stack

| Слой | Технология | Детали |
|------|-----------|--------|
| Язык | C++23 / C | `main/` компилируется с `-std=gnu++23`; исключения и RTTI выключены (`sdkconfig.defaults`) |
| SDK | ESP-IDF >= 6.0.1 | CMake + Kconfig; `MINIMAL_BUILD ON`; `PROJECT_VER 2.5.0` |
| Сборка | CMake 3.16+, `scripts/build.py` | Каноническая точка входа; 145 плат, 177 вариантов, 6 чип-таргетов |
| RTOS | FreeRTOS | Задачи приложения, аудио-пула, сетевые колбэки вне main-task |
| UI | LVGL 9.5 + `esp_lvgl_port` | LCD/OLED/эмодзи/видео; бездисплейные варианты |
| Аудио | Opus (`esp_codec_dev`, `esp_audio_codec`, `esp_audio_effects`) | AFE-конвейер, AEC, VAD, wake word |
| Wake word | ESP-SR 2.4.7 | `esp-sr` + модели `wn*_nihaoxiaozhi` |
| Крипто | mbedTLS / PSA Crypto | MQTT-шифрование, BluFi, TLS; исключения C++ выключены |
| Транспорт | WebSocket, MQTT+UDP | Общий API в `Protocol`, две реализации |
| Туллинг | Python 3 (`unittest`), `clang-format` 18 (Google, ColumnLimit 100) | Хост-тесты в `scripts/tests`, 81 тест |
| CI | GitHub Actions, контейнер `espressif/idf:v6.1` | `yamllint`, `clang-format-diff`, матрица вариантов |
| Docker | `docker/firmware-builder` | Образ на `espressif/idf:release-v6.1` |

Чип-таргеты: `esp32`, `esp32c3`, `esp32c5`, `esp32c6`, `esp32s3`, `esp32p4`.
ESP32-P4 и ESP32-S31 требуют IDF 6.1+.

## Environment

```sh
# Локальный IDF (проверено 03.10.2026)
source /home/mi/.espressif/v6.0.2/esp-idf/export.sh
idf.py --version        # ожидается 6.0.2; CI использует 6.1

# Путь, зашитый в .vscode/settings.json
idf.currentSetup = /home/mi/.espressif/v6.0.2/esp-idf
```

Локальная версия IDF (6.0.2) ниже CI (6.1). Сборка P4/S31 локально невозможна — проверяй такие варианты только в CI или в docker-образе.

Внешние каталоги (`build/`, `managed_components/`, `components/`, `sdkconfig`, `dependencies.lock`, `releases/`) генерируются и не коммитятся.

## Architecture

- `main/application.*` — главный цикл событий, жизненный цикл протокола и высокоуровневое поведение.
- `main/device_state_machine.*` — легальные переходы состояний во время выполнения.
- `main/boards/common/` — интерфейсы плат и reusable-помощники железа/сети.
- `main/boards/**/` — пины, инициализация и варианты сборки конкретных плат.
- `main/audio/` — кодеки, аудио-задачи, движки, wake words и очереди.
- `main/protocols/` — транспорт-нейтральный API плюс WebSocket и MQTT/UDP.
- `main/display/` и `main/led/` — reusable-реализации UI.
- `main/mcp_server.*` — общие MCP-инструменты на устройстве и диспетчер.
- `main/ota.*` — OTA: проверка версии, загрузка, активация, опциональная подпись.
- `main/settings.*` — доступ к NVS. Ключи NVS — постоянный публичный API.
- `main/gost_crypto/` — **в этой ветке отсутствует** (прототип ГОСТ вынесен в `feature/gost-crypto`, см. «Известные риски»).
- `main/Kconfig.projbuild` — конфигурация плат и функций.
- `main/CMakeLists.txt` — выбор источников, локалей, шрифтов и ассетов (1425 строк, глобальные GLOB по 300 `config.json`).
- `scripts/build.py` — каноническая точка входа сборки платы/варианта.
- `scripts/build_default_assets.py` — сборка `assets.bin` (лимит раздела проверяется в тестах).
- `main/idf_component.yml` — 60+ управляемых компонентов с `rules` по таргетам.

Перед добавлением чего-либо читай ближайшую существующую реализацию. Предпочитай узкий owning слой; не клади поведение конкретной платы в ядро.

## Commands

```sh
# Метаданные матрицы сборки
python3 scripts/build.py --list-boards
python3 scripts/build.py --list-boards --json | python3 -c "import json,sys;print(len(json.load(sys.stdin)))"
python3 scripts/build.py --list-languages
python3 scripts/build.py --list-wake-words

# Каноническая сборка варианта (меняет локальные sdkconfig и каталог сборки)
python3 scripts/build.py main/boards/espressif/esp32s3 --name esp32s3
python3 scripts/build.py main/boards/bread-compact-wifi-lcd --name bread-compact-wifi

# Хост-тесты (81 тест, ~7 c)
python3 -m unittest discover -s scripts/tests

# Форматирование только тронутых файлов
clang-format -i main/ota.cc
clang-format --dry-run -Werror main/ota.cc

# Линт CI-конфигурации
pipx run yamllint .github/workflows/*.yml
```

Полезные флаги `build.py`: `--language LOCALE`, `--wake-word MODEL`, `--build-options-json JSON`, `--zip`, `--select-changed` (читает список изменённых путей из stdin и печатает затронутые варианты — этим пользуется CI).

Docker-сборщик (воспроизводимый путь, когда локальный IDF старее нужного):

```sh
docker build -t xiaozhi-builder -f docker/firmware-builder/Dockerfile .
docker run --rm -v "$PWD/out:/output" xiaozhi-builder main/boards/espressif/esp32s3 --name esp32s3
```

Отключение нерелевантных расширений VS Code в области этой рабочей папки
(пишет ключ `extensionsIdentifiers/disabled` в `workspaceStorage`, поэтому применимо
только при закрытом редакторе; подробности — в `docs/agent-workspace-setup.md`):

```sh
python3 scripts/dev/vscode_disable_extensions.py --status
python3 scripts/dev/vscode_disable_extensions.py --apply
python3 scripts/dev/vscode_disable_extensions.py --undo
```

## Dependencies

Управляемые через `main/idf_component.yml` компоненты (сокращённо, полный список — в файле):

- **Дисплеи:** `esp_lcd_sh8601`, `esp_lcd_co5300`, `esp_lcd_ili9341`, `esp_lcd_gc9a01`, `esp_lcd_st77916/7796/77922`, `esp_lcd_axs15231b`, `esp_lcd_st7701` (S3/P4), `esp_lcd_spd2010`, `esp_lcd_jd9365`, `esp_lcd_hx8394`, `esp_lcd_st7703`, `esp_lcd_ili9881c`, `esp_lcd_ek79007`, `esp_lcd_st7102`, `esp_lcd_nv3023` (78).
- **Тач и I/O:** `esp_lcd_touch_ft5x06/gt911/gt1151/cst816s/st7123`, `esp_lcd_touch_cst9217`, `esp_lcd_panel_io_additions`, `esp_io_expander_tca9554`, `esp_io_expander_tca95xx_16bit`, `custom_io_expander_ch32v003`, `m5ioe1`, `m5pm1`, `button`, `knob`, `led_strip`.
- **UI:** `lvgl/lvgl ~9.5.0`, `esp_lvgl_port ~2.9.0`, `image_player`, `esp_emote_expression`, `esp_mmap_assets`, `esp-hub75` (S3/P4), `xpowerslib`.
- **Аудио:** `esp_codec_dev ~1.6.2`, `esp_audio_codec ~2.5.0`, `esp_audio_effects ~1.3.0` (держи ниже P4 rev>=3.0 CMake-гейта), `esp-sr ==2.4.7`.
- **Сеть и модемы:** `78/esp-wifi-connect ~3.3.1`, `78/esp-ml307 ~3.7.0`, `78/uart-eth-modem ~0.6.5` (не для esp32), `esp_hosted` (h2/p4), `esp_wifi_remote` (p4), `iot_usbh_rndis` (s3/p4).
- **Камера и видео:** `esp32-camera` (только S3), `esp_video`, `esp_image_effects` (не esp32), `esp_new_jpeg`, `sscma_client` (SenseCAP Watcher).
- **Питание и сенсоры:** `adc_battery_estimation`, `bq27220`, `bmi270_sensor` (S3/C5), `touch_slider_sensor`, `touch_button_sensor`.
- **Шрифты и ассеты:** `ZDarow/xiaozhi-fonts ~2.0.0`, `sh1106-esp-idf`, `servo_dog_ctrl` (C3), `otto-emoji-gif-component` (S3).

Замечания по зависимостям:

- `dependencies.lock` в `.gitignore` → версии компонентов не зафиксированы в репозитории, сборка не полностью воспроизводима. Это осознанное решение upstream, не «исправляй» молча.
- **19 из 62 компонентов — сторонние** (не `espressif/*` и не `lvgl/*`): `78/*` (3), `waveshare/*` (4), `m5stack/*` (2), `ZDarow/xiaozhi-fonts`, `esphome/esp-hub75`, `txp666/otto-emoji-gif-component`, `wvirgil123/sscma_client`, `tny-robotics/sh1106-esp-idf`, `espfriends/servo_dog_ctrl`, `kevincoooool/esp_lcd_st7102`, `cube32esp/xpowerslib`. Все тянутся с `github.com`, поэтому зеркала нужны для каждого источника, а не только для `78/*` и `ZDarow/*`. Инвентаризация: `main/idf_component.yml`. Замена на зеркала вида `xiaozhi-ru/*` либо приватный реестр — отдельная задача с полной пересборкой матрицы.
- Не редактируй `managed_components/` — это вендорские выходные, они перезаписываются.

## Required Rules

- Сохраняй несвязанные изменения worktree и держи патчи сфокусированными.
- Сборка должна экспортировать ровно одну фабрику плат через `DECLARE_BOARD(...)`.
- Никогда не меняй пины существующей платы, чтобы поддержать другое железо. Добавляй уникальную плату или релиз-вариант; идентичность платы влияет на совместимость OTA.
- Ядро зависит от интерфейсов `Board`, никогда — от конкретного класса платы или `config.h` платы.
- Камера, подсветка, дисплей, LED, аккумулятор и аналогичные возможности — опциональны. Проверяй `CONFIG_BOARD_HAS_*` перед обращением к возможности.
- Изменяй состояние во время выполнения через `Application::SetDeviceState()` и машину состояний.
- Колбэки могут выполняться вне главной задачи. Планируй мутации приложения через `Application::Schedule()` или биты событий.
- Не блокируй главный цикл событий или аудио-задачи. Избегай неограниченных очередей и повторных крупных выделений в аудио-пути.
- Не выделяй память под образ прошивки целиком: OTA-образ занимает мегабайты, читать его нужно блоками.
- Держи общую семантику сообщений в `Protocol`; проверяй оба транспорта при изменении контракта.
- Проверяй сетевой вход и сохраняй владение `cJSON` (используй `CJsonUniquePtr` из `main/cjson_utils.h`). Ключи NVS — постоянный API; при их изменении нужна миграция.
- Защищай target-специфичные функции правилами Kconfig/COMPONENT. Не предполагай, что у каждого таргета есть PSRAM или ресурсы S3/P4.
- Не редактируй вручную сгенерированные/вендоровские выходные: `build/`, `releases/`, `managed_components/`, `components/`, `sdkconfig*`, `main/assets/lang_config.h`, сгенерированные mmap-заголовки.
- Форматируй только тронутые C/C++-файлы с помощью `.clang-format` репозитория; избегай массового форматирования несвязанных файлов.
- Новый компонент обязан быть зарегистрирован: `EXTRA_COMPONENT_DIRS` в корневом `CMakeLists.txt` либо `idf_component_register` + `REQUIRES` в `main/CMakeLists.txt`. Код под `#ifdef CONFIG_*` без регистрации — источник падения сборки.
- Дублируй опцию только вместе с миграцией: `CONFIG_OTA_SIGNATURE_PUBKEY` (`main/Kconfig.projbuild`) существует как публичный API, но его потребитель — прототип в `feature/gost-crypto`. Не добавляй второе объявление той же сущности.

## Известные риски и мёртвый код (аудит 03.10.2026, часть закрыта 03.10.2026)

Не исправляй молча и не «доделывай» на свой страх и риск. Каждый пункт — отдельная задача с планом и критериями проверки.

**Закрыто 03.10.2026 (этап 0 плана).** Прототип ГОСТ вынесен в ветку `feature/gost-crypto` с README, перечисляющим дефекты; из `feature/russia-adaptation` удалены `main/gost_crypto/`, заглушка `Ota::VerifyGostSignature()` и `#include <esp_gost.h>`; `CONFIG_USE_GOST_CRYPTO=y` теперь останавливает конфигурацию через `message(FATAL_ERROR)` в начале `main/CMakeLists.txt` (проверено изолированным прогоном CMake в обеих ветвях). Коммиты: `e09d5a1` (ветка прототипа), `e92d17e` (очистка).

Остаётся открытым:

1. **Криптография в ветке `feature/gost-crypto` неверна, проверка подписи всегда проваливается.** `gost_streebog.cc` не реализует Streebog: функция сжатия построена на Magma вместо LPS из RFC 6986, `streebog_compress` помечена «Simplified», IV для 256-битного режима заполнен `0x01` во всех байтах. `gost_magma.cc` содержит S-таблицы `S1`–`S3` в виде identity и обрывается на `S0` из повторяющихся строк. `gost_signature.cc` — заглушки: `point_mul` копирует точку, `point_add` копирует аргумент, `bn_inv` возвращает единицу, `CURVE_Q` содержит 64-битное значение, `G_POINT_Y` заполнен нулями, `VerifySignature()` безусловно `return false`. В релиз нельзя: ни один тестовый вектор не проходит. Требования к готовности — в `main/gost_crypto/README.md` на той ветке.
2. **Протокол подписи не согласован с сервером.** OTA-эндпоинт отдаёт обычный бинарник без хвоста подписи, поэтому проверка `total_read - GOST_SIG_LEN` срабатывала бы на произвольных данных. Формат (отдельный `.sig` или хвост образа) должен быть зафиксирован вместе с серверной стороной.
3. **Строгий фолбэк опасен для OTA.** При включённой ГОСТ-проверке невалидная подпись приводит к `esp_ota_abort()` и отказу обновления. Политика «проверка включена = проверка обязательна» должна быть явной опцией, а не следствием `CONFIG_USE_GOST_CRYPTO`.
4. **Docker-образ ссылается на чужой upstream.** `docker/firmware-builder/Dockerfile:8` — `FIRMWARE_SOURCE_URL=https://github.com/78/xiaozhi-esp32`, тогда как репозиторий форкнутый. Метка врёт, и сборка образа не соответствует коду.
5. **`.vscode/` в `.gitignore`, но `.vscode/extensions.json` отслеживается** (добавлен через `git add -f`, коммит `12e4f83`). Расхождение намеренное: список расширений общий, `settings.json` с локальным путём `idf.currentSetup` остаётся неотслеживаемым. Не добавляй `.vscode/settings.json` в индекс.
6. **Ассеты и озвучка версионируются в открытых местах.** `main/assets/` и `scripts/spiffs_assets/` содержат бинарные модели; любая правка меняет размер образа и лимит раздела — проверяй `python3 scripts/build.py ...` и тесты ассетов.
7. **`main/ota.cc` не соответствует `.clang-format` целиком** (предсуществующее состояние, проверено clang-format 18.1.3: нарушения на строках с `cJSON *`, длине строк, `Ota::~Ota()`). CI проверяет только дифф (`clang-format-diff-18 -p1`), поэтому это не ломает сборку. Не форматируй файл целиком в рамках посторонней задачи: это создаст diff на ~600 строк. Проверяй только свои строки: `git diff -U0 -- '*.cc' '*.h' | clang-format-diff-18 -p1 -style=file`.
8. **Данные от сервера приводят к аварийному останову** (аудит 03.10.2026, `.10x/evidence/audit-20261003.md`):
   - `std::stoi` в `main/ota.cc:435` и `main/protocols/mqtt_protocol.cc:154` вызывается на строке из JSON-ответа OTA-сервера, а исключения выключены (`CONFIG_COMPILER_CXX_EXCEPTIONS=n`) → `abort()` → перезагрузка. Отдельно: `main/audio/audio_debugger.cc:25`.
   - `Settings::SetString`/`SetInt` (`main/settings.cc:53`, `:74`) вызывают `ESP_ERROR_CHECK`; ключи берутся из ответа сервера без фильтрации (`main/ota.cc:180-190`, `:202-212`), а `NVS_KEY_NAME_MAX_SIZE = 16` → ключ длиннее 15 символов или заполненный namespace дают `ESP_ERR_INVALID_ARG`/нет места → panic.
   До исправления не отправляй на сервер нестандартные значения `firmware.version` и ключей `mqtt`/`websocket`.
9. **Образ прошивки качается без проверки подписи и без контроля схемы URL**: `main/ota.cc:249-252` принимает любой `firmware.url`, `Upgrade()` (`:296`) пишет образ и переключает загрузку; `esp_ota_end` проверяет только целостность. Компромеция OTA-эндпоинта = подмена прошивки и конфигурации разделов `mqtt`/`websocket` (связано с п. 8).
10. **Секреты в логах:** `main/ota.cc:493` печатает весь активационный payload вместе с `serial_number` и `hmac`; `main/ota.cc:80` печатает `Serial-Number`. Маскируй значения при логировании.
11. **Китайский сервис в документации для пользователей:** `https://xiaozhi.me/` остался в `.github/ISSUE_TEMPLATE/01_build_flash_bug.yml:9`, `02_runtime_bug.yml:9` и `:42`, `03_feature_request.yml:9`, `05_technical_question.yml:9`, `config.yml:7`, `.github/SUPPORT.md:49`. Поддержка переведена на `support@xiaozhi.ru`, сайт — нет.

## Boards and Configuration

Выбор платы — это связанная цепочка:

`main/boards/<vendor>/<board>/config.json` -> `scripts/build.py` -> `main/Kconfig.projbuild` -> `main/CMakeLists.txt` -> исходники платы и `config.h`.

При добавлении платы или варианта обновляй каждую соответствующую ссылку в этой цепочке. Включай уникальную идентичность платы, правильный чип-таргет, настройки flash/partition, ровно один `DECLARE_BOARD` и документацию по плате. Следуй `docs/custom-board.md`.

Разделы прошивки: `partitions/v1/*.csv` и `partitions/v2/*.csv` (4m/8m/16m/32m, варианты для C3 и кастомного wake word). Таблица по умолчанию — `partitions/v2/16m.csv`.

## Validation

- Изменение только платы: собери затронутые варианты и протестируй изменённое железо.
- Изменение ядра, common-плат, аудио, протоколов, дисплея, зависимостей, Kconfig или CMake: запускай host-тесты и собери репрезентативные чип/сетевые пути.
- Изменения протоколов: проверяй WebSocket и MQTT/UDP при изменении общего поведения.
- Изменения аудио: проверяй захват, воспроизведение, wake/VAD, прерывание, переподключение и применимые режимы AEC.
- Изменения UI/ассетов: проверяй пути без-дисплея/OLED/LVGL и размер partition.
- Всегда сообщай, что протестировано, и что всё ещё требует физического железа. Успешная сборка — не валидация железа.
- Локальный IDF 6.0.2 не покрывает P4/S31: для них обязателен CI или docker-образ на `espressif/idf:release-v6.1`.

## Authoritative Documentation

- Обзор и политика SDK: `README.md`
- Руководство по вкладу: `CONTRIBUTING.md`
- Руководство по платам: `docs/custom-board.md`
- Дизайн аудио: `main/audio/README.md`
- Стиль кода: `docs/code_style.md`
- Протоколы: `docs/websocket.md`, `docs/mqtt-udp.md`, `docs/mcp-protocol.md`
- Настройка VS Code и CLI-команды: `docs/agent-workspace-setup.md`
- План работ: `plans/`
- Артефакты аудита и решений: `.10x/evidence/`, `.10x/decisions/`
- CI-матрица: `.github/workflows/build.yml`

Держи подробную или быстро меняющуюся информацию в этих файлах, а не здесь. Добавляй вложенный `AGENTS.md` только когда подсистеме нужны специализированные инструкции.

## Git and Conventions

- Ветки — английский kebab-case: `feature/…`, `fix/…`, `docs/…`, `chore/…`. Текущая ветка `feature/russia-adaptation` не переименовывать без причины.
- Коммиты — на русском, повелительное наклонении, формат `<тип>: <описание>`, заголовок ≤ 72 символов. Типы: `feat`, `fix`, `refactor`, `docs`, `test`, `chore`, `style`, `perf`, `ci`.
- Перед крупными изменениями — чекпоинт: `git commit -m "WIP: checkpoint before <описание>"`.
- Идентификаторы кода — английские (`snake_case` / `CamelCase`). Комментарии, документация и отчёты — русские.
- Даты — ДД.ММ.ГГГГ, время — 24-часовой формат.
- Секреты (`*.pem`, `*.key`, `credentials.json`, `.env`) не коммитить; маскировать в логах: `sk-****abcd`.

## Ограничения (Центральная Россия)

Следующие правила действуют наряду с общими правилами конфигурации агента. Они имеют высший приоритет при противоречии.

### Сеть и синхронизация

- `git push` и синхронизация с удалёнными репозиториями — строго запрещены без явного указания или чёткого подтверждения Хозяина.
- Загрузка проектов и публикация в сеть — запрещены без явного указания.
- Не коммить `.env`, `*.pem`, `*.key`, `credentials.json`, `secrets.*`. Секреты — только через переменные окружения или `.env`-файл ( добавь в `.gitignore` при `git init`).
- В логах и отчётах маскируй секреты: `sk-****abcd`.

### Установка пакетов и управление системой

- `apt install` всегда требует подтверждения (`ask`).
- `sudo rm *`, `rm -rf /`, `mkfs`, `dd if=` и другие деструктивные операции — запрещены.
- `systemctl restart/stop` для глобальных/системных сервисов — требуют подтверждения. Управление локальными сервисами проекта (docker-compose, dev-серверы, локальные демоны) — разрешено автономно.

### Git

- `git init`, `git add`, `git commit` — разрешены в автономном режиме, но только в рамках рабочей директории текущего проекта.
- Перед крупными изменениями (рефакторинг, миграция, удаление файлов) — создай снапшот: `git commit -m "WIP: checkpoint before <описание>"`.
- При критической ошибке — откатись к последнему рабочему состоянию (`git stash`, `git checkout`, `git reset`).
- Commit messages — на русском, в повелительном наклонении.
- Названия веток — на английском, kebab-case: `fix/auth-timeout`, `feature/docker-stats`.

### Язык и стиль

- Все ответы, комментарии, документация — на русском.
- Имена переменных, функций, классов — на английском (`snake_case`, `CamelCase`).
- Даты — ДД.ММ.ГГГГ, время — 24-часовой формат.

### Самопроверка перед отчётом

- Линтер: `clang-format --dry-run -Werror <files>` (или эквивалент для языка проекта).
- Типы: аналог `mypy --strict` для соответствующего языка.
- Тесты: `python3 -m unittest discover -s scripts/tests -v`.
- Прочитай свой код глазами ревьюера — нет ли TODO, заглушек, `print()` для отладки, закомментированного кода?
- Проверь `.gitignore` — не добавлено ли лишнего?
- Если хотя бы один пункт провален — исправь и повтори проверку.
