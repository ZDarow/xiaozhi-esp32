# Внешние сервисы, API и библиотеки — 03.10.2026

Ветка: `feature/russia-adaptation` (`d500fa4`). Документ отвечает на вопрос «что подключить к
проекту для расширения функционала и оптимизации» и содержит проверенные на 03.10.2026 данные.

## 0. Метод и достоверность

Проверено программно:

- доступность хостов и API — `curl` с машины разработки (ЦФО, домашний канал);
- метаданные репозиториев — GitHub API (`stars`, `license`, `pushed_at`, дерево файлов);
- состав компонентов — распаковка tarball-версий из реестра `components.espressif.com`;
- утверждения об ESP-IDF — по локальной копии `/home/mi/.espressif/v6.0.2/esp-idf` (Kconfig, исходники).

Не проверялось: доступность из сетей мобильных операторов и с устройства; официальные прайсы
сервисов, чьи сайты недоступны с машины (Yandex Cloud, VK, Selectel); работа на реальном железе.

Уровни уверенности: **подтверждено** (проверено кодом/запросом), **по документации**
(заявлено вендором), **гипотеза** (требует проверки на устройстве).

## 1. Матрица доступности — читать первой

Из этой сети на 03.10.2026:

| Хост | Код | Вывод |
|------|-----|-------|
| `api.open-meteo.com`, `geocoding-api.open-meteo.com` | 200 / 200 | доступны, без ключа |
| `nominatim.openstreetmap.org` | 302 | доступен, нужен User-Agent |
| `api.telegram.org` | 401 (без токена) | доступен |
| `huggingface.co`, `ollama.com` | 200 | доступны |
| `github.com`, `hub.docker.com`, `components.espressif.com`, `docs.espressif.com`, `esp32.org` | 200 | доступны |
| `timeweb.io` | 200 | доступен (S3-совместимое хранилище) |
| `yandex.ru`, `yandex.cloud`, `api.giga.chat`, `salutesmart.ru`, `vk.com`, `selectel.ru` | 000 | **недоступны** |
| `ru.pool.ntp.org` | 000 | недоступен, `pool.ntp.org` — 200 |

Следствие: любая интеграция, требующая Яндекс/Salute/VK/Selectel, для этого форка сейчас
неработоспособна без прокси или замены. Рекомендуемые в отчёте варианты проверены на
доступность; для недоступных указана альтернатива.

## 2. Сводная оценка

Критерии: П — производительность, Б — безопасность, С — стоимость, И — простота интеграции,
СО — поддержка сообщества. Оценка 1–5.

| Вариант | П | Б | С | И | СО | Вердикт для этого проекта |
|---|---|---|---|---|---|---|
| `xinnan-tech/xiaozhi-esp32-server` (свой сервер) | 4 | 4 | 5 | 4 | 5 | **брать**: снимает зависимость от `xiaozhi.me`, нужен для РФ |
| `sherpa-onnx` на своём сервере (STT/TTS) | 5 | 4 | 4 | 3 | 5 | **брать**: снимает зависимость от облачных STT/TTS |
| ESP-SR 2.4.7 (текущий стек) | 4 | 4 | 5 | 5 | 5 | уже используется; расширять моделями |
| `espressif/esp-mqtt` (после 6.1) | 4 | 4 | 5 | 5 | 5 | **обязательно**: сейчас отсутствует в `idf_component.yml` |
| Подпись образов без Secure Boot | 4 | 5 | 5 | 4 | 5 | **брать**: закрывает риск 9 аудита |
| Open-Meteo + геокодирование | 4 | 4 | 5 | 5 | 4 | **брать**: погода/геолокация без ключей |
| Telegram Bot API | 3 | 3 | 5 | 5 | 5 | **брать с оговоркой**: секрет в прошивке |
| ESPHome `voice_assistant` | 4 | 3 | 4 | 2 | 5 | **не брать**: альтернативная прошивка, не расширение нашей |
| `mac8005/xiaozhi-mcp-ha` (мост в Home Assistant) | 3 | 3 | 5 | 4 | 3 | брать опционально, для HA-экосистемы |
| Yandex SpeechKit | 5 | 4 | 3 | 4 | 4 | **отложить**: недоступен из этой сети |
| GigaChat / Salute | 5 | 4 | 2 | 3 | 4 | **отклонить**: закрытый контур, нужна договорённость |
| `espressif/esp-gmf` | 4 | 4 | 5 | 3 | 3 | наблюдать: смена парадигмы аудиостека |
| `espressif/esp-dl` | 4 | 4 | 5 | 2 | 4 | по необходимости (свои модели на устройстве) |
| openWakeWord | 2 | 4 | 5 | 2 | 3 | **отклонить**: нет активной поддержки с 12.2025 |
| Silero (VAD/TTS) | 4 | 5 | 5 | 2 | 4 | отклонить для устройства, годится для сервера |
| Vosk | 3 | 3 | 5 | 2 | 4 | отклонить: качество ниже Whisper/SenseVoice |

