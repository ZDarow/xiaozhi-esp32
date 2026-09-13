# Голосовой чат-бот на базе MCP

(Русский | [中文](README_zh.md) | [日本語](README_ja.md))

## Введение

👉 [Human: Give AI a camera vs AI: Instantly finds out the owner hasn't washed hair for three days【bilibili】](https://www.bilibili.com/video/BV1bpjgzKEhd/)

👉 [Handcraft your AI girlfriend, beginner's guide【bilibili】](https://www.bilibili.com/video/BV1XnmFYLEJN/)

Как точка входа для голосового взаимодействия, чат-бот XiaoZhi AI использует возможности больших языковых моделей, таких как Qwen / DeepSeek, и достигает управления множеством терминалов через протокол MCP.

<img src="docs/mcp-based-graph.jpg" alt="Управление всем через MCP" width="320">

## Последние обновления

- Проект теперь требует ESP-IDF v6.0.1 или новее. [ESP-IDF v6.1](https://github.com/espressif/esp-idf/releases/tag/v6.1) — рекомендуемый SDK. ESP-IDF 5.x больше не поддерживается. Текущая матрица содержит 171 вариант; сборка ESP32-S31 требует IDF 6.1 или новее.
- Криптографический код MQTT и BluFi перенесён на PSA Crypto. Разделение компонентов IDF 6 и совместимость сторонних зависимостей также учтены.
- Конвейер аудио, конкуренция, проверка пакетов MQTT/UDP и выбор матрицы релизов усилены.
- ESP32-P4 Rev1 и Rev3 обе поддерживаются на IDF 6 с ESP-SR 2.4.7.

### Реализованные функции

- Wi-Fi, проводной Ethernet, USB RNDIS и ML307/EC801E или NT26 Cat.1 4G; поддерживаемые платы могут переключаться между Wi-Fi и 4G
- Офлайн активация голоса с помощью [ESP-SR](https://github.com/espressif/esp-sr), включая настраиваемые слова активации
- Два транспорта связи: [WebSocket](docs/websocket.md) и [MQTT + UDP](docs/mqtt-udp.md)
- Аудиопоток Opus с традиционными конвейерами стримингового распознавания речи + LLM + TTS и конечными моделями голоса в реальном времени; оборудование с поддержкой AEC поддерживает взаимодействие в реальном времени в полудуплексном режиме
- Распознавание дикторов, идентификация текущего диктора [3D Speaker](https://github.com/modelscope/3D-Speaker)
- OLED / LCD дисплеи с поддержкой эмодзи и богатых выражений, а также ввод видео с камеры на поддерживаемых платах
- Отображение уровня батареи и управление питанием
- 39 языков интерфейса, с локализованными голосовыми подсказками там, где доступно, и английским fallback
- Платформы чипов ESP32, ESP32-C3, ESP32-C5, ESP32-C6, ESP32-S3 и ESP32-P4
- Настройка Wi-Fi через точку доступа или BluFi
- Устройственный MCP для управления устройствами (Динамик, LED, Серво, GPIO и т.д.)
- Облачный MCP для расширения возможностей больших моделей (умный дом, управление ПК, поиск знаний, email и т.д.)
- Настраиваемые слова активации, шрифты, эмодзи и фоны чата с онлайн-редактированием на веб-основе ([Генератор кастомных ассетов](https://github.com/ZDarow/xiaozhi-assets-generator))

## Лицензия

Данный проект распространяется под лицензией [MIT](LICENSE).

## Поддерживаемые платы

| Чип | Минимальная версия ESP-IDF | PSRAM | Flash | Примечания |
|-----|---------------------------|-------|-------|------------|
| ESP32 | v6.0.1 | Не требуется | 16 МБ | Базовая поддержка |
| ESP32-S3 | v6.0.1 | 8 МБ (рекомендуется) | 16 МБ | Аппаратное ускорение |
| ESP32-C3 | v6.0.1 | Не требуется | 16 МБ | Wi-Fi, Bluetooth |
| ESP32-C5 | v6.0.1 | Не требуется | 16 МБ | Wi-Fi 6 |
| ESP32-C6 | v6.0.1 | Не требуется | 16 МБ | Wi-Fi 6, Bluetooth 5 |
| ESP32-P4 | v6.1 | 8 МБ | 16 МБ | ESP-SR 2.4.7 |

## Поддержка

- 📧 [Служба поддержки](.github/SUPPORT.md)
- 📚 [Документация](docs/)
- 🐙 [GitHub репозиторий](https://github.com/ZDarow/xiaozhi-esp32)

### Практика на макетной плате

См. учебник на Feishu:

👉 ["Энциклопедия чат-бота XiaoZhi AI"](https://ccnphfhqs21z.feishu.cn/wiki/F5krwD16viZoF0kKkvDcrZNYnhb?from=from_copylink)

Демо на макетной плате:

![Демо на макетной плате](docs/v1/wiring2.jpg)

### Поддерживает 138 директорий плат и 171 вариантов релизов (частичный список)

- [LiChuang ESP32-S3 Development Board](https://oshwhub.com/li-chuang-kai-fa-ban/li-chuang-shi-zhan-pai-esp32-s3-kai-fa-ban)
- [Espressif ESP32-S3-BOX-3](https://github.com/espressif/esp-box)
- [M5Stack CoreS3](https://docs.m5stack.com/zh_CN/core/CoreS3)
- [M5Stack AtomS3R + Echo Base](https://docs.m5stack.com/en/atom/Atomic%20Echo%20Base)
- [Magic Button 2.4](https://gf.bilibili.com/item/detail/1108782064)
- [Waveshare ESP32-S3-Touch-AMOLED-1.8](https://www.waveshare.net/shop/ESP32-S3-Touch-AMOLED-1.8.htm)
- [LILYGO T-Circle-S3](https://github.com/Xinyuan-LilyGO/T-Circle-S3)
- [XiaGe Mini C3](https://oshwhub.com/tenclass01/xmini_c3)
- [CuiCan AI Pendant](https://oshwhub.com/movecall/cuican-ai-pendant-lights-up-y)
- [WMnologo-Xingzhi-1.54TFT](https://github.com/WMnologo/xingzhi-ai)
- [SenseCAP Watcher](https://www.seeedstudio.com/SenseCAP-Watcher-W1-A-p-5979.html)
- [ESP-HI Low Cost Robot Dog](https://www.bilibili.com/video/BV1BHJtz6E2S/)

<div style="display: flex; justify-content: space-between;">
  <a href="docs/v1/lichuang-s3.jpg" target="_blank" title="LiChuang ESP32-S3 Development Board">
    <img src="docs/v1/lichuang-s3.jpg" width="240" />
  </a>
  <a href="docs/v1/espbox3.jpg" target="_blank" title="Espressif ESP32-S3-BOX3">
    <img src="docs/v1/espbox3.jpg" width="240" />
  </a>
  <a href="docs/v1/m5cores3.jpg" target="_blank" title="M5Stack CoreS3">
    <img src="docs/v1/m5cores3.jpg" width="240" />
  </a>
  <a href="docs/v1/atoms3r.jpg" target="_blank" title="AtomS3R + Echo Base">
    <img src="docs/v1/atoms3r.jpg" width="240" />
  </a>
  <a href="docs/v1/magiclick.jpg" target="_blank" title="Magic Button 2.4">
    <img src="docs/v1/magiclick.jpg" width="240" />
  </a>
  <a href="docs/v1/waveshare.jpg" target="_blank" title="Waveshare ESP32-S3-Touch-AMOLED-1.8">
    <img src="docs/v1/waveshare.jpg" width="240" />
  </a>
  <a href="docs/v1/lilygo-t-circle-s3.jpg" target="_blank" title="LILYGO T-Circle-S3">
    <img src="docs/v1/lilygo-t-circle-s3.jpg" width="240" />
  </a>
  <a href="docs/v1/xmini-c3.jpg" target="_blank" title="XiaGe Mini C3">
    <img src="docs/v1/xmini-c3.jpg" width="240" />
  </a>
  <a href="docs/v1/movecall-cuican-esp32s3.jpg" target="_blank" title="CuiCan">
    <img src="docs/v1/movecall-cuican-esp32s3.jpg" width="240" />
  </a>
  <a href="docs/v1/wmnologo_xingzhi_1.54.jpg" target="_blank" title="WMnologo-Xingzhi-1.54">
    <img src="docs/v1/wmnologo_xingzhi_1.54.jpg" width="240" />
  </a>
  <a href="docs/v1/sensecap_watcher.jpg" target="_blank" title="SenseCAP Watcher">
    <img src="docs/v1/sensecap_watcher.jpg" width="240" />
  </a>
  <a href="docs/v1/esp-hi.jpg" target="_blank" title="ESP-HI Low Cost Robot Dog">
    <img src="docs/v1/esp-hi.jpg" width="240" />
  </a>
</div>

## Программное обеспечение

### Прошивка и прошивка

Для начинающих рекомендуется использовать прошивку, которую можно прошить без настройки среды разработки.

Прошивка по умолчанию подключается к официальному серверу [xiaozhi.me](https://xiaozhi.me). Личные пользователи могут зарегистрировать аккаунт для бесплатного использования модели Qwen в реальном времени.

👉 [Руководство по прошивке для начинающих](https://ccnphfhqs21z.feishu.cn/wiki/Zpz4wXBtdimBrLk25WdcXzxcnNS)

### Среда разработки

- Cursor или VSCode
- Установите плагин ESP-IDF. Минимальная версия SDK — [ESP-IDF v6.0.1](https://github.com/espressif/esp-idf/releases/tag/v6.0.1); рекомендуется [ESP-IDF v6.1](https://github.com/espressif/esp-idf/releases/tag/v6.1). ESP-IDF 5.x не поддерживается.
- Linux лучше Windows для более быстрой компиляции и меньшего количества проблем с драйверами
- Этот проект использует стиль кода Google C++, пожалуйста, соблюдайте его при отправке кода

### Документация для разработчиков

- [Руководство по кастомным платам](docs/custom-board.md) — как создавать кастомные платы для XiaoZhi AI
- [Использование MCP для управления IoT](docs/mcp-usage.md) — как управлять IoT-устройствами через протокол MCP
- [Поток взаимодействия MCP](docs/mcp-protocol.md) — реализация протокола MCP на устройстве
- [Документ протокола MQTT + UDP](docs/mqtt-udp.md)
- [Подробная документация протокола WebSocket](docs/websocket.md)

## Конфигурация большой модели

Если у вас уже есть устройство чат-бота XiaoZhi AI и оно подключено к официальному серверу, вы можете войти в [консоль xiaozhi.me](https://xiaozhi.me) для настройки.

👉 [Видеоурок по управлению бэкендом (старый интерфейс)](https://www.bilibili.com/video/BV1jUCUY2EKM/)

## Связанные открытые проекты

Для развертывания сервера на персональном компьютере обратитесь к следующим открытым проектам:

- [xinnan-tech/xiaozhi-esp32-server](https://github.com/xinnan-tech/xiaozhi-esp32-server) — Python сервер
- [joey-zhou/xiaozhi-esp32-server-java](https://github.com/joey-zhou/xiaozhi-esp32-server-java) — Java сервер
- [AnimeAIChat/xiaozhi-server-go](https://github.com/AnimeAIChat/xiaozhi-server-go) — Golang сервер
- [hackers365/xiaozhi-esp32-server-golang](https://github.com/hackers365/xiaozhi-esp32-server-golang) — Golang сервер

Другие клиентские проекты, использующие протокол связи XiaoZhi:

- [huangjunsen0406/py-xiaozhi](https://github.com/huangjunsen0406/py-xiaozhi) — Python клиент
- [TOM88812/xiaozhi-android-client](https://github.com/TOM88812/xiaozhi-android-client) — Android клиент
- [100askTeam/xiaozhi-linux](https://github.com/100askTeam/xiaozhi-linux) — Linux клиент от 100ask
- [ZDarow/xiaozhi-sf32](https://github.com/ZDarow/xiaozhi-sf32) — прошивка Bluetooth-чипа от Sichuan
- [QuecPython/solution-xiaozhiAI](https://github.com/QuecPython/solution-xiaozhiAI) — прошивка QuecPython от Quectel

Инструменты кастомных ассетов:

- [ZDarow/xiaozhi-assets-generator](https://github.com/ZDarow/xiaozhi-assets-generator) — Генератор кастомных ассетов (слова активации, шрифты, эмодзи, фоны)

## О проекте

Это открытый проект ESP32, выпущенный под лицензией MIT, которая позволяет использовать его бесплатно всем, включая коммерческие цели.

Надеемся, что этот проект поможет каждому понять разработку ИИ-аппаратного обеспечения и применить стремительно развивающиеся большие языковые модели к реальным аппаратным устройствам.

Если у вас есть идеи или предложения, смело создавайте Issues или присоединяйтесь к нашей [Discord](https://discord.gg/C759fGMBcZ) или QQ группе: 1095994019

## История звёзд

<a href="https://star-history.com/#ZDarow/xiaozhi-esp32&Date">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="https://api.star-history.com/svg?repos=ZDarow/xiaozhi-esp32&type=Date&theme=dark" />
    <source media="(prefers-color-scheme: light)" srcset="https://api.star-history.com/svg?repos=ZDarow/xiaozhi-esp32&type=Date" />
    <img alt="График истории звёзд" src="https://api.star-history.com/svg?repos=ZDarow/xiaozhi-esp32&type=Date" />
  </picture>
</a>