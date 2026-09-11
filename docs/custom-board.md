# Руководство по кастомным платам

Это руководство описывает, как добавить новую плату в проект голосового помощника XiaoZhi AI.
XiaoZhi AI поддерживает 70+ плат на базе ESP32; каждая находится в своей директории под `main/boards/`.

## Важно

> **Внимание**: для кастомной платы, чей IO-конфиг отличается от существующей платы, никогда не
> перезаписывайте конфигурацию исходной платы. Всегда создавайте новый тип платы — либо
> используйте массив `builds` в `config.json`, чтобы получить отдельное имя прошивки с
> разными `sdkconfig`-макросами. Используйте `python scripts/build.py [board-directory]`
> для сборки прошивки.
>
> Перезапись конфигурации существующей платы опасна, потому что OTA-обновления могут заменить
> вашу кастомную прошивку на штатную прошивку для исходной платы. Каждая плата должна иметь
> уникальную идентичность и собственный канал обновлений.

## Структура директории

Директория платы обычно содержит:

- `xxx_board.cc` — инициализация на уровне платы и связующий код.
- `config.h` — назначение пинов и настройки на уровне платы.
- `config.json` — сообщаемый тип платы и конфигурация релизов, потребляемая CMake и `scripts/build.py`.
- `README.md` — плато-специфичные заметки.