## 3. Сервисы и библиотеки

### 3.1 Серверная часть

#### `xinnan-tech/xiaozhi-esp32-server`

- Описание: эталонный бэкенд протокола xiaozhi (Python + Java + Vue). Поддерживает MQTT+UDP и
  WebSocket, MCP-эндпоинт, плагины, RAGFlow-базу знаний, управление устройствами из панели.
- Ссылки: `https://github.com/xinnan-tech/xiaozhi-esp32-server` (MIT, ★10 721, последний
  коммит 2026-09-29), документация по развёртыванию — `docs/docker/`, `docker-setup.sh`.
- Англоязычный форк документации: `https://github.com/Coriana/xiaozhi-esp32-server-eng`.
- Плюсы: снимает зависимость от китайского облака; «бесплатная конфигурация» из README
  (все компоненты бесплатны); стриминг с 0.5.2 даёт выигрыш ~2,5 с отклика; есть локальные
  STT/TTS (FunASR/SenseVoiceSmall) вместо облачных; Docker.
- Минусы: образы с 0.8.2 только под x86_64 — для arm64 собирать локально; образы по умолчанию
  в `ghcr.nju.edu.cn` (Китай) — нужен свой реестр; сервер на 2 ядра / 4 ГБ при FunASR;
  безопасность зависит от конфигурации (MQTT/UDP без TLS по умолчанию).

#### `sherpa-onnx` (k2-fsa)

- Описание: офлайн-распознавание и синтез речи на CPU: Whisper, Zipformer, Paraformer,
  SenseVoice, Piper, VAD, KWS, диаризация. C++ с биндингами для 12 языков.
- Ссылки: `https://github.com/k2-fsa/sherpa-onnx` (Apache-2.0, ★15 085, коммит 2026-09-22),
  документация и модели — `https://k2-fsa.github.io/sherpa/`.
- Плюсы: Apache-2.0, работает офлайн, есть SenseVoice (многоязычный, включая русский),
  готовые примеры для сервера xiaozhi, wasm-сборки для проверки моделей без установки.
- Минусы: **каталогов для ESP32/MCU в репозитории нет** — проверено деревом GitHub API
  (верхний уровень: `Sources`, `cmake`, `c-api-examples`, `wasm` и языковые каталоги);
  модели занимают сотни МБ; на ESP32-S3 для реального времени не подтверждено.

#### OpenAI-совместимый шлюз (Ollama, openclaw)

- Описание: `tigerbryan/openclaw-xiaozhi` — мост «прошивка xiaozhi → OpenAI-совместимый
  `/v1/chat/completions`». `https://github.com/tigerbryan/openclaw-xiaozhi` (MIT, ★26, 2026-02).
- Плюсы: подключается любой локальный LLM (Ollama, vLLM, llama.cpp), данные не покидают РФ.
- Минусы: нагрузка на CPU; качество зависит от модели; проект молодой, сообщество минимальное.

### 3.2 Речь на устройстве

#### ESP-SR 2.4.7 (текущий стек)

