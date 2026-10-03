# Настройка рабочей области и VS Code

Документ описывает минимально необходимый набор инструментов для работы с репозиторием
`xiaozhi-esp32` (ESP-IDF, C++23, Python-туллинг) и команды CLI для отключения или
удаления нерелевантных инструментов в этой рабочей области.

Проверено: VS Code 1.140.0, Linux Mint 22.3, локальный ESP-IDF 6.0.2, дата 03.10.2026.

## 1. Обязательный минимум

| Инструмент | Версия | Проверка |
|------------|--------|----------|
| ESP-IDF | >= 6.0.1 (рекоменд. 6.1; локально 6.0.2) | `source /home/mi/.espressif/v6.0.2/esp-idf/export.sh && idf.py --version` |
| Python | 3.x (только стандартная библиотека) | `python3 -V` |
| clang-format | 18 (Google, ColumnLimit 100) | `clang-format --version` |
| CMake | >= 3.16 | `cmake --version` |
| Docker | любой (сборщик `docker/firmware-builder`) | `docker --version` |

Проект **не** использует: Java/Kotlin/Gradle/Maven, .NET, Node.js/npm, ESLint/Prettier,
Black/Ruff/mypy/pylance, Conan, PlatformIO, Arduino IDE. Пакетных менеджеров Python
(`pyproject.toml`, `requirements.txt` для основного туллинга) в корне нет — зависимости
не требуются, скрипты используют stdlib.

## 2. Расширения VS Code: рекомендуемые

Уже присутствуют в `.vscode/extensions.json`:

| Расширение | Роль в проекте |
|------------|----------------|
| `espressif.esp-idf-extension` | сборка, прошивка, монитор, менюшная интеграция (обязательно) |
| `llvm-vs-code-extensions.vscode-clangd` | навигация и подсветка C++23 (основной IntelliSense) |
| `ms-vscode.cpptools` | IntelliSense/отладка для встраиваемых целей |
| `xaver.clang-format` | форматирование по `.clang-format` репозитория |
| `ms-vscode.cmake-tools` | CMake 3.16+, `MINIMAL_BUILD`, регистрация компонентов |
| `ms-vscode.makefile-tools` | Makefile-цели, если используются точечные сборки |
| `timonwong.shellcheck` | проверка `scripts/**/*.sh`, `docker/firmware-builder/entrypoint.sh` |
| `redhat.vscode-yaml` | `.github/workflows/build.yml`, `main/idf_component.yml` |
| `editorconfig.editorconfig` | отступы, единый стиль редактирования |
| `mikestead.dotenv` | `.env.example` — адаптация под Центральную Россию |
| `ms-python.python`, `ms-python.debugpy` | `scripts/build.py`, `scripts/tests`, отладка Python |
| `davidanson.vscode-markdownlint`, `yzhang.markdown-all-in-one` | перевод и правка `docs/`, `README.md` |
| `streetsidesoftware.code-spell-checker(-russian)` | проверка русскоязычной документации |
| `ms-ceintl.vscode-language-pack-ru` | русский UI редактора |
| `vivaxy.vscode-conventional-commits` | формат коммитов `<тип>: <описание>` |
| `github.vscode-github-actions` | CI `build.yml`, матрица вариантов |
| `kilocode.kilo-code` | агентный режим работы в этой рабочей области |
| `vadimcn.vscode-lldb` | GDB-отладка на железе |
| `eamodio.gitlens` | история форка `ZDarow/xiaozhi-esp32` |

> Конфликт IntelliSense: `clangd` и `cpptools` не должны работать одновременно —
> включается двойная индексация и тормоза. Выбери один (рекомендуется `clangd`)
> и отключи второй командой из раздела 5.

## 3. Нерелевантные расширения: зачем убирать

Расширения перечислены в `.vscode/extensions.json` → `unwantedRecommendations`
(это подсказка в UI, а не запрет). Установлены в текущем профиле VS Code: 95.

