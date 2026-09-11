# Использование MCP для IoT-управления

> Этот документ описывает, как реализовать IoT-управление для устройств ESP32 с помощью протокола MCP.
> Для подробного протокола передачи данных см. [`mcp-protocol.md`](./mcp-protocol.md).

## Введение

MCP (Model Context Protocol) — рекомендуемый протокол IoT-управления в этом проекте. Он использует
JSON-RPC 2.0, чтобы бэкенд мог обнаруживать и вызывать «инструменты», зарегистрированные устройством,
предоставляя гибкий способ предоставления функциональности устройства.

## Типичный поток

1. Устройство загружается и подключается к бэкенду по WebSocket или MQTT.
2. Бэкенд отправляет вызов `initialize` для запуска сессии MCP.
3. Бэкенд выдаёт `tools/list`, чтобы обнаружить доступные инструменты и их схемы ввода.
4. Бэкенд вызывает отдельные инструменты с помощью `tools/call`, чтобы управлять устройством.

См. [`mcp-protocol.md`](./mcp-protocol.md) для точного формата сообщений.

## Регистрация инструментов на устройстве

Инструменты регистрируются через синглтон `McpServer`. Есть два API регистрации:

- `McpServer::AddTool` — обычный инструмент, видимый в ответе по умолчанию `tools/list` и вызываемый ИИ.
- `McpServer::AddUserOnlyTool` — скрытый инструмент, возвращаемый только тогда, когда бэкенд запрашивает инструменты с `withUserTools=true`. Используйте для привилегированных или инициированных пользователем действий (перезагрузка, обновление прошивки, снимки экрана и т.д.), которые не должны вызываться автономно ИИ.

Оба API имеют одинаковую сигнатуру:

```cpp
void AddTool(
    const std::string& name,         // уникальное имя инструмента, напр. self.dog.forward
    const std::string& description,  // краткое описание для модели
    const PropertyList& properties,  // входные параметры (могут быть пустыми); поддерживаемые типы: bool, int, string
    std::function<ReturnValue(const PropertyList&)> callback // реализация
);

void AddUserOnlyTool(
    const std::string& name,
    const std::string& description,
    const PropertyList& properties,
    std::function<ReturnValue(const PropertyList&)> callback
);
```

- `name` — уникальный идентификатор. Стиль именования `module.action` работает хорошо.
- `description` — описание на естественном языке; используется ИИ для принятия решения о вызове.
- `properties` — входные параметры. Поддерживаемые типы свойств: boolean, integer, string, с необязательными минимум/максимум и значениями по умолчанию.
- `callback` — реализация. Возвращаемые значения могут быть `bool`, `int` или `std::string`.

## Пример (ESP-Hi)

```cpp
void InitializeTools() {
    auto& mcp_server = McpServer::GetInstance();

    // Пример 1: без аргументов — движение робота вперёд
    mcp_server.AddTool("self.dog.forward",
        "Move the robot forward",
        PropertyList(),
        [this](const PropertyList&) -> ReturnValue {
            servo_dog_ctrl_send(DOG_STATE_FORWARD, NULL);
            return true;
        });

    // Пример 2: с аргументами — установка цвета RGB-света
    mcp_server.AddTool("self.light.set_rgb",
        "Set the RGB color of the light",
        PropertyList({
            Property("r", kPropertyTypeInteger, 0, 255),
            Property("g", kPropertyTypeInteger, 0, 255),
            Property("b", kPropertyTypeInteger, 0, 255)
        }),
        [this](const PropertyList& properties) -> ReturnValue {
            int r = properties["r"].value<int>();
            int g = properties["g"].value<int>();
            int b = properties["b"].value<int>();
            led_on_ = true;
            SetLedColor(r, g, b);
            return true;
        });
}
```

## Пример — регистрация пользовательского инструмента

```cpp
mcp_server.AddUserOnlyTool("self.display.clear_cache",
    "Clear locally cached images. User-only action.",
    PropertyList(),
    [](const PropertyList&) -> ReturnValue {
        ClearLocalCache();
        return true;
    });
```

Инструмент, зарегистрированный таким образом, не появится в обычном ответе `tools/list`.
Бэкенд должен установить `params.withUserTools = true`, чтобы увидеть его.

## Встроенные инструменты

`McpServer::AddCommonTools` и `McpServer::AddUserOnlyTools` автоматически регистрируют ряд инструментов:

### Обычные инструменты (вызываемые ИИ) — из `AddCommonTools`

| Инструмент | Описание |
|------|-------------|
| `self.get_device_status` | Возвращает текущую громкость, экран, батарею, сеть и т.д. |
| `self.audio_speaker.set_volume` | Установить громкость динамика (`volume`: 0-100). |
| `self.screen.set_brightness` | Установить яркость экрана при наличии подсветки (`brightness`: 0-100). |
| `self.screen.set_theme` | Переключить тему UI (`theme`: `"light"` или `"dark"`), когда включён LVGL. |
| `self.camera.take_photo` | Сделать фото с встроенной камеры (если плата её имеет) и ответить на заданный `question` по ней. |

Специфичные для платы инструменты добавляются после этих в `InitializeTools()` каждой платы.

### Пользовательские инструменты — из `AddUserOnlyTools`

Эти инструменты скрыты по умолчанию. Бэкенд должен передать `withUserTools=true` в `tools/list`, чтобы увидеть их. Они предназначены для приложений-компаньонов / конечных пользователей, а не для ИИ.

| Инструмент | Описание |
|------|-------------|
| `self.get_system_info` | Возвращает JSON-объект с описанием системы. |
| `self.reboot` | Перезагружает устройство через короткую задержку. |
| `self.upgrade_firmware` | Скачивает прошивку с `url` и устанавливает, затем перезагружается. |
| `self.screen.get_info` | Возвращает текущую ширину, высоту экрана и признак монохромности (только LVGL-платы). |
| `self.screen.snapshot` | Делает снимок экрана в JPEG и загружает на `url` (LVGL-платы, когда `CONFIG_LV_USE_SNAPSHOT=y`). |
| `self.screen.preview_image` | Скачивает и отображает изображение с `url` на экране. |
| `self.assets.set_download_url` | Устанавливает URL загрузки для раздела ассетов. |

## Примеры JSON-RPC

### 1. Получить список инструментов

```json
{
  "jsonrpc": "2.0",
  "method": "tools/list",
  "params": { "cursor": "", "withUserTools": false },
  "id": 1
}
```

### 2. Движение шасси вперёд

```json
{
  "jsonrpc": "2.0",
  "method": "tools/call",
  "params": {
    "name": "self.chassis.go_forward",
    "arguments": {}
  },
  "id": 2
}
```

### 3. Переключение режима света

```json
{
  "jsonrpc": "2.0",
  "method": "tools/call",
  "params": {
    "name": "self.chassis.switch_light_mode",
    "arguments": { "light_mode": 3 }
  },
  "id": 3
}
```

### 4. Перезагрузка устройства (пользовательский инструмент)

```json
{
  "jsonrpc": "2.0",
  "method": "tools/call",
  "params": {
    "name": "self.reboot",
    "arguments": {}
  },
  "id": 4
}
```

## Примечания

- Имена инструментов, параметры и возвращаемые значения должны совпадать с тем, что устройство регистрирует через `AddTool` / `AddUserOnlyTool`.
- Предпочитайте MCP для любого нового IoT-управления.
- Для протокола передачи данных и продвинутых тем см. [`mcp-protocol.md`](./mcp-protocol.md).