- Ссылки: `https://components.espressif.com/components/espressif/esp-sr`,
  `https://docs.espressif.com/projects/esp-sr/en/latest/esp-sr/index.html`.
- Плюсы: WakeNet (3 мс на кадр, 16 КБ RAM, 320 КБ флеш), MultiNet (WER 8,5 % в чистой речи,
  21,3 % в шуме), AFE с AEC/NS/VAD, ESP-SR TTS (только китайский). Лицензия допускает
  коммерческое использование, заказная модель слова-активации — платно.
- Минусы: китайский TTS; нет русской wake word «из коробки»; модель под 2 МБ флеш.
- Статус в проекте: `CONFIG_SR_WN_WN9_NIHAOXIAOZHI_TTS=y` в `sdkconfig.defaults.esp32s3`
  и `.esp32p4` — то есть китайская озвучка включена.

#### openWakeWord / Silero / Vosk — почему не берём

- `dscripka/openWakeWord` (Apache-2.0, ★2 809, последний коммит 2025-12-30): обучение и
  inference на десктопе, для MCU нужен свой конвейер; поддержка фактически заморожена.
- Silero (MIT, ★10 345 репозиторий `silero-vad`): качественно, но требует ONNX-рантайм
  на MCU, место в флеш; ESP-SR уже даёт VAD.
- Vosk (`alphacep/vosk-api`, Apache-2.0, ★15 161): качество распознавания ниже SenseVoice/Whisper.

### 3.3 Облачные STT/TTS

#### Yandex SpeechKit (по документации, недоступен из этой сети)

- Ссылки: `https://yandex.cloud/ru/docs/speechkit/`, синхронное распознавание v1
  `https://yandex.cloud/ru/docs/speechkit/stt/api/request-api` — **обе вернули код 000**.
- Плюсы (заявлено): STT v1/v2/v3, TTS v3 с Brand Voice по шаблону, gRPC, оплата в рублях.
- Минусы: недоступен из проверенной сети; цена TTS порядка 650 ₽ за 1 млн символов
  (посторонний обзор, не подтверждён официальной страницей); Brand Voice — по заявке.
- Вывод: не закладывать в архитектуру; держать точку расширения в сервере.

#### OpenAI / ElevenLabs

Доступ не проверялся (не в зоне задачи). Плюс ElevenLabs — качество; минусы для этого
проекта — зарубежные платежи и задержка. Интеграция возможна только через свой сервер
(п. 3.1), не через прошивку.

### 3.4 OTA-безопасность и хранилища

#### Подпись образов без Secure Boot (ESP-IDF, встроено)

- Описание: `CONFIG_SECURE_SIGNED_APPS_NO_SECURE_BOOT` («Require signed app images»,
  зависит только от `!SECURE_BOOT`) использует схему подписи Secure Boot v2, но не требует
  eFuse. `CONFIG_SECURE_SIGNED_ON_UPDATE_NO_SECURE_BOOT` (по умолчанию `y`) включает
  проверку подписи при любом OTA-обновлении через `esp_ota_ops.h`.
- Проверено по исходникам: `components/app_update/esp_ota_ops.c:527-538` — `ota_verify_partition()`
  вызывает `esp_image_verify(ESP_IMAGE_VERIFY, ...)`; вызывается из `esp_ota_end()` (стр. 594).
  То есть подпись проверяется автоматически на уже существующем пути `main/ota.cc:296`.
- Ссылки: `https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32/security/secure-boot-v2.html`
  (200), генерация ключа — `espsecure generate-signing-key`.
- Плюсы: закрывает риск 9 аудита (подмена образа) без перепрошивки eFuse у пользователей;
  ключ проверки вкомпилирован в приложение, отзыв — сменой прошивки.
- Минусы: не защищает от физического доступа (это честно указано в help); приватный ключ
  должен храниться в CI, а не в репозитории.

#### MCUboot

- Ссылка: `https://docs.mcuboot.com/` (200). Плюсы: откат при неудачной загрузке, независим
  загрузчик, подпись. Минусы: несовместимость с текущим загрузчиком ESP-IDF без переезда
  стораджа, объём работы большой. **Не брать сейчас** — подпись образов решает задачу дешевле.