| Группа | Расширения | Причина |
|--------|-----------|---------|
| Конкурирующие toolchain'ы | `platformio.platformio-ide`, `pioarduino.pioarduino-ide`, `disroop.conan` | свой indexer/toolchain, конфликтует с ESP-IDF Component Manager и `clangd` |
| Java/Kotlin | `redhat.java`, `vscjava.*` (6 шт.), `vmware.vscode-spring-boot`, `vscjava.vscode-spring-boot-dashboard`, `fwcd.kotlin`, `mathiasfrohlich.kotlin`, `redhat.vscode-xml` | в репозитории нет JVM-кода; тянут Language Server (~1 ГБ RAM) |
| .NET | `ms-dotnettools.vscode-dotnet-runtime` | в репозитории нет C# |
| Web/JS | `ms-vscode.vscode-typescript-next`, `dbaeumer.vscode-eslint`, `esbenp.prettier-vscode`, `christian-kohler.npm-intellisense`, `ecmel.vscode-html-css`, `solnurkarim.html-to-css-autocompletion`, `zignd.html-css-class-completion`, `tobermory.es6-string-html`, `vincaslt.highlight-matching-tag`, `formulahendry.auto-rename-tag`, `formulahendry.code-runner`, `meganrogge.template-string-converter`, `ritwickdey.liveserver`, `techer.open-in-browser` | нет веб-ассетов и Node-сборки; `formulahendry.code-runner` выполняет произвольные команды по F5 |
| Python-инструменты не по проекту | `ms-python.vscode-pylance`, `ms-python.mypy-type-checker`, `ms-python.black-formatter`, `charliermarsh.ruff`, `jbenden.c-cpp-flylint` | нет конфигурации `pyproject.toml`/`setup.cfg`/`mypy.ini`; `flylint` — Python-линтер, ложные срабатывания на C++ |
| Облако и удалённые среды | `ms-azuretools.vscode-docker`, `ms-azuretools.vscode-containers`, `ms-vscode.azure-repos`, `ms-vscode-remote.remote-wsl`, `ms-vscode.remote-repositories`, `github.codespaces`, `github.remotehub` | Docker используется только через CLI; WSL/Codespaces/Azure не применяются, в Центральной России внешние сервисы ограничены |
| Дублирующие CMake/оболочку | `twxs.cmake`, `josetr.cmake-language-support-vscode`, `foxundermoon.shell-format` | `ms-vscode.cmake-tools` уже покрывает CMake; shell-форматтер не соответствует стилю shell-скриптов проекта |
| Прочее | `tamasfe.even-better-toml` (нет TOML), `ms-vscode.powershell` (хост Linux), `c0der-himel.vscode-wev-dev-extension-pack`, `c0der-himel.remove-console-log`, `igorsbitnev.error-gutters`, `usernamehw.errorlens` | не влияют на сборку, дублируют встроенный функционал |

Нейтральные (оставлены как есть, на усмотрение Хозяина): `vscodevim.vim`,
`hars.cppsnippets`, `idma88.c-cpp--snippets`, `christian-kohler.path-intellisense`,
`donjayamanne.githistory`, `mhutchie.git-graph`, `github.vscode-pull-request-github`,
`aaron-bond.better-comments`, `oderwat.indent-rainbow`, `bierner.color-info`,
`bierner.docs-view`, `bierner.markdown-mermaid`, `bierner.markdown-preview-github-styles`,
`jason2866.esp-decoder`, `github.github-vscode-theme`, `pkief.material-icon-theme`,
`vscode-icons-team.vscode-icons`.

## 4. CLI-команды: диагностика

```bash
# Список установленных расширений с версиями
code --list-extensions --show-versions

# Сколько всего установлено
code --list-extensions | wc -l

# Есть ли среди установленных нерелевантные (сверка с extensions.json)
code --list-extensions | grep -E 'platformio|pioarduino|conan|^redhat\.java|^vscjava|kotlin|spring-boot|dotnet|pylance|mypy|black-formatter|ruff|eslint|prettier|vscode-docker|codespaces|remote-wsl'

# Установленные расширения, которые точно нужны проекту
code --list-extensions | grep -E 'esp-idf-extension|clangd|cpptools|clang-format|cmake-tools|dotenv|shellcheck'
```

