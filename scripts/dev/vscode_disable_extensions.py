#!/usr/bin/env python3
"""Отключение расширений VS Code в области действия одной рабочей папки.

VS Code хранит состояние включённости расширений в SQLite-хранилище.
Для профиля это `User/globalStorage/state.vscdb`, для рабочей области —
`User/workspaceStorage/<id>/state.vscdb`. Ключ в обоих случаях один:
`extensionsIdentifiers/disabled`, значение — JSON-массив объектов
`{"id": "<publisher.name>", "uuid": "<идентификатор расширения>"}`.

Профильный уровень затрагивает все рабочие области профиля, поэтому здесь
используется уровень рабочей области: расширения отключаются только в той
папке, где запущен скрипт.

Сценарий безопасен: по умолчанию ничего не меняет, создаёт резервную копию
базы перед записью и отказывается работать, если базу держит запущенный
VS Code (в этом случае состояние из памяти редактора перезапишет файл).

Использование:
    python3 scripts/dev/vscode_disable_extensions.py            # сухая проверка
    python3 scripts/dev/vscode_disable_extensions.py --status   # текущее состояние
    python3 scripts/dev/vscode_disable_extensions.py --apply    # применить
    python3 scripts/dev/vscode_disable_extensions.py --undo     # откат

Если редактор запущен и закрыть его нельзя, добавь `--force`: SQLite обновляет
ключи по отдельности, поэтому запись переживает работу редактора, пока тот
не перезапишет этот же ключ. После `--force` обязательно перезагрузи окно
(Developer: Reload Window) и проверь `--status`.
"""

import argparse
import json
import os
import shutil
import sqlite3
import sys
import time
from pathlib import Path

STORAGE_KEY = "extensionsIdentifiers/disabled"
DEFAULT_USER_DATA = Path.home() / ".config" / "Code" / "User"
DEFAULT_MANIFEST = Path.home() / ".vscode" / "extensions" / "extensions.json"
REPO_EXTENSIONS_JSON = Path(__file__).resolve().parents[2] / ".vscode" / "extensions.json"


def die(message: str, code: int = 1) -> None:
    print(f"Ошибка: {message}", file=sys.stderr)
    sys.exit(code)


def folder_uri(path: Path) -> str:
    """URI рабочей области в том виде, в котором её хранит VS Code."""
    return "file://" + str(path.resolve())


def find_workspace_storage(workspace: Path, user_data: Path) -> Path:
    """Каталог workspaceStorage для указанной папки."""
    storage_root = user_data / "workspaceStorage"
    if not storage_root.is_dir():
        die(f"каталог {storage_root} не найден — VS Code ещё не открывал рабочую область")
    target = folder_uri(workspace)
    for candidate in sorted(storage_root.iterdir()):
        descriptor = candidate / "workspace.json"
        if not descriptor.is_file():
            continue
        try:
            data = json.loads(descriptor.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError):
            continue
        location = data.get("folder") or data.get("workspace") or ""
        if location == target:
            return candidate
    die(f"рабочая область {target} не найдена в workspaceStorage — открой папку в VS Code один раз")


def load_disabled(db_path: Path) -> list[dict]:
    """Текущий список отключённых расширений рабочей области."""
    if not db_path.is_file():
        return []
    connection = sqlite3.connect(f"file:{db_path}?mode=ro", uri=True)
    try:
        row = connection.execute(
            "SELECT value FROM ItemTable WHERE key = ?", (STORAGE_KEY,)
        ).fetchone()
    except sqlite3.Error as error:
        die(f"чтение {db_path} не удалось: {error}")
    finally:
        connection.close()
    if not row or not row[0]:
        return []
    try:
        return json.loads(row[0])
    except json.JSONDecodeError:
        die(f"значение ключа {STORAGE_KEY} в {db_path} повреждено")


def load_uuids(manifest: Path) -> dict[str, str]:
    """Идентификаторы (uuid) установленных расширений из манифеста VS Code."""
    if not manifest.is_file():
        die(f"манифест расширений не найден: {manifest}")
    entries = json.loads(manifest.read_text(encoding="utf-8"))
    uuids: dict[str, str] = {}
    for entry in entries:
        identifier = entry.get("identifier") or {}
        extension_id, extension_uuid = identifier.get("id"), identifier.get("uuid")
        if extension_id:
            uuids[extension_id] = extension_uuid or ""
    return uuids


def load_unwanted(extensions_json: Path) -> list[str]:
    """Список нерелевантных расширений из репозитория."""
    if not extensions_json.is_file():
        die(f"файл не найден: {extensions_json}")
    data = json.loads(extensions_json.read_text(encoding="utf-8"))
    return list(data.get("unwantedRecommendations") or [])