#### Хранилище образов и OTA

- `timeweb.io` — 200, S3-совместимое, российский провайдер. Плюсы: доступность из ЦФО,
  оплата в рублях. Минусы: платно, нужен HTTPS-сертификат для домена.
- Yandex Object Storage / Selectel — 000 с этой машины. Вывод: **выбрать Timeweb или другой
  доступный S3**, зеркала Yandex/VK держать только как запасной вариант за прокси.

### 3.5 Инфраструктура и прошивка

#### ESP-IDF 6.1 — breaking changes, влияющие на проект

- Ссылка: `https://github.com/espressif/esp-idf/releases/tag/v6.1` (релиз 2026-08-27).
- **MQTT вынесен в component manager**: `espressif/mqtt`. В `main/idf_component.yml` этой
  зависимости нет (проверено grep), при переходе на 6.1 сборка сломается. Это P0-пункт.
- `mbedtls` 4.1.0: PSA opaque keys, сохранённые прежними версиями, требуют переимпорта.
- JPEG-декодер: добавлены строгие проверки (GHSA-v6r2-f6p2-88cj) — у нас `esp_driver_jpeg`
  в `PRIV_REQUIRES`, проверять обновление.
- ESP32-P4: ревизия по умолчанию v3.0, бинарники для <3.0 несовместимы — `sdkconfig.defaults.esp32p4`
  уже использует `CONFIG_SPIRAM_SPEED_200M=y`, ревизию надо проверить.
- **DSI**: `use_dma2d` заменён на `esp_lcd_dpi_panel_enable_dma2d()`. В коде смешанное
  состояние: `m5stack/tab5` и `esp32-p4-function-ev-board` уже вызывают новый API, а
  `lilygo/t-display-p4` (строка 197) и `m5stack/corep4` (строка 280) ещё ставят флаг.
- EIM (ESP-IDF Installation Manager) умеет ставить SDK с зеркал Espressif — полезно для РФ.

#### LVGL 9.5: PPA, DMA2D, кэш

- Ссылки: `https://lvgl.io/docs/open/9.5/integration/chip_vendors/espressif/hardware_accelerator_ppa`,
  `.../tips_and_tricks`, `https://lvgl.io/docs/open/integration/chip_vendors/espressif/hardware_accelerator_dma2d`.
- Подтверждено по документации: `CONFIG_LV_USE_PPA=y`, `CONFIG_LV_DRAW_BUF_ALIGN=64`
  (PPA даёт ~30 % экономии на заливке/копировании, до ~40 % на повороте);
  `CONFIG_LVGL_PORT_ENABLE_PPA` включает аппаратный поворот без кода;
  `CONFIG_SPIRAM_SPEED_200M` и увеличение `CONFIG_LV_PPA_BURST_LENGTH` лечат underrun на P4.
- Текущее состояние проекта: базовый `sdkconfig.defaults` содержит
  `CONFIG_COMPILER_OPTIMIZATION_SIZE=y`; для P4 в `sdkconfig.defaults.esp32p4` есть
  `CONFIG_COMPILER_OPTIMIZATION_PERF=y`, для S3 — нет (то есть S3 собирается с оптимизацией
  размера). Это осознанный размен: экономия флеш против скорости. Для S3 с PSRAM рекомендую
  отдельную перф-опцию только если хватает раздела `ota_*` (3,9 МБ в `partitions/v2/16m.csv`).
- PPA/DMA2D в проекте не включены ни в одном `sdkconfig.defaults*` (проверено grep) —
  включать только для вариантов P4.

#### `espressif/esp-gmf`

- Ссылка: `https://github.com/espressif/esp-gmf` (★171, коммиты 2026-09).
- Плюсы: унификация аудио/медиа-конвейера, готовые элементы и примеры, свежий CI на IDF 6.1/P4.
- Минусы: молодой проект, API нестабилен, миграция с `esp_audio_codec`/`esp_audio_effects`/
  `esp_codec_dev` — большой объём. **Наблюдать, не внедрять.**

