# Implementation Plan: Адаптация xiaozhi-esp32 для Центральной России

## Approach
Данная задача требует комплексного подхода, затрагивающего документацию, конфигурацию облачных сервисов, криптографию и процессы разработки. Выбран **HARD mode** из-за:
- Необходимости реализации ГОСТ 2012 (криптографические алгоритмы)
- Замены множества hardcoded китайских сервисов
- Соответствия строгим ограничениям Центральной России (git push, apt, секреты)

**Альтернативы, рассмотренные:**
1. *Минимальный подход* — только перевод документации и env-переменные для URL. Отклонено: не решает проблему ГОСТ и OTA подписи.
2. *Форк с полной заменой* — создать отдельный репозиторий. Отклонено: нарушает политику OTA совместимости и усложняет поддержку.
3. *Выбранный подход* — адаптация в текущем репозитории с конфигурируемостью через Kconfig/env, сохранением обратной совместимости.

---

## Steps

### Phase 1: Аудит и подготовка (2-3 часа)

#### 1.1 Полный аудит китайских сервисов (30 мин)
```bash
# Поиск всех вхождений
grep -r "tenclass\|api.tenclass\|xiaozhi.me\|78/xiaozhi\|oshwhub.com/tenclass" --include="*.md" --include="*.yml" --include="*.yaml" --include="*.py" --include="*.cc" --include="*.h" --include="*.c" .
```
**Файлы к проверке:**
- `README.md`, `README_zh.md`, `README_ja.md` — ссылки на github.com/78/*, oshwhub.com/tenclass*, xiaozhi.me
- `main/Kconfig.projbuild` — `CONFIG_OTA_URL` default = `https://api.tenclass.net/xiaozhi/ota/`
- `.github/ISSUE_TEMPLATE/*.yml` — email `xiaozhi.ai@tenclass.com`, ссылка на `xiaozhi.me`
- `.github/SUPPORT.md` — email и сайт
- `main/idf_component.yml` — зависимости `78/esp_lcd_nv3023`, `78/esp-wifi-connect`, `78/esp-ml307`, `78/uart-eth-modem`, `78/xiaozhi-fonts`
- `scripts/build.py` — логика сборки, релизы
- Документация в `docs/*.md`

#### 1.2 Создание бэкап-коммита (5 мин)
```bash
git commit -am "WIP: checkpoint before Russia adaptation"
```

#### 1.3 Подготовка .env.example и .gitignore (15 мин)
Создать шаблон переменных окружения для всех конфигурируемых URL.

---

### Phase 2: Перевод документации на русский (4-6 часов)

#### 2.1 Основные файлы (приоритет высокий)
| Файл | Статус | Примечание |
|------|--------|------------|
| `README.md` | Перевести | Основной файл, сохранить структуру |
| `.github/SUPPORT.md` | Перевести | Критично для поддержки пользователей |
| `.github/ISSUE_TEMPLATE/*.yml` (5 файлов) | Перевести | Шаблоны багов, фич, вопросов |
| `docs/custom-board.md` | Перевести | Руководство по платам |
| `docs/websocket.md` | Перевести | Протокол WebSocket |
| `docs/mqtt-udp.md` | Перевести | Протокол MQTT+UDP |
| `docs/mcp-protocol.md` | Перевести | MCP протокол |
| `docs/mcp-usage.md` | Перевести | Использование MCP |
| `docs/code_style.md` | Перевести | Стиль кода |
| `docs/blufi.md` | Перевести | BluFi настройка |
| `docs/glyph-push.md` | Перевести | Glyph push |
| `docs/notify.md` | Перевести | Уведомления |

#### 2.2 Правила перевода
- Термины: OTA → OTA, WebSocket → WebSocket, MQTT → MQTT, GPIO → GPIO
- "Flash" → "Прошивка", "Build" → "Сборка", "Board" → "Плата"
- Сохранить английские названия инструментов (ESP-IDF, CMake, Git)
- Ссылки на внешние ресурсы оставить, добавить примечание о языковой версии

#### 2.3 Автоматизация (опционально)
Использовать скрипт с `translate-shell` или API для предварительного перевода, затем ручная правка.

---

### Phase 3: Замена китайских облачных сервисов (6-8 часов)

#### 3.1 Kconfig конфигурация (main/Kconfig.projbuild)
```kconfig
# Заменить hardcoded OTA URL на конфигурируемый
config OTA_URL
    string "Default OTA URL"
    default "https://ota.xiaozhi.ru/xiaozhi/ota/"  # Российский зеркало
    help
        URL для проверки обновлений прошивки. Можно переопределить через env OTA_URL.

config OTA_SIGNATURE_PUBKEY
    string "OTA Signature Public Key (PEM)"
    default ""
    help
        Публичный ключ для проверки подписи OTA (ГОСТ 2012). Пусто = без проверки.

config MQTT_DEFAULT_ENDPOINT
    string "Default MQTT Endpoint"
    default "mqtt.xiaozhi.ru"
    help
        MQTT брокер по умолчанию.

config WEBSOCKET_DEFAULT_ENDPOINT
    string "Default WebSocket Endpoint"
    default "wss://ws.xiaozhi.ru"
    help
        WebSocket сервер по умолчанию.

config ASSETS_GENERATOR_URL
    string "Custom Assets Generator URL"
    default "https://assets.xiaozhi.ru"
    help
        Генератор кастомных ассетов (замена github.com/78/xiaozhi-assets-generator)

config SUPPORT_EMAIL
    string "Support Email"
    default "support@xiaozhi.ru"
    help
        Email для поддержки (замена xiaozhi.ai@tenclass.com)

config SUPPORT_WEBSITE
    string "Support Website"
    default "https://xiaozhi.ru"
    help
        Сайт поддержки (замена xiaozhi.me)
```

#### 3.2 Переменные окружения (scripts/build.py + CI)
Добавить поддержку env-переменных для переопределения при сборке:
```python
# В build.py: чтение из os.environ
ota_url = os.environ.get("OTA_URL", config_value)
```

#### 3.3 Замена зависимостей 78/* в idf_component.yml
**Стратегия:** Форкнуть критические компоненты на российский GitHub/GitLab или использовать зеркала.
| Компонент | Действие |
|-----------|----------|
| `78/esp_lcd_nv3023` | Форкнуть на `xiaozhi-ru/esp_lcd_nv3023` |
| `78/esp-wifi-connect` | Форкнуть или заменить на `espressif/esp-wifi-connect` |
| `78/esp-ml307` | Форкнуть на `xiaozhi-ru/esp-ml307` |
| `78/uart-eth-modem` | Форкнуть на `xiaozhi-ru/uart-eth-modem` |
| `78/xiaozhi-fonts` | Форкнуть на `xiaozhi-ru/xiaozhi-fonts` |

#### 3.4 Обновление ссылок в документации и board README
- `github.com/78/xiaozhi-assets-generator` → `github.com/xiaozhi-ru/xiaozhi-assets-generator`
- `oshwhub.com/tenclass01/*` → оставить (аппаратная документация) или зеркало
- `xiaozhi.me` → `xiaozhi.ru` (или оставить как fallback)

#### 3.5 GitHub Issue Templates и SUPPORT.md
Заменить email `xiaozhi.ai@tenclass.com` → `support@xiaozhi.ru`
Заменить ссылки на `xiaozhi.me` → `xiaozhi.ru`

---

### Phase 4: Реализация ГОСТ 2012 для TLS и OTA (12-16 часов)

#### 4.1 Анализ текущей криптографии
Текущее состояние:
- MQTT: PSA Crypto (AES-GCM для шифрования пакетов)
- OTA: HMAC-SHA256 для активации (esp_hmac с KEY0)
- TLS: mbedTLS через ESP-IDF (стандартные наборы шифров)

#### 4.2 Выбор библиотеки ГОСТ
**Варианты:**
1. *mbedTLS с ГОСТ* — ESP-IDF включает mbedTLS, но ГОСТ не встроен по умолчанию
2. *OpenSSL с ГОСТ (engine)* — тяжелый для ESP32
3. *КриптоПро CSP для встраиваемых систем* — коммерческий, требует лицензии
4. *Самостоятельная реализация* — высокий риск ошибок

**Рекомендация:** Использовать **mbedTLS с поддержкой ГОСТ** через патчи или компонент `espressif/mbedtls-gost` (если доступен), либо интегрировать `gost-engine` для mbedTLS.

#### 4.3 Kconfig для ГОСТ
```kconfig
menu "Russian Cryptography (ГОСТ 2012)"

config USE_GOST_CRYPTO
    bool "Enable GOST 2012 Cryptography"
    default n
    help
        Включить поддержку ГОСТ 2012 (ТК 26) для TLS и подписи OTA.
        Требует дополнительной памяти (flash + RAM).

config GOST_TLS_CIPHERSUITES
    bool "Enable GOST TLS Cipher Suites"
    depends on USE_GOST_CRYPTO
    default y
    help
        Включить TLS_GOSTR341112_256_WITH_KUZNYECHIK_CTR_OMAC и аналоги.

config GOST_OTA_SIGNING
    bool "Enable GOST 2012 for OTA Signature Verification"
    depends on USE_GOST_CRYPTO
    default y
    help
        Проверка подписи OTA образа по ГОСТ Р 34.10-2012 (512/256 бит).

config GOST_KEY_STORAGE
    choice
        prompt "GOST Key Storage"
        depends on USE_GOST_CRYPTO
        default GOST_KEY_STORAGE_EFUSE
    config GOST_KEY_STORAGE_EFUSE
        bool "eFuse (HMAC peripheral)"
        help
            Использовать HMAC периферию ESP32 для хранения ключей.
    config GOST_KEY_STORAGE_FLASH
        bool "Encrypted Flash Partition"
        help
            Хранить ключи в зашифрованном разделе flash.
    config GOST_KEY_STORAGE_EXTERNAL
        bool "External Secure Element (ATECC608A, etc.)"
    endchoice

endmenu
```

#### 4.4 Реализация OTA подписи ГОСТ
**Файлы к изменению:**
- `main/ota.cc` — добавление проверки подписи при загрузке
- `main/ota.h` — новые методы `VerifyGostSignature()`
- `main/CMakeLists.txt` — линковка ГОСТ библиотеки

**Алгоритм:**
1. При сборке: подписать `merged-binary.bin` закрытым ключом ГОСТ 34.10-2012
2. В прошивке: хранить публичный ключ (PEM/DER) в Kconfig или efuse
3. При OTA: после скачивания проверить подпись перед `esp_ota_end()`

#### 4.5 TLS с ГОСТ
**Файлы к изменению:**
- `main/protocols/mqtt_protocol.cc` — настройка PSA/mbedTLS для ГОСТ шифров
- `main/protocols/websocket_protocol.cc` — аналогично для WebSocket
- Компонент `mbedtls` в ESP-IDF — включить `MBEDTLS_SSL_PROTO_TLS1_2` и ГОСТ ciphersuites

---

### Phase 5: Соответствие ограничениям Центральной России (2-3 часа)

#### 5.1 Git workflow
- **Ветки:** `feature/russia-docs`, `feature/russia-cloud-config`, `feature/gost-crypto`, `feature/russia-compliance`
- **Коммиты:** На русском, повелительное наклонение
  - `перевести README на русский`
  - `добавить Kconfig для OTA URL`
  - `реализовать проверку ГОСТ подписи OTA`
- **Git push:** Только с явного подтверждения Хозяина

#### 5.2 Секреты и конфигурация
- Создать `.env.example` с шаблоном всех URL и ключей
- Добавить `.env` в `.gitignore`
- В коде использовать `os.getenv()` / Kconfig для чтения
- Никаких хардкодов секретов

#### 5.3 Apt install
- Все системные зависимости документировать в `docs/russia-setup.md`
- Не выполнять `apt install` автоматически

#### 5.4 Кодстайл
- Переменные/функции: `snake_case` / `CamelCase` (английский)
- Комментарии: русский
- Документация: русский

---

### Phase 6: Тестирование и валидация (4-6 часов)

#### 6.1 Unit тесты (scripts/tests/)
- Тест парсинга Kconfig значений
- Тест верификации ГОСТ подписи (мок)
- Тест fallback на дефолтные URL при отсутствии env

#### 6.2 Интеграционные тесты
- Сборка для 3+ плат (ESP32, ESP32-S3, ESP32-C3)
- Проверка OTA с ГОСТ подписью
- Проверка TLS handshake с ГОСТ ciphersuites

#### 6.3 Ручные тесты (требуют железа)
- Прошивка на реальную плату
- Проверка подключения к российским эндпоинтам
- Проверка OTA обновления

---

### Phase 7: CI/CD адаптация (2-3 часа)

#### 7.1 GitHub Actions (.github/workflows/build.yml)
- Добавить job для сборки с `USE_GOST_CRYPTO=y`
- Настроить артефакты с суффиксом `-gost`
- Добавить проверку форматирования `clang-format` для изменённых файлов

#### 7.2 Скрипт сборки (scripts/build.py)
- Поддержка флага `--gost` или env `USE_GOST_CRYPTO=1`
- Генерация отдельных релизов для ГОСТ-вариантов

---

## Timeline

| Phase | Duration | Dependencies |
|-------|----------|--------------|
| 1. Аудит и подготовка | 2-3 ч | — |
| 2. Перевод документации | 4-6 ч | 1 |
| 3. Замена облачных сервисов | 6-8 ч | 1 |
| 4. ГОСТ 2012 реализация | 12-16 ч | 1, 3 |
| 5. Соответствие ограничениям | 2-3 ч | 1 |
| 6. Тестирование | 4-6 ч | 2, 3, 4 |
| 7. CI/CD адаптация | 2-3 ч | 3, 4 |
| **Total** | **32-45 ч** | — |

---

## Rollback Plan

При критических проблемах:
```bash
# 1. Откат к чекпоинту
git reset --hard HEAD~1  # или конкретный коммит WIP

# 2. Откат отдельных фаз
git checkout HEAD~1 -- main/Kconfig.projbuild  # только Kconfig
git checkout HEAD~1 -- .github/  # только GitHub templates
git checkout HEAD~1 -- docs/  # только документация

# 3. Полный откат к main
git checkout main
git clean -fd
```

**Критические точки необратимости:**
- Изменение `main/idf_component.yml` (зависимости) — требует полной пересборки
- Добавление ГОСТ кода — увеличивает размер прошивки (~50-100KB)
- Изменение OTA URL — ломает совместимость со старыми устройствами без env

---

## Security Checklist

- [ ] **Input validation** — все env переменные валидируются (URL формат, длина ключей)
- [ ] **Auth checks** — OTA подпись верифицируется перед установкой
- [ ] **Rate limiting** — не применимо (устройство-инициатор)
- [ ] **Error handling** — graceful fallback на стандартные URL/алгоритмы
- [ ] **Secrets management** — приватные ключи ГОСТ только в efuse/secure element, НЕ в коде
- [ ] **TLS validation** — проверка сертификатов сервера (CA bundle)
- [ ] **OTA integrity** — SHA256 + ГОСТ подпись двойная проверка
- [ ] **No hardcoded secrets** — grep по репозиторию перед коммитом
- [ ] **GOST compliance** — использование сертифицированных реализаций (ФСБ/ФСТЭК)

---

## Files to Modify (Summary)

### Core Config
- `main/Kconfig.projbuild` — новые опции OTA, MQTT, WS, ГОСТ
- `main/idf_component.yml` — замена 78/* зависимостей
- `main/CMakeLists.txt` — условная компиляция ГОСТ

### OTA & Crypto
- `main/ota.cc` / `main/ota.h` — ГОСТ верификация, env URL
- `main/protocols/mqtt_protocol.cc` — ГОСТ TLS
- `main/protocols/websocket_protocol.cc` — ГОСТ TLS

### Documentation (Translate to Russian)
- `README.md`
- `.github/SUPPORT.md`
- `.github/ISSUE_TEMPLATE/01_build_flash_bug.yml`
- `.github/ISSUE_TEMPLATE/02_runtime_bug.yml`
- `.github/ISSUE_TEMPLATE/03_feature_request.yml`
- `.github/ISSUE_TEMPLATE/04_board_support_request.yml`
- `.github/ISSUE_TEMPLATE/05_technical_question.yml`
- `.github/ISSUE_TEMPLATE/config.yml`
- `docs/custom-board.md`
- `docs/websocket.md`
- `docs/mqtt-udp.md`
- `docs/mcp-protocol.md`
- `docs/mcp-usage.md`
- `docs/code_style.md`
- `docs/blufi.md`
- `docs/glyph-push.md`
- `docs/notify.md`

### Build & CI
- `scripts/build.py` — env переменные, ГОСТ флаг
- `.github/workflows/build.yml` — матрица сборок с ГОСТ

### New Files
- `.env.example` — шаблон переменных окружения
- `docs/russia-setup.md` — инструкция по настройке в РФ
- `main/gost_crypto.h/cc` — обёртка для ГОСТ (если нужна)

---

## Next Steps

```bash
# 1. Создать ветку
git checkout -b feature/russia-adaptation

# 2. Начать с Phase 1: аудит
grep -r "tenclass\|api.tenclass\|xiaozhi.me\|78/xiaozhi" --include="*.md" --include="*.yml" --include="*.py" .

# 3. Создать чекпоинт
git commit -am "WIP: checkpoint before Russia adaptation"

# 4. После завершения плана — запустить реализацию
# /cook @plans/russia-adaptation-20260911.md
```

---

## Что требует физического железа для валидации

1. **OTA с ГОСТ подписью** — нужна плата с ESP32-S3 (PSRAM) для тестирования полного цикла
2. **TLS ГОСТ handshake** — нужен тестовый сервер с ГОСТ сертификатом
3. **BluFi provisioning** — проверка на реальном железе
4. **Wake word / Audio pipeline** — проверка регрессий после изменений

**Успешная сборка ≠ валидация железа.** Все изменения протоколов и криптографии требуют тестирования на целевых платах.