def holders(db_path: Path) -> list[int]:
    """Процессы, которые держат открытым файл базы (значит, VS Code запущен)."""
    found: list[int] = []
    for pid in filter(str.isdigit, os.listdir("/proc")):
        fd_dir = f"/proc/{pid}/fd"
        try:
            names = os.listdir(fd_dir)
        except OSError:
            continue
        for name in names:
            try:
                if os.readlink(f"{fd_dir}/{name}") == str(db_path):
                    found.append(int(pid))
                    break
            except OSError:
                continue
    return found


def write_disabled(db_path: Path, entries: list[dict]) -> None:
    backup = db_path.with_name(f"{db_path.name}.bak-{int(time.time())}")
    shutil.copy2(db_path, backup)
    connection = sqlite3.connect(db_path)
    try:
        connection.execute(
            "INSERT OR REPLACE INTO ItemTable (key, value) VALUES (?, ?)",
            (STORAGE_KEY, json.dumps(entries)),
        )
        connection.commit()
    except sqlite3.Error as error:
        connection.rollback()
        die(f"запись в {db_path} не удалась: {error} (резервная копия: {backup})")
    finally:
        connection.close()
    print(f"резервная копия: {backup}")


def latest_backup(db_path: Path) -> Path | None:
    backups = sorted(db_path.parent.glob(f"{db_path.name}.bak-*"))
    return backups[-1] if backups else None


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Отключение расширений VS Code в области одной рабочей папки.",
    )
    parser.add_argument("--apply", action="store_true", help="записать изменения (по умолчанию сухая проверка)")
    parser.add_argument("--undo", action="store_true", help="восстановить состояние из последней резервной копии")
    parser.add_argument("--status", action="store_true", help="показать текущее состояние и ничего не менять")
    parser.add_argument(
        "--force",
        action="store_true",
        help="записать, даже если базу держит запущенный VS Code (риск перезаписи из памяти редактора)",
    )
    parser.add_argument("--workspace", default=Path.cwd(), type=Path, help="папка рабочей области (по умолчанию текущая)")
    parser.add_argument("--user-data", default=DEFAULT_USER_DATA, type=Path, help="каталог User данных VS Code")
    parser.add_argument("--manifest", default=DEFAULT_MANIFEST, type=Path, help="манифест установленных расширений")
    parser.add_argument("--extensions-json", default=REPO_EXTENSIONS_JSON, type=Path, help="файл .vscode/extensions.json")
    args = parser.parse_args()

    workspace = args.workspace.resolve()
    storage = find_workspace_storage(workspace, args.user_data)
    db_path = storage / "state.vscdb"
    current = load_disabled(db_path)
    current_ids = {entry.get("id") for entry in current}
    print(f"рабочая область: {folder_uri(workspace)}")
    print(f"хранилище:       {db_path}")
    print(f"отключено сейчас: {len(current_ids)}")

    if args.undo:
        backup = latest_backup(db_path)
        if backup is None:
            die("резервных копий нет — откатывать нечего")
        if holders(db_path) and not args.force:
            die("базу держит запущенный VS Code — закрой редактор и повтори (или добавь --force)", code=2)
        shutil.copy2(backup, db_path)
        print(f"состояние восстановлено из {backup}")
        print("перезагрузи окно VS Code для этой папки: Developer: Reload Window")
        return 0

    unwanted = load_unwanted(args.extensions_json)
    uuids = load_uuids(args.manifest)
    todo = [item for item in unwanted if item not in current_ids]
    missing = [item for item in todo if item not in uuids]

    print(f"нерелевантных в .vscode/extensions.json: {len(unwanted)}")
    print(f"требуют отключения в этой области:        {len(todo)}")
    for item in todo:
        print(f"  - {item}")
    if missing:
        print(f"предупреждение: нет в манифесте установленных ({len(missing)}): {', '.join(missing)}")

    if args.status or not args.apply:
        print("\nрежим проверки: изменения не записаны. Для применения добавь --apply")
        return 0

    busy = holders(db_path)
    if busy and not args.force:
        print(
            f"\nбазу держит запущенный VS Code (pid {', '.join(map(str, busy))}). "
            "Он перезапишет файл из памяти — закрой редактор и повтори команду "
            "(либо добавь --force и перезагрузи окно после записи).",
            file=sys.stderr,
        )
        return 2
    if busy:
        print(
            f"\nвнимание: запись идёт при открытом VS Code (pid {', '.join(map(str, busy))}). "
            "SQLite обновляет ключи по отдельности, поэтому правка сохранится, если "
            "редактор не перезапишет этот ключ сам. После проверки перезагрузи окно: "
            "Developer: Reload Window."
        )

    merged = current + [{"id": item, "uuid": uuids.get(item, "")} for item in todo]
    write_disabled(db_path, merged)
    print(f"готово: отключено {len(merged)} расширений в области {folder_uri(workspace)}")
    print("перезагрузи окно VS Code для этой папки, чтобы изменения вступили в силу")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