#### `espressif/esp-dl`

- Ссылка: `https://github.com/espressif/esp-dl` (MIT, ★1 158, активные коммиты 2026-09-30).
- Плюсы: вывод моделей прямо на ESP32/P4 для собственных wake word и классификаторов.
- Минусы: требует конвертации моделей и места во флеш; для текущих задач избыточно.

### 3.6 Бесплатные внешние API для MCP-инструментов

| API | Ссылка | Ключ | Проверка | Ограничения |
|-----|--------|------|----------|-------------|
| Open-Meteo (погода) | `https://open-meteo.com/en/docs` | нет | 200 | бесплатно, без гарантий |
| Open-Meteo геокодирование | `https://geocoding-api.open-meteo.com/v1/search?name=Москва&language=ru` | нет | 200, JSON корректен | 1 запрос/с по политике |
| Nominatim (OSM) | `https://nominatim.org/release-docs/latest/` | нет | 302 | обязателен User-Agent, ≤1 req/s |
| Overpass API | `https://wiki.openstreetmap.org/wiki/Overpass_API` | нет | 406 на корне (норма) | тяжёлые запросы могут High Bandwidth |
| Telegram Bot API | `https://api.telegram.org/bot<token>/sendMessage` | токен | 401 без токена | секрет хранится в прошивке |

Open-Mete и геокодирование проверены живым запросом: ответ содержит
`{"results":[{"name":"Москва","latitude":55.75204,"longitude":37.61781,…}]}` и
`current.temperature_2m` с `timezone=Europe/Moscow`.

## 4. Готовые примеры интеграции

### 4.1 MCP-инструмент «погода» без ключей и платежей

Файл: `main/boards/<плата>/<плата>.cc`, в конструкторе платы вызвать `InitializeTools()`.
API HTTP — `NetworkInterface::CreateHttp()` из компонента `78/esp-ml307` (`NetworkResult<T>` =
`std::expected<T, NetworkError>`), сигнатуры `SetTimeout/SetHeader/Open/GetStatusCode/ReadAll/Close`.

