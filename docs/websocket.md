# Протокол WebSocket-связи

Этот документ описывает протокол WebSocket-связи между устройством и сервером, основанный на
текущем коде. При реализации сервера проверяйте всё с реальной реализацией.

---

## 1. Общий поток

1. **Инициализация устройства**
   - Устройство загружается и инициализирует `Application`:
     - Инициализирует аудиокодек, дисплей, светодиоды и т.д.
     - Подключается к сети.
     - Создаёт экземпляр протокола WebSocket (`WebsocketProtocol`), реализующий интерфейс `Protocol`.
   - Входит в главный цикл и ждёт событий (аудиоввод, аудиовывод, отложенные задачи и т.д.).

2. **Открытие WebSocket-соединения**
   - Когда устройство должно начать голосовую сессию (активация, нажатие кнопки и т.д.), оно вызывает `OpenAudioChannel()`:
     - Читает URL WebSocket из настроек.
     - Устанавливает заголовки запроса (`Authorization`, `Protocol-Version`, `Device-Id`, `Client-Id`).
     - Вызывает `Connect()` для установки WebSocket-соединения.

3. **Устройство отправляет сообщение "hello"**
   - После подключения устройство отправляет JSON-сообщение. Пример:
   ```json
   {
     "type": "hello",
     "version": 1,
     "features": {
       "mcp": true,
       "aec": true,
       "glyph_push": true
     },
     "text_font": {
       "bundle": "noto-v1",
       "charset": "common",
       "size": 20,
       "bpp": 4
     },
     "transport": "websocket",
     "audio_params": {
       "format": "opus",
       "sample_rate": 16000,
       "channels": 1,
       "frame_duration": 60
     }
   }
   ```
   - `features` необязательно и генерируется из конфигурации времени компиляции. Например, `"mcp": true` означает поддержку MCP, а `"aec": true` отправляется, когда включён `CONFIG_USE_SERVER_AEC`.
   - `"glyph_push": true` и `text_font` рекламируют необязательное расширение динамической подачи глифов текста. См. [Расширение динамической подачи глифов текста](glyph-push.md).
   - `frame_duration` соответствует `OPUS_FRAME_DURATION_MS` (обычно 60 мс).

4. **Сервер отвечает "hello"**
   - Устройство ждёт JSON-сообщение, где `"type"` равно `"hello"`, а `"transport"` равно `"websocket"`.
   - Сервер может включить `session_id`; устройство сохраняет его.
   - Пример:
   ```json
   {
     "type": "hello",
     "transport": "websocket",
     "session_id": "xxx",
     "audio_params": {
       "format": "opus",
       "sample_rate": 24000,
       "channels": 1,
       "frame_duration": 60
     }
   }
   ```
   - Если `transport` совпадает, устройство помечает аудиоканал как открытый.
   - Если допустимый hello не приходит в течение таймаута (по умолчанию 10 секунд), соединение считается неудавшимся, и вызывается обратный вызов сетевой ошибки.

5. **Последующие обмены**
   - Два типа данных отправляются в обоих направлениях:
     1. **Бинарные аудиоданные** (Opus-кодированные)
     2. **Текстовые JSON-сообщения** (состояние чата, события TTS/STT, сообщения MCP и т.д.)

   - В коде обратный вызов приёма разделяет трафик следующим образом:
     - `OnData(...)`:
       - Если `binary` равно `true`, полезные данные обрабатываются как кадр Opus и декодируются.
       - Если `binary` равно `false`, полезные данные разбираются как JSON и диспатчатся по `type`.

   - Когда сервер или сеть разрывают соединение, вызывается `OnDisconnected()`:
     - Устройство вызывает `on_audio_channel_closed_()` и в конечном итоге возвращается в состояние простоя.

6. **Закрытие WebSocket-соединения**
   - Когда устройство хочет завершить сессию, оно вызывает `CloseAudioChannel()` для разрыва сокета и возвращается в состояние простоя.
   - Та же цепочка обратных вызовов выполняется, если сервер закрывает сокет первым.

---

## 2. Общие заголовки запросов

При установке WebSocket-соединения устройство устанавливает следующие заголовки:

- `Authorization`: токен доступа, обычно в формате `"Bearer <token>"`.
- `Protocol-Version`: номер версии протокола, соответствующий полю `version` в сообщении hello.
- `Device-Id`: физический MAC-адрес устройства.
- `Client-Id`: UUID, сгенерированный программно (сбрасывается при стирании NVS или полной перепрошивке).

Эти заголовки отправляются с WebSocket-рукопожатием; сервер может использовать их для аутентификации или бухгалтерского учёта.

---

## 3. Версии бинарного протокола

Устройство поддерживает несколько версий бинарного протокола, выбираемых полем `version` в настройках:

### 3.1 Версия 1 (по умолчанию)
Сырые кадры Opus без дополнительных метаданных. Уровень WebSocket уже различает текстовые и бинарные кадры.

### 3.2 Версия 2
Использует структуру `BinaryProtocol2`:
```c
struct BinaryProtocol2 {
    uint16_t version;        // версия протокола
    uint16_t type;           // тип сообщения (0: OPUS, 1: JSON)
    uint32_t reserved;       // зарезервировано
    uint32_t timestamp;      // временная метка в миллисекундах (полезно для серверного AEC)
    uint32_t payload_size;   // размер полезной нагрузки в байтах
    uint8_t payload[];       // полезные данные
} __attribute__((packed));
```

### 3.3 Версия 3
Использует структуру `BinaryProtocol3`:
```c
struct BinaryProtocol3 {
    uint8_t type;            // тип сообщения
    uint8_t reserved;        // зарезервировано
    uint16_t payload_size;   // размер полезной нагрузки
    uint8_t payload[];       // полезные данные
} __attribute__((packed));
```

---

## 4. Структура JSON-сообщений

Текстовые кадры WebSocket несут JSON. Наиболее распространённые значения `"type"` и их семантика перечислены ниже. Поля, не указанные здесь, могут быть специфичны для реализации или необязательны.

### 4.1 Устройство -> Сервер

1. **Hello**
   - Отправляется после установки соединения; объявляет параметры устройства.
   - Пример:
     ```json
     {
       "type": "hello",
       "version": 1,
       "features": {
         "mcp": true,
         "aec": true
       },
       "transport": "websocket",
       "audio_params": {
         "format": "opus",
         "sample_rate": 16000,
         "channels": 1,
         "frame_duration": 60
       }
     }
     ```

2. **Listen**
   - Сообщает серверу, что устройство начинает или останавливает захват микрофона.
   - Общие поля:
     - `"session_id"`: идентификатор сессии.
     - `"type": "listen"`
     - `"state"`: `"start"`, `"stop"` или `"detect"` (слово активации обнаружено).
     - `"mode"`: `"auto"`, `"manual"` или `"realtime"`.
   - Пример (начало прослушивания):
     ```json
     {
       "session_id": "xxx",
       "type": "listen",
       "state": "start",
       "mode": "manual"
     }
     ```

3. **Abort**
   - Прерывает текущее воспроизведение TTS или голосовой канал.
   - Пример:
     ```json
     {
       "session_id": "xxx",
       "type": "abort",
       "reason": "wake_word_detected"
     }
     ```
   - `reason` может быть `"wake_word_detected"` или другими значениями, определёнными реализацией.

4. **Wake Word Detected**
   - Отправляется устройством, когда локальный детектор слов активации срабатывает.
   - Аудио с вокалом может быть потоково отправлено до этого сообщения, чтобы сервер мог выполнять проверку голосового биометрического профиля.
   - Пример:
     ```json
     {
       "session_id": "xxx",
       "type": "listen",
       "state": "detect",
       "text": "Hi XiaoZhi"
     }
     ```

5. **MCP**
   - Рекомендуемый канал для IoT-управления. Обнаружение возможностей устройства и вызовы инструментов все идут через сообщения `type: "mcp"`, чей `payload` — JSON-RPC 2.0 (см. [документ протокола MCP](./mcp-protocol.md)).
   - Пример ответа устройства -> сервер:
     ```json
     {
       "session_id": "xxx",
       "type": "mcp",
       "payload": {
         "jsonrpc": "2.0",
         "id": 1,
         "result": {
           "content": [
             { "type": "text", "text": "true" }
           ],
           "isError": false
         }
       }
     }
     ```