## 5. CLI-команды: отключение и удаление

### 5.1. Отключение (расширение остаётся, но выключено)

`--disable-extension` в VS Code 1.140.0 работает на уровне профиля пользователя
(`~/.config/Code/User/globalStorage/storage.json`), то есть действует на все рабочие области.
Флага `--disable-workspace-extensions` в этой сборке нет — для изоляции используй профиль (5.3).

```bash
# Пример: отключить конкурирующие toolchain'ы и web-стек
for ext in \
  platformio.platformio-ide \
  pioarduino.pioarduino-ide \
  disroop.conan \
  formulahendry.code-runner \
  ritwickdey.liveserver \
  ms-python.vscode-pylance \
  ms-python.mypy-type-checker \
  ms-python.black-formatter \
  charliermarsh.ruff \
  jbenden.c-cpp-flylint
do
  code --disable-extension "$ext"
done

# Второй IntelliSense (если выбран clangd) — отключить cpptools-набор
code --disable-extension ms-vscode.cpptools
code --disable-extension ms-vscode.cpp-devtools
```

### 5.2. Удаление (освобождает место, восстанавливается через `--install-extension`)

Сначала прогони dry-run — команда печатает список, ничего не удаляя:

```bash
UNWANTED="platformio.platformio-ide pioarduino.pioarduino-ide disroop.conan \
redhat.java vscjava.vscode-java-pack vscjava.vscode-java-debug vscjava.vscode-java-dependency \
vscjava.vscode-java-test vscjava.vscode-maven vscjava.vscode-gradle vmware.vscode-spring-boot \
vscjava.vscode-spring-boot-dashboard redhat.vscode-xml fwcd.kotlin mathiasfrohlich.kotlin \
ms-dotnettools.vscode-dotnet-runtime tamasfe.even-better-toml \
ms-vscode.vscode-typescript-next dbaeumer.vscode-eslint esbenp.prettier-vscode \
christian-kohler.npm-intellisense ecmel.vscode-html-css solnurkarim.html-to-css-autocompletion \
zignd.html-css-class-completion tobermory.es6-string-html vincaslt.highlight-matching-tag \
formulahendry.auto-rename-tag formulahendry.code-runner meganrogge.template-string-converter \
ritwickdey.liveserver techer.open-in-browser ms-azuretools.vscode-docker \
ms-azuretools.vscode-containers ms-vscode.azure-repos ms-vscode-remote.remote-wsl \
ms-vscode.remote-repositories github.codespaces github.remotehub \
ms-vscode.powershell twxs.cmake josetr.cmake-language-support-vscode \
foxundermoon.shell-format c0der-himel.vscode-wev-dev-extension-pack \
c0der-himel.remove-console-log igorsbitnev.error-gutters usernamehw.errorlens \
ms-vscode.cpp-devtools"

echo "$UNWANTED" | tr ' ' '\n' | grep -v '^$'
```

Применение (после подтверждения Хозяина):

```bash
echo "$UNWANTED" | tr ' ' '\n' | grep -v '^$' | xargs -r -n1 code --uninstall-extension

# Установить недостающие рекомендуемые
code --install-extension espressif.esp-idf-extension
code --install-extension xaver.clang-format
code --install-extension redhat.vscode-yaml
code --install-extension mikestead.dotenv
code --install-extension timonwong.shellcheck
```

Отдельный список — «мягкие» расширения Python-туллинга. Они помечены в
`unwantedRecommendations`, потому что проект не имеет конфигурации `pyproject.toml`,
`mypy.ini` или `.ruff.toml`, но удалять их стоит только если Python вне проекта не
используется:

```bash
OPTIONAL_PY="ms-python.vscode-pylance ms-python.mypy-type-checker \
ms-python.black-formatter charliermarsh.ruff jbenden.c-cpp-flylint"

# dry-run
echo "$OPTIONAL_PY" | tr ' ' '\n' | grep -v '^$'

# применение — по решению Хозяина
# echo "$OPTIONAL_PY" | tr ' ' '\n' | grep -v '^$' | xargs -r -n1 code --uninstall-extension
```

### 5.3. Изоляция рабочей области через профиль

Профиль — единственный способ ограничить набор расширений одной рабочей областью
при работе нескольких проектов на одной машине:

```bash
# Создать профиль (интерактивно) или запустить проект в отдельном профиле
code --profile xiaozhi-esp32 .

# Внутри профиза отключить лишнее — записи изолированы в профиле, не в User/settings.json
code --profile xiaozhi-esp32 --disable-extension platformio.platformio-ide

# Запуск без расширений — для чистой проверки «собирается ли проект как есть»
code --disable-extensions --new-window .
```

### 5.4. Разовая диагностика расширений

```bash
# Какие расширения реально активны в окне (лог GPU/расширений)
code --status
code --prof-startup --new-window .   # профилирование старта, лог в ./Exthost*
```

## 6. Нерелевантные инструменты хоста (CLI)

Проверь, нет ли конфликтующих CLI в `PATH` — они не нужны проекту и могут перехватывать
сборку или запускаться агентом по ошибке:

```bash
for tool in pio platformio arduino-cli conan eslint prettier black ruff mypy pylance node npm gradle mvn dotnet java kotlinc; do
  p=$(command -v "$tool" 2>/dev/null) && echo "$tool -> $p"
done
```

Удаление (Linux Mint, только по явному подтверждению Хозяина):

```bash
# pipx-окружения — сначала посмотреть, что установлено
pipx list

# Удалить конкретное окружение целиком (пример)
pipx uninstall ruff
pipx uninstall black

# Node-пакеты глобально — посмотреть и удалить лишнее
npm ls -g --depth=0
npm uninstall -g prettier eslint
```

Если инструмент нужен в других проектах, удаляй только глобальную линковку, а не пакет,
либо оставь его и добавь в `.gitignore`-подобный список исключений агента
(см. раздел «Ограничения» в `AGENTS.md`).

## 7. Критерии готовности настройки

- [ ] `code --list-extensions` не содержит `platformio`, `pioarduino`, `conan`, `redhat.java`, `vscjava.*`, `kotlin`, `spring-boot`, `dotnet`, `pylance`, `mypy-type-checker`, `black-formatter`, `ruff`, `eslint`, `prettier`, `vscode-docker`, `codespaces`, `remote-wsl`.
- [ ] `code --list-extensions | grep esp-idf-extension` возвращает `espressif.esp-idf-extension`.
- [ ] `clang-format --version` → 18.x; `clang-format --dry-run -Werror main/ota.cc` проходит для тронутых файлов.
- [ ] `python3 -m unittest discover -s scripts/tests` → `Ran 81 tests ... OK`.
- [ ] `source /home/mi/.espressif/v6.0.2/esp-idf/export.sh && idf.py --version` → `v6.0.2`.
- [ ] В панели Extensions нет конфликта двух IntelliSense (активен либо `clangd`, либо `cpptools`).

## 8. Шаринг настроек в репозитории

`.vscode/` указан в `.gitignore`, поэтому `extensions.json` и `settings.json` действуют
только локально. Варианты:

```bash
# Точечный коммит только файлов рекомендаций
git add -f .vscode/extensions.json
git commit -m "chore: добавить рекомендации расширений VS Code"

# Либо исключение из .gitignore: убрать строку ".vscode/" и добавить
#   .vscode/*
#   !.vscode/extensions.json
```

`settings.json` содержит локальный путь `idf.currentSetup` — в репозиторий его не коммитить,
замени на относительный или документируй путь в `CONTRIBUTING.md`.
