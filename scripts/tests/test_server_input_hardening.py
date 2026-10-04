"""Хост-тесты защиты от некорректных данных сервера.

Проверяет то, что обычный ответ сервера не может вызвать аварийный останов:

* `main/string_utils.h` — строгий разбор целого без исключений;
* `main/ota.cc` — `ParseVersion` и белый список ключей NVS;
* `main/settings.cc` — отсутствие `ESP_ERROR_CHECK` на путях записи.

Логика C++ собирается и запускается на хосте; для `ParseVersion` и
`IsAllowedServerKey` исходник извлекается из файлов, чтобы тест ловил регрессии
при правках без дублирования кода.
"""

import re
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
STRING_UTILS = ROOT / "main" / "string_utils.h"
OTA_CC = ROOT / "main" / "ota.cc"
SETTINGS_CC = ROOT / "main" / "settings.cc"
SETTINGS_H = ROOT / "main" / "settings.h"
OTA_IDF_YML = ROOT / "main" / "idf_component.yml"

# Собираются без ESP-IDF: нужны только стандартные заголовки.
HELPERS = r"""
#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <string_view>

#define TAG "Ota"
#define ESP_LOGW(tag, ...) ((void)0)
#define ESP_LOGE(tag, ...) ((void)0)
"""

PARSER_HARNESS = HELPERS + r"""
#include "string_utils.h"

// Определение метода извлекается из main/ota.cc целиком (сигнатура + тело).
@BODY@

int main() {
    struct {
        const char* input;
        bool expect_ok;
        int expect_first;
    } cases[] = {
        {"1.0.0", true, 1},
        {"2.5.13", true, 2},
        {"0.0.1", true, 0},
        {"1.0.0-beta", false, 0},
        {"1.x", false, 0},
        {"latest", false, 0},
        {"1..2", false, 0},
        {"", false, 0},
        {"-1.0", false, 0},
        {" 1.0", false, 0},
        {"99999999999999999999.0", false, 0},
    };
    for (const auto& c : cases) {
        auto parsed = ParseVersion(c.input);
        bool ok = !parsed.empty();
        if (ok != c.expect_ok) {
            std::printf("FAIL ParseVersion(%s): ok=%d expected %d\n", c.input, (int)ok,
                        (int)c.expect_ok);
            return 1;
        }
        if (ok && parsed[0] != c.expect_first) {
            std::printf("FAIL ParseVersion(%s): first=%d expected %d\n", c.input, parsed[0],
                        c.expect_first);
            return 1;
        }
    }
    return 0;
}
"""

WHITELIST_HARNESS = HELPERS + r"""
#include <string>
#include <string_view>
#include <vector>

// Извлекается из main/ota.cc.
@WHITELIST@

int main() {
    struct {
        const char* key;
        bool allowed;
    } mqtt_cases[] = {
        {"endpoint", true},   {"client_id", true},  {"username", true},
        {"password", true},   {"keepalive", true},  {"publish_topic", true},
        {"evil_key_long_enough_to_pass_nvs_limit", false},
        {"url", false},
        {"", false},
    };
    for (const auto& c : mqtt_cases) {
        bool allowed = IsAllowedServerKey(c.key, kAllowedMqttKeys);
        if (allowed != c.allowed) {
            std::printf("FAIL mqtt key '%s': allowed=%d expected %d\n", c.key, (int)allowed,
                        (int)c.allowed);
            return 1;
        }
    }
    struct {
        const char* key;
        bool allowed;
    } ws_cases[] = {
        {"url", true}, {"token", true}, {"version", true}, {"endpoint", false}, {"x", false},
    };
    for (const auto& c : ws_cases) {
        bool allowed = IsAllowedServerKey(c.key, kAllowedWebsocketKeys);
        if (allowed != c.allowed) {
            std::printf("FAIL websocket key '%s': allowed=%d expected %d\n", c.key, (int)allowed,
                        (int)c.allowed);
            return 1;
        }
    }
    if (IsAllowedServerKey(nullptr, kAllowedMqttKeys)) {
        std::printf("FAIL nullptr key accepted\n");
        return 1;
    }
    return 0;
}
"""