---

### 4.2 Сервер -> Устройство

1. **Hello**
   - Подтверждение рукопожатия.
   - Должно включать `"type": "hello"` и `"transport": "websocket"`.
   - Может включать `audio_params`, означающие аудиопараметры, которые ожидает сервер / канонический набор, согласованный с устройством.
   - Может включать `session_id`, который устройство записывает.
   - После получения устройство устанавливает событие «аудиоканал открыт».

2. **STT**
   - `{"session_id": "xxx", "type": "stt", "text": "..."}`
   - Результат распознавания речи для высказывания пользователя. Обычно отображается на дисплее перед переходом к ответу.

3. **LLM**
   - `{"session_id": "xxx", "type": "llm", "emotion": "happy", "text": "😀"}`
   - Сообщает устройству обновить эмоцию / выражение лица в UI.

4. **TTS**
   - `{"session_id": "xxx", "type": "tts", "state": "start"}`: сервер собирается потоково отправлять аудио TTS. Устройство переходит в состояние воспроизведения.
   - `{"session_id": "xxx", "type": "tts", "state": "stop"}`: сегмент TTS завершён.
   - `{"session_id": "xxx", "type": "tts", "state": "sentence_start", "text": "..."}`: отобразить текущее предложение в UI (например, субтитры).

5. **MCP**
   - Сервер отправляет IoT-команды или получает результаты вызовов инструментов. Структура `payload` следует JSON-RPC 2.0.
   - Пример `tools/call` сервера -> устройству:
     ```json
     {
       "session_id": "xxx",
       "type": "mcp",
       "payload": {
         "jsonrpc": "2.0",
         "method": "tools/call",
         "params": {
           "name": "self.light.set_rgb",
           "arguments": { "r": 255, "g": 0, "b": 0 }
         },
         "id": 1
       }
     }
     ```

6. **System**
   - Системное управление, часто используется для удалённого обновления / управления.
   - Пример:
     ```json
     {
       "session_id": "xxx",
       "type": "system",
       "command": "reboot"
     }
     ```
   - Поддерживаемые команды:
     - `"reboot"`: перезагрузить устройство.

7. **Alert**
   - Указывает устройству показать оповещение и воспроизвести звук вибрации. Обрабатывается в `Application::OnIncomingJson`.
   - Пример:
     ```json
     {
       "session_id": "xxx",
       "type": "alert",
       "status": "Warning",
       "message": "Battery low",
       "emotion": "sad"
     }
     ```
   - Поля:
     - `status`: краткое заголовок, отображаемый на экране.
     - `message`: подробное сообщение.
     - `emotion`: эмоция, отображаемая во время оповещения (например, `"sad"`, `"neutral"`).

8. **Custom** (необязательно)
   - Доступно, когда включено `CONFIG_RECEIVE_CUSTOM_MESSAGE`.
   - Пример:
     ```json
     {
       "session_id": "xxx",
       "type": "custom",
       "payload": {
         "message": "anything you want"
       }
     }
     ```

9. **Binary audio frames**
   - Когда сервер отправляет Opus-кодированные аудиоданные в бинарных кадрах, устройство декодирует и воспроизводит их.
   - Кадры, полученные во время состояния `listening`, отбрасываются, чтобы избежать конфликтов с микрофонным потоком.

---

## 5. Аудиокодек

1. **Устройство загружает аудио с микрофона**
   - После необязательной обработки AEC / NR / AGC аудио кодируется в Opus и отправляется в бинарных кадрах.
   - В зависимости от версии протокола кадры могут быть сырыми Opus (v1) или упакованными в метаданные (v2/v3).

2. **Устройство воспроизводит аудио сервера**
   - Входящие бинарные кадры также обрабатываются как Opus.
   - Устройство декодирует их и отправляет на аудиовыход.
   - Если частота дискретизации отличается от выходной частоты устройства, она пересэмпливается после декодирования.

---

## 6. Состояния устройства

### 6.1 Основные состояния

Конечный автомат состояний устройства определён в [`main/device_state.h`](../main/device_state.h) и включает:

- `kDeviceStateUnknown`
- `kDeviceStateStarting`
- `kDeviceStateWifiConfiguring`
- `kDeviceStateIdle`
- `kDeviceStateConnecting`
- `kDeviceStateListening`
- `kDeviceStateSpeaking`
- `kDeviceStateUpgrading`
- `kDeviceStateActivating`
- `kDeviceStateAudioTesting`    (заводское / проверка аудио)
- `kDeviceStateFatalError`      (некритическая ошибка, требующая действий пользователя)

### 6.2 Типичные переходы

1. **Idle -> Connecting**
   - Инициируется словом активации или нажатием кнопки. Устройство вызывает `OpenAudioChannel()`, настраивает WebSocket и отправляет `"type":"hello"`.

2. **Connecting -> Listening**
   - После подключения вызывается `SendStartListening(...)` и начинается потоковая передача микрофона.

3. **Listening -> Speaking**
   - Сервер отправляет `{"type":"tts","state":"start"}`; устройство останавливает отправку микрофонного аудио и воспроизводит входящий TTS.

4. **Speaking -> Idle**
   - Сервер отправляет `{"type":"tts","state":"stop"}`. При включённом автопродолжении устройство возвращается в Listening; иначе — в Idle.

5. **Listening / Speaking -> Idle** (abort)
   - `SendAbortSpeaking(...)` или `CloseAudioChannel()` прерывают сессию и закрывают WebSocket.

### 6.3 Диаграмма состояний в авто-режиме

```mermaid
stateDiagram
  direction TB
  [*] --> kDeviceStateUnknown
  kDeviceStateUnknown --> kDeviceStateStarting: Initialize
  kDeviceStateStarting --> kDeviceStateWifiConfiguring: Configure WiFi
  kDeviceStateStarting --> kDeviceStateActivating: Activate device
  kDeviceStateActivating --> kDeviceStateUpgrading: New firmware detected
  kDeviceStateActivating --> kDeviceStateIdle: Activation complete
  kDeviceStateIdle --> kDeviceStateConnecting: Start connecting
  kDeviceStateConnecting --> kDeviceStateIdle: Connection failed
  kDeviceStateConnecting --> kDeviceStateListening: Connection succeeded
  kDeviceStateListening --> kDeviceStateSpeaking: TTS start
  kDeviceStateSpeaking --> kDeviceStateListening: TTS stop
  kDeviceStateListening --> kDeviceStateIdle: Manual abort
  kDeviceStateSpeaking --> kDeviceStateIdle: Auto stop
  kDeviceStateStarting --> kDeviceStateAudioTesting: Factory audio test
  kDeviceStateStarting --> kDeviceStateFatalError: Fatal error
```

### 6.4 Диаграмма состояний в ручном режиме

```mermaid
stateDiagram
  direction TB
  [*] --> kDeviceStateUnknown
  kDeviceStateUnknown --> kDeviceStateStarting: Initialize
  kDeviceStateStarting --> kDeviceStateWifiConfiguring: Configure WiFi
  kDeviceStateStarting --> kDeviceStateActivating: Activate device
  kDeviceStateActivating --> kDeviceStateUpgrading: New firmware detected
  kDeviceStateActivating --> kDeviceStateIdle: Activation complete
  kDeviceStateIdle --> kDeviceStateConnecting: Start connecting
  kDeviceStateConnecting --> kDeviceStateIdle: Connection failed
  kDeviceStateConnecting --> kDeviceStateListening: Connection succeeded
  kDeviceStateIdle --> kDeviceStateListening: Start listening
  kDeviceStateListening --> kDeviceStateIdle: Stop listening
  kDeviceStateIdle --> kDeviceStateSpeaking: Start speaking
  kDeviceStateSpeaking --> kDeviceStateIdle: Stop speaking
```

---

## 7. Обработка ошибок

1. **Ошибка подключения**
   - Если `Connect(url)` не удался или серверный hello не получен до таймаута, вызывается `on_network_error_()` и устройство показывает оповещение «не удалось подключиться».

2. **Отключение сервера**
   - Если WebSocket неожиданно разрывается, вызывается `OnDisconnected()`:
     - Выполняется `on_audio_channel_closed_()`.
     - Устройство возвращается в Idle (или повторяет попытку, в зависимости от политики).

---

## 8. Прочие замечания

