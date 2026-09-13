# Руководство по вкладу

## Установка окружения

### ESP-IDF
```bash
# Скачать и установить ESP-IDF v6.1
git clone --depth 1 https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh
. ./export.sh
```

### Проверка версии
```bash
idf.py --version
# Должно быть >= 6.0.1
```

## Сборка проекта

### Канонический способ (рекомендуется)
```bash
python3 scripts/build.py <директория-платы> --name <имя-варианта>
```

### Пример
```bash
python3 scripts/build.py main/boards/espressif/esp32s3 --name esp32s3
```

## Тестирование

### Host-тесты
```bash
python3 -m unittest discover -s scripts/tests -v
```

### Форматирование кода
```bash
clang-format -i <файлы>
clang-format --dry-run -Werror <файлы>
```

## Правила коммитов

- Русский язык, повелительное наклонение
- Формат: `<тип>: <краткое описание>`
- Типы: `feat`, `fix`, `refactor`, `docs`, `test`, `chore`, `style`, `perf`, `ci`
- Длина заголовка ≤ 72 символа

## Названия веток

- Английский язык, kebab-case
- Формат: `feature/<what>`, `fix/<what>`, `docs/<what>`, `chore/<what>`

## Pull Request

- Описание на русском
- Чек-лист: что сделано, как тестировалось, breaking changes
- Ссылки на связанные задачи