```cpp
// main/boards/<vendor>/<board>/<board>.cc
#include "mcp_server.h"
#include "board.h"
#include "cjson_utils.h"
#include <cstdio>

void WeatherBoard::InitializeTools() {
    auto& mcp = McpServer::GetInstance();

    mcp.AddTool("local.get_weather",
                "Возвращает текущую погоду в указанном городе.\n"
                "Используй инструмент, когда пользователь спрашивает о погоде, температуре, осадках.\n"
                "Параметр city — название города на русском или английском языке.",
                PropertyList({
                    Property("city", kPropertyTypeString, "Москва"),
                }),
                [](const PropertyList& properties) -> ReturnValue {
                    const std::string city = properties["city"].value<std::string>();
                    auto network = Board::GetInstance().GetNetwork();
                    if (network == nullptr) {
                        return std::string("Сеть недоступна");
                    }

                    // 1) Геокодирование: Open-Meteo, без ключа.
                    char geo_url[320];
                    std::string q;
                    for (char c : city) {
                        q.push_back(c == ' ' ? '+' : c);  // плюс вместо пробела
                    }
                    snprintf(geo_url, sizeof(geo_url),
                             "https://geocoding-api.open-meteo.com/v1/search"
                             "?name=%s&count=1&language=ru&format=json",
                             q.c_str());

                    auto http = network->CreateHttp(0);
                    http->SetTimeout(5000);
                    if (!http->Open("GET", geo_url)) {
                        http->Close();
                        return std::string("Не удалось открыть соединение");
                    }
                    std::string geo_body = http->ReadAll();
                    http->Close();

                    CJsonUniquePtr geo_root(cJSON_Parse(geo_body.c_str()));
                    cJSON* results =
                        geo_root != nullptr ? cJSON_GetObjectItem(geo_root.get(), "results") : nullptr;
                    cJSON* first = cJSON_IsArray(results) ? cJSON_GetArrayItem(results, 0) : nullptr;
                    cJSON* lat = cJSON_IsObject(first) ? cJSON_GetObjectItem(first, "latitude") : nullptr;
                    cJSON* lon =
                        cJSON_IsObject(first) ? cJSON_GetObjectItem(first, "longitude") : nullptr;
                    cJSON* name = cJSON_IsObject(first) ? cJSON_GetObjectItem(first, "name") : nullptr;
                    if (!cJSON_IsNumber(lat) || !cJSON_IsNumber(lon)) {
                        return std::string("Город не найден: " + city);
                    }

                    // 2) Текущая погода: Open-Meteo, без ключа.
                    char wx_url[384];
                    snprintf(wx_url, sizeof(wx_url),
                             "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
                             "&current=temperature_2m,relative_humidity_2m,weather_code"
                             "&timezone=auto",
                             lat->valuedouble, lon->valuedouble);

                    auto http2 = network->CreateHttp(0);
                    http2->SetTimeout(5000);
                    if (!http2->Open("GET", wx_url)) {
                        http2->Close();
                        return std::string("Сервис погоды недоступен");
                    }
                    std::string wx_body = http2->ReadAll();
                    http2->Close();

                    CJsonUniquePtr wx_root(cJSON_Parse(wx_body.c_str()));
                    cJSON* current =
                        wx_root != nullptr ? cJSON_GetObjectItem(wx_root.get(), "current") : nullptr;
                    if (!cJSON_IsObject(current)) {
                        return std::string("Некорректный ответ сервиса погоды");
                    }
                    cJSON* temperature = cJSON_GetObjectItem(current, "temperature_2m");
                    cJSON* humidity = cJSON_GetObjectItem(current, "relative_humidity_2m");
                    cJSON* code = cJSON_GetObjectItem(current, "weather_code");

                    char answer[160];
                    snprintf(answer, sizeof(answer),
                             "%s: %.1f °C, влажность %.0f%%, код погоды %d",
                             cJSON_IsString(name) ? name->valuestring : city.c_str(),
                             cJSON_IsNumber(temperature) ? temperature->valuedouble : 0.0,
                             cJSON_IsNumber(humidity) ? humidity->valuedouble : 0.0,
                             cJSON_IsNumber(code) ? code->valueint : -1);
                    return std::string(answer);
                });
}
```

Регистрация по требованию `main/mcp_server.cc:36`: «Custom tools must be added in the board's
InitializeTools function». `main/ota.cc` и `main/assets.cc` используют тот же `CreateHttp`,
поэтому блокировок и таймаутов достаточно: `SetTimeout(5000)`, один инструмент — два запроса,
ответ короче 200 байт.

### 4.2 Обязательная зависимость для ESP-IDF 6.1

```yaml
# main/idf_component.yml
dependencies:
  # ESP-IDF 6.1: esp-mqtt перенесён в component manager (release notes v6.1)
  espressif/mqtt: "^1.0.0"
```

Страница компонента доступна: `https://components.espressif.com/components/espressif/mqtt` (200).
Проверка после правки: `python3 scripts/build.py main/boards/bread-compact-wifi --name check-mqtt`.

### 4.3 Подпись OTA-образов (закрывает риск 9 аудита)

```bash
# Ключи генерируются вне репозитория, файл в .gitignore
espsecure generate-signing-key secure_boot_signing_key.pem
espsecure extract-public-key secure_boot_signing_key.pem signature_verification_key.bin
```

```kconfig
# sdkconfig.defaults
CONFIG_SECURE_SIGNED_APPS_NO_SECURE_BOOT=y
CONFIG_SECURE_SIGNED_ON_UPDATE_NO_SECURE_BOOT=y
CONFIG_SECURE_BOOT_BUILD_SIGNED_BINARIES=y
CONFIG_SECURE_BOOT_SIGNING_KEY="secure_boot_signing_key.pem"
CONFIG_SECURE_BOOT_VERIFICATION_KEY="signature_verification_key.bin"
CONFIG_SECURE_SIGNED_APPS_RSA_SCHEME=y
```