1. **Аутентификация**
   - Устройство передаёт `Authorization: Bearer <token>`; сервер должен его проверять.
   - Если токен отсутствует или недействителен, сервер может отклонить рукопожатие или прекратить сессию позже.

2. **Объём сессии**
   - Многие сообщения содержат `session_id`, полезный, когда сервер обслуживает несколько параллельных взаимодействий.

3. **Аудиополезные данные**
   - Формат аудио по умолчанию — Opus при 16 кГц, моно. Длительность кадра управляется `OPUS_FRAME_DURATION_MS` (обычно 60 мс). Сервер может использовать 24 кГц на downstream для лучшего воспроизведения музыки.

4. **Выбор версии бинарного протокола**
   - Настраивается через параметр `version`:
     - v1: сырой Opus
     - v2: метаданные + временная метка (полезно для серверного AEC)
     - v3: лёгкий заголовок
   - Значение отражается в заголовке `Protocol-Version` и сообщении hello.

5. **IoT-управление через MCP**
   - Все обнаружение возможностей IoT и управление идут через MCP (`type: "mcp"`). Устаревший протокол `type: "iot"` устарел.
   - MCP работает как по WebSocket, так и по MQTT, обеспечивая лучшую стандартизацию и расширяемость.
   - Подробности в [документе протокола MCP](./mcp-protocol.md) и [использовании MCP для IoT](./mcp-usage.md).

6. **Некорректный JSON**
   - Когда обязательное поле, такое как `type`, отсутствует, устройство логирует `ESP_LOGE(TAG, "Missing message type, data: %s", data);` и игнорирует сообщение.

---

## 9. Пример потока сообщений

Упрощённый двусторонний обмен:

1. **Устройство -> Сервер** (рукопожатие)
   ```json
   {
     "type": "hello",
     "version": 1,
     "features": {
       "mcp": true,
       "aec": true
     },
     "transport": "websocket",
     "audio_params": {
       "format": "opus",
       "sample_rate": 16000,
       "channels": 1,
       "frame_duration": 60
     }
   }
   ```

2. **Сервер -> Устройство** (подтверждение рукопожатия)
   ```json
   {
     "type": "hello",
     "transport": "websocket",
     "session_id": "xxx",
     "audio_params": {
       "format": "opus",
       "sample_rate": 16000
     }
   }
   ```

3. **Устройство -> Сервер** (начало прослушивания)
   ```json
   {
     "session_id": "xxx",
     "type": "listen",
     "state": "start",
     "mode": "auto"
   }
   ```
   Устройство начинает потоковую передачу бинарных кадров Opus.

4. **Сервер -> Устройство** (результат ASR)
   ```json
   {
     "session_id": "xxx",
     "type": "stt",
     "text": "what the user said"
   }
   ```

5. **Сервер -> Устройство** (начало TTS)
   ```json
   {
     "session_id": "xxx",
     "type": "tts",
     "state": "start"
   }
   ```
   Сервер продолжает отправкой бинарных кадров Opus для воспроизведения устройством.

6. **Сервер -> Устройство** (конец TTS)
   ```json
   {
     "session_id": "xxx",
     "type": "tts",
     "state": "stop"
   }
   ```
   Устройство останавливает воспроизведение и, если не поступит дальнейших указаний, возвращается в состояние простоя.

---

## 10. Краткое содержание

Этот протокол переносит текстовые JSON-сообщения и бинарные кадры Opus по WebSocket-соединению для реализации потоковой передачи аудио, воспроизведения TTS, распознавания речи, управления состоянием устройства, диспатча MCP и многого другого. Ключевые особенности:

- **Рукопожатие**: отправьте `"type":"hello"` и дождитесь ответа сервера.
- **Аудиоканал**: двунаправленный поток Opus с тремя вариантами бинарного кадрирования.
- **JSON-сообщения**: диспатч по `"type"` (TTS, STT, MCP, WakeWord, System, Alert, Custom, ...).
- **Расширяемость**: дополнительные поля в JSON, дополнительные заголовки для аутентификации.

Сервер и устройство должны согласовать значение, тайминг и обработку ошибок каждого типа сообщения, чтобы сессия работала гладко. Описанный выше текст обеспечивает базу для интеграции, отладки и расширения.