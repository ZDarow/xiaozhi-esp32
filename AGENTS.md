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
- `main/gost_crypto/` — незавершённая попытка ГОСТ 2012. **Не подключена и небезопасна, см. «Известные риски».**
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
- Компоненты `78/*` и `ZDarow/*` тянутся с `github.com`. Для Центральной России нужны зеркала: либо заменить на `xiaozhi-ru/*` (см. TODO в `main/idf_component.yml`), либо настроить `idf_component.yml` через приватный реестр. Правка требует отдельной задачи и полной пересборки.
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
- Дублируй опцию только вместе с миграцией: `CONFIG_OTA_SIGNATURE_PUBKEY` (`main/Kconfig.projbuild`) и `CONFIG_GOST_PUBLIC_KEY` (`main/gost_crypto/Kconfig`) — одна и та же сущность, объявленная дважды.

## Известные риски и мёртвый код (аудит 03.10.2026)

Не исправляй молча и не «доделывай» на свой страх и риск. Каждый пункт — отдельная задача с планом и критериями проверки.

1. **`main/gost_crypto/` не подключён к сборке.** Компонент отсутствует в `EXTRA_COMPONENT_DIRS`/`REQUIRES` (`grep -rn gost_crypto CMakeLists.txt main/CMakeLists.txt` пусто), при этом `main/ota.cc:26` делает `#include "gost_crypto.h"` под `CONFIG_USE_GOST_CRYPTO`. Любая сборка с `CONFIG_USE_GOST_CRYPTO=y` падает на include. `main/gost_crypto/Kconfig` в дерево Kconfig не подключён, поэтому его опции (`GOST_HASH_512`, `GOST_PUBLIC_KEY`, `GOST_KEY_STORAGE_*`) не существуют при конфигурации.
2. **Криптография неверна, проверка подписи всегда проваливается.** `main/gost_crypto/gost_streebog.cc` не реализует Streebog: функция сжатия построена на Magma вместо LPS из RFC 6986, `streebog_compress` помечена «Simplified», IV для 256-битного режима заполнен `0x01` во всех байтах. `gost_magma.cc` содержит S-таблицы `S1`–`S3` в виде identity и обрывается на `S0` из повторяющихся строк. `gost_signature.cc` — заглушки: `point_mul` копирует точку, `point_add` копирует аргумент, `bn_inv` возвращает единицу, `CURVE_Q` содержит 64-битное значение, `G_POINT_Y` заполнен нулями, `VerifySignature()` безусловно `return false` (строка 135). Включать этот код в релиз нельзя: он не проходит ни один тестовый вектор.
3. **Переполнение буфера в `main/ota.cc`.** Строки 435 и 649: буфер `uint8_t hash[GOST_HASH_LEN_256]` (32 байта) используется под `gost::Hash512` при `CONFIG_GOST_HASH_512` (опция по умолчанию `y` в `main/gost_crypto/Kconfig`) → запись 64 байт в 32-байтный буфер, выход за границу стека.
4. **Мёртвый код.** `Ota::VerifyGostSignature()` объявлен (`main/ota.h:62`) и реализован (`main/ota.cc:615`), но не вызывается нигде; при этом читает весь образ в `std::vector` в куче.
5. **Протокол подписи не согласован с сервером.** `main/ota.cc:417` вычитает `GOST_SIG_LEN` из размера скачанного образа, но OTA-эндпоинт отдаёт обычный бинарник без хвоста подписи; проверка срабатывает на произвольных данных.
6. **Строгий фолбэк опасен для OTA.** При включённой ГОСТ-проверке невалидная подпись приводит к `esp_ota_abort()` и отказу обновления. Политика «проверка включена = проверка обязательна» должна быть явной опцией, а не следствием `CONFIG_USE_GOST_CRYPTO`.
7. **Docker-образ ссылается на чужой upstream.** `docker/firmware-builder/Dockerfile:8` — `FIRMWARE_SOURCE_URL=https://github.com/78/xiaozhi-esp32`, тогда как репозиторий форкнутый. Метка врёт, и сборка образа не соответствует коду.
8. **`.vscode/` в `.gitignore`.** Настройки редактора не попадают в репозиторий; `.vscode/extensions.json` и `settings.json` действуют только локально. Для шаринки нужен `git add -f .vscode/extensions.json` или точечное исключение в `.gitignore`.
9. **Ассеты и озвучка версионируются в открытых местах.** `main/assets/` и `scripts/spiffs_assets/` содержат бинарные модели; любая правка меняет размер образа и лимит раздела — проверяй `python3 scripts/build.py ...` и тесты ассетов.

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