Опции подтверждены в локальном Kconfig: `components/bootloader/Kconfig.projbuild:516`
(`SECURE_SIGNED_APPS_NO_SECURE_BOOT`), `:608` (`SECURE_SIGNED_ON_UPDATE_NO_SECURE_BOOT`),
`:711` (`SECURE_BOOT_SIGNING_KEY`), `:729` (`SECURE_BOOT_VERIFICATION_KEY`). Приложение
подписывается на этапе сборки, проверка выполняется внутри `esp_ota_end()`, изменения
в `main/ota.cc` не требуются.

### 4.4 Свой сервер xiaozhi с зеркалами и без китайских образов

```yaml
# docker-compose.override.yml рядом с main/xiaozhi-server/docker-compose_all.yml
services:
  xiaozhi-esp32-server:
    image: registry.local/xiaozhi-esp32-server:server_latest
  xiaozhi-esp32-server-web:
    image: registry.local/xiaozhi-esp32-server:web_latest
```

```bash
# Сборка под amd64 и перенос в свой реестр (образы 0.8.2+ только x86_64)
git clone --recursive https://github.com/xinnan-tech/xiaozhi-esp32-server.git
cd xiaozhi-esp32-server
docker build -f Dockerfile-server -t registry.local/xiaozhi-esp32-server:server_latest .
docker build -f Dockerfile-web     -t registry.local/xiaozhi-esp32-server:web_latest .
docker push registry.local/xiaozhi-esp32-server:server_latest
```

Конфигурация сервера — `main/xiaozhi-server/data/.config.yaml`: прописать `websocket` и
`ota` на свой домен, в разделе STT/TTS — SenseVoiceSmall (локально) либо sherpa-onnx,
LLM — локальный OpenAI-совместимый эндпоинт.

### 4.5 PPA и DMA2D для вариантов P4

```kconfig
# добавить в sdkconfig.defaults.esp32p4
CONFIG_LV_USE_PPA=y
CONFIG_LV_DRAW_BUF_ALIGN=64
CONFIG_LVGL_PORT_ENABLE_PPA=y
```

Эффект заявлен в документации LVGL 9.5: до ~30 % экономии времени рендеринга на заливке и
копировании, до ~40 % на повороте. В коде `esp_lcd_dpi_panel_enable_dma2d()` уже применяется
в `main/boards/m5stack/tab5` и `main/boards/espressif/esp32-p4-function-ev-board`; для
`lilygo/t-display-p4:197` и `m5stack/corep4:280` флаг `use_dma2d` нужно заменить.

### 4.6 Разделение сетевого стека (текущее состояние)

Факт: заголовки `http.h`, `network_interface.h`, `web_socket.h`, `udp.h`, `mqtt.h` отсутствуют
в репозитории и в 61 зависимости `main/idf_component.yml`, но присутствуют в компоненте
`78/esp-ml307` версии 3.7.3 (54 файла, `include/http.h`, `include/network_interface.h`,
`include/esp_network.h`). В форке закреплено `78/esp-ml307: ~3.7.0`, в upstream — `~3.7.3`.

Вывод: сетевой слой проекта фактически приходит из модемного компонента ML307. Это работает,
но делает модемный компонент обязательной зависимостью для **всех** плат, включая Wi-Fi, и
при зеркалировании зависимостей (риск 7 аудита) зеркалить нужно и его. Рекомендация: вынести
`http/network_interface/web_socket/mqtt/udp` в отдельный компонент проекта отдельной задачей;
до этого — зафиксировать минимальную версию `78/esp-ml307`, а не диапазон `~3.7.0`,
иначе обновление с 3.7.1 может убрать используемые заголовки.

## 5. Очередь внедрения