Платы могут находиться прямо под `main/boards/` или быть сгруппированы по производителям под
`main/boards/<manufacturer>/<board>/` (см. [Поддиректории производителей](#поддиректории-производителей) ниже).

## Шаги

### 1. Создите директорию платы

Создайте новую директорию под `main/boards/` с именованием `[vendor]-[model]` (например, `m5stack-tab5`):

```bash
mkdir main/boards/my-custom-board
```

### 2. Создайте конфигурационные файлы

#### config.h

Определите все аппаратные настройки в `config.h`:

- Частоты дискретизации аудио и сопоставление пинов I2S.
- Адрес I2C и пины аудиокодека.
- Пины кнопок и LED.
- Параметры дисплея и пины.

Пример (из `lichuang-c3-dev`):

```c
#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

// Аудио
#define AUDIO_INPUT_SAMPLE_RATE  24000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

#define AUDIO_I2S_GPIO_MCLK GPIO_NUM_10
#define AUDIO_I2S_GPIO_WS   GPIO_NUM_12
#define AUDIO_I2S_GPIO_BCLK GPIO_NUM_8
#define AUDIO_I2S_GPIO_DIN  GPIO_NUM_7
#define AUDIO_I2S_GPIO_DOUT GPIO_NUM_11

#define AUDIO_CODEC_PA_PIN       GPIO_NUM_13
#define AUDIO_CODEC_I2C_SDA_PIN  GPIO_NUM_0
#define AUDIO_CODEC_I2C_SCL_PIN  GPIO_NUM_1
#define AUDIO_CODEC_ES8311_ADDR  ES8311_CODEC_DEFAULT_ADDR

// Кнопки
#define BOOT_BUTTON_GPIO        GPIO_NUM_9

// Дисплей
#define DISPLAY_SPI_SCK_PIN     GPIO_NUM_3
#define DISPLAY_SPI_MOSI_PIN    GPIO_NUM_5
#define DISPLAY_DC_PIN          GPIO_NUM_6
#define DISPLAY_SPI_CS_PIN      GPIO_NUM_4

#define DISPLAY_WIDTH   320
#define DISPLAY_HEIGHT  240
#define DISPLAY_MIRROR_X true
#define DISPLAY_MIRROR_Y false
#define DISPLAY_SWAP_XY true

#define DISPLAY_OFFSET_X  0
#define DISPLAY_OFFSET_Y  0

#define DISPLAY_BACKLIGHT_PIN GPIO_NUM_2
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT true

#endif // _BOARD_CONFIG_H_
```

#### config.json

`config.json` определяет совместимый с OTA сообщаемый тип и управляет `scripts/build.py`:

```json
{
    "type": "my-custom-board",
    "target": "esp32s3",
    "builds": [
        {
            "name": "my-custom-board",
            "sdkconfig_append": [
                "CONFIG_ESPTOOLPY_FLASHSIZE_8MB=y",
                "CONFIG_PARTITION_TABLE_CUSTOM_FILENAME=\"partitions/v2/8m.csv\""
            ]
        }
    ]
}
```

**Поля:**
- `type`: совместимый с OTA семейство плат, сообщаемое прошивкой. Держите стабильным после релиза.
- `target`: целевой чип, должен соответствовать реальному оборудованию (`esp32`, `esp32s3`, `esp32c3`, `esp32c6`, `esp32p4`, ...).
- `name`: совместимое с OTA имя варианта прошивки, сообщаемое в релизных сборках; обычно совпадает с `type`.
- `sdkconfig_append`: дополнительные строки sdkconfig, объединяемые с defaults.

И `type`, и `name` должны содержать только строчные буквы, цифры, точки (`.`) и дефисы (`-`).
Подчёркивания, пробелы и заглавные буквы не допускаются.

**Часто используемые записи `sdkconfig_append`:**

```json
// Размер flash
"CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y"
"CONFIG_ESPTOOLPY_FLASHSIZE_8MB=y"

// Таблица разделов
"CONFIG_PARTITION_TABLE_CUSTOM_FILENAME=\"partitions/v2/4m.csv\""
"CONFIG_PARTITION_TABLE_CUSTOM_FILENAME=\"partitions/v2/8m.csv\""

// Аудиопайплайн
"CONFIG_USE_DEVICE_AEC=y"          // включить on-device AEC
```

Проект по умолчанию использует 16 МБ flash и `partitions/v2/16m.csv` на применимых таргетах.
Не повторяйте значения, которые уже совпадают с эффективными defaults проекта и таргета;
используйте `sdkconfig_append` только для реальных плато-специфичных переопределений.

Не выбирайте язык или конкретное слово активации в `config.json` платы. Это параметры сборки
пользователя и должны настраиваться согласованно через `menuconfig` или параметры скрипта
сборки, чтобы CLI, агент и онлайн-сборки могли использовать один и тот же интерфейс.

### 3. Реализуйте класс платы

Создайте `my_custom_board.cc` с реализацией на уровне платы.

Базовый класс платы имеет:

1. **Объявление класса**: наследуйтесь от `WifiBoard` или `Ml307Board`.
2. **Вспомогательные функции инициализации**: I2C, дисплей, кнопки, IoT/MCP-инструменты и т.д.
3. **Виртуальные переопределения**: `GetAudioCodec()`, `GetDisplay()`, `GetBacklight()`, ...
4. **Регистрация платы**: `DECLARE_BOARD(ClassName)`.

```cpp
#include "wifi_board.h"
#include "codecs/es8311_audio_codec.h"
#include "display/lcd_display.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "mcp_server.h"

#include <esp_log.h>
#include <driver/i2c_master.h>
#include <driver/spi_common.h>

#define TAG "MyCustomBoard"

class MyCustomBoard : public WifiBoard {
private:
    i2c_master_bus_handle_t codec_i2c_bus_;
    Button boot_button_;
    LcdDisplay* display_;

    void InitializeI2c() {
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_0,
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &codec_i2c_bus_));
    }

    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_SPI_MOSI_PIN;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.sclk_io_num = DISPLAY_SPI_SCK_PIN;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            app.ToggleChatState();
        });
    }

    void InitializeDisplay() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;

        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_SPI_CS_PIN;
        io_config.dc_gpio_num = DISPLAY_DC_PIN;
        io_config.spi_mode = 2;
        io_config.pclk_hz = 80 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI2_HOST, &io_config, &panel_io));

        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = GPIO_NUM_NC;
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
        panel_config.bits_per_pixel = 16;
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel));

        esp_lcd_panel_reset(panel);
        esp_lcd_panel_init(panel);
        esp_lcd_panel_invert_color(panel, true);
        esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);

        display_ = new SpiLcdDisplay(panel_io, panel,
                                    DISPLAY_WIDTH, DISPLAY_HEIGHT,
                                    DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y,
                                    DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
    }

    void InitializeTools() {
        // Регистрируйте MCP-инструменты здесь; см. docs/mcp-usage.md.
    }

public:
    MyCustomBoard() : boot_button_(BOOT_BUTTON_GPIO) {
        InitializeI2c();
        InitializeSpi();
        InitializeDisplay();
        InitializeButtons();
        InitializeTools();
        GetBacklight()->SetBrightness(100);
    }

    virtual AudioCodec* GetAudioCodec() override {
        static Es8311AudioCodec audio_codec(
            codec_i2c_bus_,
            I2C_NUM_0,
            AUDIO_INPUT_SAMPLE_RATE,
            AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK,
            AUDIO_I2S_GPIO_BCLK,
            AUDIO_I2S_GPIO_WS,
            AUDIO_I2S_GPIO_DOUT,
            AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN,
            AUDIO_CODEC_ES8311_ADDR);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }

    virtual Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
    }
};

DECLARE_BOARD(MyCustomBoard);
```

### 4. Подключение к системе сборки

#### Добавьте запись Kconfig

В `main/Kconfig.projbuild` добавьте запись в блок `choice BOARD_TYPE`:

```kconfig
choice BOARD_TYPE
    prompt "Board Type"
    default BOARD_TYPE_BREAD_COMPACT_WIFI
    help
        Board type.

    # ... другие записи ...

    config BOARD_TYPE_MY_CUSTOM_BOARD
        bool "My Custom Board"
        depends on IDF_TARGET_ESP32S3  # выберите соответствующий таргет
endchoice
```

Примечания:
- Идентификатор должен быть в верхнем регистре с подчёркиваниями.
- `depends on` ограничивает запись соответствующим таргетом (`IDF_TARGET_ESP32S3`, `IDF_TARGET_ESP32C3`, ...).
- Метка может быть локализована.

#### Добавьте ветку в CMakeLists.txt

Откройте `main/CMakeLists.txt` и расширьте цепочку типов плат:

```cmake
elseif(CONFIG_BOARD_TYPE_MY_CUSTOM_BOARD)
    set(BOARD_DIR "my-custom-board")
    set(BUILTIN_TEXT_FONT font_puhui_basic_20_4)     # выберите шрифт для дисплея
    set(BUILTIN_ICON_FONT font_awesome_20_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_64)         # необязательно, для эмодзи
```

**Руководство по шрифтам и эмодзи:**

Выбирайте размер шрифта, соответствующий разрешению дисплея:
- Маленький (128x64 OLED): `font_puhui_basic_14_1` / `font_awesome_14_1`
- Маленький-средний (240x240): `font_puhui_basic_16_4` / `font_awesome_16_4`
- Средний (240x320): `font_puhui_basic_20_4` / `font_awesome_20_4`
- Большой (480x320+): `font_puhui_basic_30_4` / `font_awesome_30_4`

Коллекции эмодзи:
- `twemoji_32` — 32x32 пикселя (маленькие экраны).
- `twemoji_64` — 64x64 пикселя (большие экраны).

### 5. Сборка и прошивка

#### Вариант A — использовать `idf.py` вручную

1. Установите целевой чип (в первый раз, или при смене таргета):
   ```bash
   idf.py set-target esp32s3     # ESP32-S3
   idf.py set-target esp32c3     # ESP32-C3
   idf.py set-target esp32       # ESP32
   ```

2. Очистите устаревшую конфигурацию:
   ```bash
   idf.py fullclean
   ```

3. Выберите плату через menuconfig:
   ```bash
   idf.py menuconfig
   ```
   Перейдите в `Xiaozhi Assistant -> Board Type` и выберите свою плату.

4. Сборка и прошивка:
   ```bash
   idf.py build
   idf.py flash monitor
   ```

#### Вариант B — использовать `build.py` (рекомендуется)

Если директория платы содержит `config.json`, можно автоматически настроить и собрать:

```bash
python scripts/build.py my-custom-board
```

Выбор языка и слова активации — параметры сборки пользователя:

```bash
python scripts/build.py my-custom-board \
  --language en-US \
  --wake-word wn9_jarvis_tts
```

`--language` принимает локаль, перечисленную в `main/assets/locales/`.
`--wake-word` принимает имя модели ESP-SR, `nihaoxiaozhi` (которое выбирает
совместимую модель для таргета), или `disabled`. Целевые ESP32-C3/C5/C6 поддерживают
модели WakeNet9s (`wn9s_*`); сборки ESP32-S3/P4/S31 автоматически используют
движок активации на базе AFE.

Запросите допустимые значения в текстовом или машинно-читаемом виде:

```bash
python scripts/build.py --list-languages
python scripts/build.py --list-languages --json
python scripts/build.py --list-wake-words
python scripts/build.py --list-wake-words --json
```

Список слов активации читается из разрешённого в данный момент компонента ESP-SR, поэтому
сначала выполните `idf.py reconfigure`, если `managed_components/` ещё не заполнена.

Скрипт:
- Выводит справку при запуске без аргументов. Используйте `--list-boards`, чтобы перечислить
  типы и варианты плат.
- Запрашивает вариант, когда выбранная плата имеет несколько сборок. В неинтерактивных
  средах передайте `--name <variant>`.
- Читает `target` из `config.json`, очищает существующую директорию сборки только
  при изменении таргета, затем настраивает таргет, имя платы, defaults и `sdkconfig_append`
  выбранной сборки в одном вызове `idf.py reconfigure`. Последующий `idf.py build`
  переиспользует эту конфигурацию.
- Передаёт `name` выбранной сборки как сообщаемое имя варианта прошивки.
- Собирает `build/merged-binary.bin` без создания ZIP по умолчанию. Передайте `--zip`,
  чтобы воссоздать `releases/v<version>_<name>.zip`.

### 6. Напишите README

В `README.md` опишите плату, аппаратные требования, инструкции по сборке и любые специальные заметки.

## Поддиректории производителей

Платы могут быть сгруппированы по производителям под `main/boards/<manufacturer>/<board>/`.
Это рекомендуемый макет, когда один поставщик поставляет несколько вариантов — например,
`main/boards/waveshare/esp32-p4-nano/` или `main/boards/lceda-course-examples/eda-tv-pro/`.

Для платы в поддиректории производителя добавьте то же значение в `config.json`,
например `"manufacturer": "waveshare"`. Прошивка сообщает его как `board.manufacturer`
вместе с `board.type` и `board.name`. Плоские community-платы без этого поля сообщают
пустую строку manufacturer.

Установите `BOARD_DIR` на полный путь относительно `main/boards/`:

```cmake
elseif(CONFIG_BOARD_TYPE_WAVESHARE_ESP32_P4_NANO)
    set(BOARD_DIR "waveshare/esp32-p4-nano")
    set(BUILTIN_TEXT_FONT font_puhui_basic_30_4)
    set(BUILTIN_ICON_FONT font_awesome_30_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_64)
```

Система сборки загружает источники из `main/boards/${BOARD_DIR}/` и читает сообщаемый тип
платы из `config.json` этой директории. Если `config.json` или его верхнеуровневый `type`
отсутствуют, полный `BOARD_DIR` с заменой `/` на `-` используется как fallback-тип.

Правила:
- Используйте макет производителя, когда есть две и более платы от одного и того же
  поставщика, которые разделяют драйверы, ассеты или документацию.
- Используйте плоский макет для одиночных плат и community-примеров.
- Имена директорий — строчные с дефисами (например, `waveshare`, `lceda-course-examples`).

## Общие компоненты плат

Несколько переиспользуемых компонентов находятся в `main/boards/common/`. Их можно включить
прямо из класса платы:

### Драйверы дисплеев

Поддерживаемые семейства LCD:
- ST7789 (SPI)
- ILI9341 (SPI)
- SH8601 (QSPI)
- и многие другие.

### Аудиокодеки

- `Es8311AudioCodec` (самый распространённый)
- `Es8374AudioCodec`
- `Es8388AudioCodec`
- `Es8389AudioCodec`
- `BoxAudioCodec` (комбо микрофонного массива ES7210 + кодек, используемое на платах ESP-Box)
- `NoAudioCodec` (прямой I2S без внешнего кодека)
- `DummyAudioCodec` (заглушка для плат без аудио)

### Управление питанием

- Вспомогательные функции PMIC `Axp2101`.
- Вспомогательные функции зарядного устройства `Sy6970`.
- `AdcBatteryMonitor` — простой монитор напряжия батареи на базе АЦП.
- `PowerSaveTimer` / `SleepTimer` — вспомогательные функции для планирования light-sleep.

### Сеть

- `WifiBoard` — базовый класс для Wi-Fi.
- `Ml307Board` / `Nt26Board` — базовые классы для 4G-модемов.
- `DualNetworkBoard` — переключаемый базовый класс Wi-Fi / 4G.
- `RndisBoard` — сеть RNDIS-over-USB (ESP32-S3 / ESP32-P4).
- Вспомогательные функции `EspVideo` для ESP-Video на ESP32-S3 / ESP32-P4.

### Вспомогательные функции ввода

- `Button` — стандартные кнопки (клик, долгое нажатие, многократные клики).
- `Knob` — обёртка для энкодера.
- `PressToTalkMcpTool` — инструмент push-to-talk, регистрирующийся через MCP.
- `SystemReset` — вспомогательная функция, выполняющая безопасный factory reset при удержании кнопки при загрузке.

### Интеграция MCP

Любая плата может регистрировать кастомные инструменты — управление динамиком, яркость экрана,
чтение батареи, управление светом и т.д. См. [Использование MCP для IoT-управления](./mcp-usage.md).

## Иерархия классов плат

- `Board` — базовый класс
  - `WifiBoard` — плата с Wi-Fi
  - `Ml307Board` / `Nt26Board` — платы с 4G-модемом
  - `DualNetworkBoard` — переключаемая плата Wi-Fi + 4G
  - `RndisBoard` — плата RNDIS-over-USB

## Советы

1. **Начинайте с похожей платы** — копирование и доработка существующей платы обычно быстрее,
   чем создание с нуля.
2. **Поднимайте поэтапно** — сначала запустите дисплей, затем аудио, затем весь стек.
3. **Дважды проверьте назначение пинов** — каждый пин, определённый в `config.h`, должен
   соответствовать вашей схеме.
4. **Проверьте аппаратную совместимость** — особенно комбинации кодека / PMIC / контроллера
   тачскрина.

## Устранение неполадок

1. **Дисплей отображается неправильно** — проверьте SPI-конфигурацию, зеркалирование и
   инверсию цвета.
2. **Нет аудио** — проверьте I2S-проводку, пин включения УУ, адрес I2C кодека.
3. **Не удаётся подключиться к Wi-Fi** — перепроверьте учётные данные Wi-Fi и метод провайдинга.
4. **Не удаётся достичь сервера** — проверьте конфигурацию конечных точек WebSocket / MQTT.

## Ссылки

- ESP-IDF документация: https://docs.espressif.com/projects/esp-idf/
- LVGL документация: https://docs.lvgl.io/
- ESP-SR документация: https://github.com/espressif/esp-sr