def extract_method(source: str, signature: str) -> str:
    """Вырезает тело метода по сигнатуре: сигнатура плюс блок в фигурных скобках."""
    start = source.find(signature)
    if start < 0:
        raise AssertionError(f"Не найдена сигнатура: {signature}")
    brace = source.find("{", start)
    depth = 0
    for index in range(brace, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[start:index + 1]
    raise AssertionError("Не найден конец тела метода")


def extract_namespace_block(source: str) -> str:
    """Вырезает содержимое анонимного пространства имён из main/ota.cc."""
    start = source.find("namespace {")
    if start < 0:
        raise AssertionError("Не найден анонимный namespace в main/ota.cc")
    brace = source.find("{", start)
    depth = 0
    for index in range(brace, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[brace + 1:index]
    raise AssertionError("Не найден конец анонимного namespace")


def run_harness(source: str, harness: str) -> subprocess.CompletedProcess:
    with tempfile.TemporaryDirectory() as directory:
        cpp = Path(directory) / "harness.cpp"
        cpp.write_text(harness, encoding="utf-8")
        binary = Path(directory) / "harness"
        compile_result = subprocess.run(
            ["g++", "-std=gnu++23", "-Wall", "-Wextra", "-Werror", "-o", str(binary), str(cpp),
             "-I", str(ROOT / "main")],
            capture_output=True,
            text=True,
            timeout=120,
        )
        if compile_result.returncode != 0:
            raise AssertionError(
                "Компиляция тестовой обвязки не удалась:\n"
                f"{compile_result.stdout}\n{compile_result.stderr}"
            )
        return subprocess.run([str(binary)], capture_output=True, text=True, timeout=60)


class ServerInputHardeningTest(unittest.TestCase):
    def test_parse_int_accepts_only_plain_decimal(self):
        self.assertTrue(STRING_UTILS.exists(), "Отсутствует main/string_utils.h")

        harness = HELPERS + r"""
#include "string_utils.h"

#include <climits>

int main() {
    struct {
        const char* input;
        bool ok;
        int value;
    } cases[] = {
        {"0", true, 0},        {"42", true, 42},   {"007", true, 7},
        {"-5", true, -5},      {"+9", true, 9},    {"999999", true, 999999},
        {"", false, 0},        {" ", false, 0},    {" 1", false, 0},
        {"1 ", false, 0},      {"1a", false, 0},    {"a", false, 0},
        {"-", false, 0},       {"+", false, 0},    {"0x10", false, 0},
        {"1.5", false, 0},     {"99999999999999999999", false, 0},
        {"1e3", false, 0},
    };
    for (const auto& c : cases) {
        int value = 12345;
        bool ok = ParseInt(c.input, value);
        if (ok != c.ok) {
            std::printf("FAIL ParseInt('%s'): ok=%d expected %d\n", c.input, (int)ok, (int)c.ok);
            return 1;
        }
        if (ok && value != c.value) {
            std::printf("FAIL ParseInt('%s'): value=%d expected %d\n", c.input, value, c.value);
            return 1;
        }
    }
    // Границы диапазона
    int value = 0;
    if (ParseInt("70000", value, 1, 65535)) {
        std::printf("FAIL: значение вне диапазона принято\n");
        return 1;
    }
    if (!ParseInt("65535", value, 1, 65535) || value != 65535) {
        std::printf("FAIL: верхняя граница диапазона отклонена\n");
        return 1;
    }
    if (!ParseInt("1", value, 1, 65535) || value != 1) {
        std::printf("FAIL: нижняя граница диапазона отклонена\n");
        return 1;
    }
    if (ParseInt("0", value, 1, 65535)) {
        std::printf("FAIL: значение ниже диапазона принято\n");
        return 1;
    }
    if (!ParseInt("999999", value, 0, 999999)) {
        std::printf("FAIL: максимум версии отклонён\n");
        return 1;
    }
    if (ParseInt("1000000", value, 0, 999999)) {
        std::printf("FAIL: слишком большой сегмент версии принят\n");
        return 1;
    }
    return 0;
}
"""
        result = run_harness("", harness)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_parse_version_rejects_malformed_segments(self):
        source = OTA_CC.read_text(encoding="utf-8")
        body = extract_method(source, "std::vector<int> Ota::ParseVersion")
        body = body.replace("Ota::ParseVersion", "ParseVersion")
        harness = PARSER_HARNESS.replace("@BODY@", body)
        result = run_harness("", harness)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_server_keys_are_whitelisted(self):
        source = OTA_CC.read_text(encoding="utf-8")
        block = extract_namespace_block(source)
        self.assertIn("kAllowedMqttKeys", block)
        self.assertIn("kAllowedWebsocketKeys", block)
        harness = WHITELIST_HARNESS.replace("@WHITELIST@", block)
        result = run_harness("", harness)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_no_std_stoi_left_in_server_input_paths(self):
        """std::stoi бросает исключение, а исключения в проекте выключены."""
        for path in (
            ROOT / "main" / "ota.cc",
            ROOT / "main" / "protocols" / "mqtt_protocol.cc",
            ROOT / "main" / "audio" / "audio_debugger.cc",
        ):
            with self.subTest(path=path.name):
                self.assertNotIn("std::stoi", path.read_text(encoding="utf-8"))

    def test_settings_write_paths_do_not_abort(self):
        """Ошибка записи в NVS не должна поднимать panic (ESP_ERROR_CHECK → abort)."""
        source = SETTINGS_CC.read_text(encoding="utf-8")
        offenders = [
            line.strip()
            for line in source.splitlines()
            if "ESP_ERROR_CHECK" in line
        ]
        self.assertEqual(offenders, [], f"Остались вызовы ESP_ERROR_CHECK: {offenders}")
        # Валидация ключа обязана быть до записи: NVS_KEY_NAME_MAX_SIZE = 16.
        self.assertIn("kMaxKeyLength", SETTINGS_H.read_text(encoding="utf-8"))
        self.assertIn("NVS_KEY_NAME_MAX_SIZE", SETTINGS_H.read_text(encoding="utf-8"))

    def test_settings_setters_report_result(self):
        """Сеттеры возвращают bool: вызывающий может отреагировать на отказ."""
        header = SETTINGS_H.read_text(encoding="utf-8")
        for method in ("SetString", "SetInt", "SetBool"):
            with self.subTest(method=method):
                self.assertRegex(header, rf"bool {method}\(")

    def test_mqtt_component_dependency_declared(self):
        """ESP-IDF 6.1 вынес esp-mqtt в component manager."""
        manifest = OTA_IDF_YML.read_text(encoding="utf-8")
        self.assertRegex(manifest, r"espressif/mqtt")

    def test_mask_secret_hides_middle(self):
        """Маскирование должно оставлять только края, но не само секретное значение."""
        source = OTA_CC.read_text(encoding="utf-8")
        body = extract_method(source, "std::string MaskSecret(")
        harness = HELPERS + (
            "#include <string>\n\n"
            "@BODY@\n\n"
            r"""
int main() {
    struct {
        const char* input;
        const char* expected;
    } cases[] = {
        {"", "********"},
        {"1234567", "********"},
        {"12345678", "********"},
        {"123456789", "1234****6789"},
        {"0123456789abcdef", "0123****cdef"},
    };
    for (const auto& c : cases) {
        std::string got = MaskSecret(c.input);
        if (got != c.expected) {
            std::printf("FAIL MaskSecret('%s') = '%s', expected '%s'\n", c.input, got.c_str(),
                        c.expected);
            return 1;
        }
    }
    // Секрет длиной больше 8 символов не должен попадать в лог целиком.
    std::string secret = "0123456789abcdef";
    if (MaskSecret(secret) == secret) {
        std::printf("FAIL: значение не замаскировано\n");
        return 1;
    }
    return 0;
}
"""
        ).replace("@BODY@", body)
        result = run_harness("", harness)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_secrets_are_not_logged_verbatim(self):
        """Серийный номер и HMAC не должны печататься целиком (P1-4)."""
        source = OTA_CC.read_text(encoding="utf-8")
        offenders = [
            line.strip()
            for line in source.splitlines()
            if "ESP_LOG" in line
            and (
                "serial_number_.c_str()" in line
                or ("payload" in line and "json.c_str()" in line)
                or "activation_challenge_.c_str()" in line
            )
        ]
        self.assertEqual(offenders, [], f"Секреты печатаются в лог: {offenders}")
        self.assertIn("MaskSecret", source)


if __name__ == "__main__":
    unittest.main()