| № | Задача | Приоритет | Ожидаемый эффект | Проверка |
|---|--------|-----------|------------------|----------|
| 1 | Добавить `espressif/mqtt` в `idf_component.yml` | P0 | сборка на IDF 6.1 не сломается | `build.py` для Wi-Fi и MQTT-платы |
| 2 | Подпись образов без Secure Boot (4.3) | P0 | закрывает риск 9 | `esp_ota_end()` отвергает неподписанный образ |
| 3 | Заменить `use_dma2d` на `esp_lcd_dpi_panel_enable_dma2d()` | P1 | совместимость с IDF 6.1 | сборка `lilygo/t-display-p4`, `m5stack/corep4` |
| 4 | Инструмент погоды (4.1) | P1 | новая функция без ключей и оплаты | хост-тест разбора JSON + замер RSS |
| 5 | Свой сервер xiaozhi (4.4) | P1 | снятие зависимости от `xiaozhi.me` | разговор с устройства |
| 6 | sherpa-onnx как локальные STT/TTS | P2 | автономность и стоимость | сравнение WER с облаком |
| 7 | PPA/DMA2D для P4 (4.5) | P2 | FPS на P4 | `lv_demo_benchmark` до/после |
| 8 | Закрепить `78/esp-ml307` точной версией | P2 | воспроизводимость сборки | чистая сборка в docker |
| 9 | Мост в Home Assistant | P3 | экосистема HA | инструкция в `docs/` |

## 6. Что отвергнуто и почему

- **ESPHome `voice_assistant`** (`https://esphome.io/components/voice_assistant`, версия
  2026.9.1) — это отдельная прошивка для Home Assistant, а не расширение нашей. Замена
  форка на ESPHome означает потерю MCP, OTA-контура и 177 вариантов.
- **Yandex SpeechKit, GigaChat, Salute, VK Cloud, Selectel** — недоступны с этой машины
  (код 000). Архитектуру под них не строим; при появлении доступа — точка расширения
  в собственном сервере.
- **MCUboot** — решает больше, чем нужно сейчас, и ломает совместимость с текущим
  загрузчиком; подпись образов закрывает риск без миграции.
- **openWakeWord, Silero, Vosk на устройстве** — либо нет активной поддержки, либо хуже
  ESP-SR при том же месте во флеш.
- **sherpa-onnx на MCU** — в репозитории нет каталогов для ESP32 (проверено деревом GitHub).
  Только сервер.

## 7. Ссылки

- ESP-IDF 6.1 release notes: `https://github.com/espressif/esp-idf/releases/tag/v6.1`
- Secure Boot v2: `https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32/security/secure-boot-v2.html`
- esp-sr: `https://components.espressif.com/components/espressif/esp-sr`
- esp-mqtt: `https://components.espressif.com/components/espressif/mqtt`
- esp-lvgl-port: `https://components.espressif.com/components/espressif/esp_lvgl_port`
- LVGL 9.5 PPA: `https://lvgl.io/docs/open/9.5/integration/chip_vendors/espressif/hardware_accelerator_ppa`
- LVGL tips: `https://lvgl.io/docs/open/integration/chip_vendors/espressif/tips_and_tricks`
- xiaozhi-esp32-server: `https://github.com/xinnan-tech/xiaozhi-esp32-server`
- Форк сервера с англ. документацией: `https://github.com/Coriana/xiaozhi-esp32-server-eng`
- sherpa-onnx: `https://github.com/k2-fsa/sherpa-onnx`
- Мост OpenAI-совместимого шлюза: `https://github.com/tigerbryan/openclaw-xiaozhi`
- Мост в Home Assistant (HACS): `https://github.com/mac8005/xiaozhi-mcp-ha`
- ESPHome voice assistant: `https://esphome.io/components/voice_assistant`
- Open-Meteo: `https://open-meteo.com/en/docs`
- OSM Nominatim: `https://nominatim.org/release-docs/latest/`
- Overpass API: `https://wiki.openstreetmap.org/wiki/Overpass_API`
- MCUboot: `https://docs.mcuboot.com/`
- esp-gmf: `https://github.com/espressif/esp-gmf`
- esp-dl: `https://github.com/espressif/esp-dl`

Приложение `4.1` намеренно не включает вызов `Property` со строковым значением по умолчанию
для кириллицы без проверки длины: перед отправкой строки следует обрезать до
`kMaxLength` (в проекте применяется `SetMaxLength`, см. `main/mcp_server.h:110-116`).