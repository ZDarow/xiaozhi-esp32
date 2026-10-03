#include "settings.h"

#include <esp_log.h>
#include <nvs_flash.h>

#define TAG "Settings"

Settings::Settings(const std::string& ns, bool read_write) : ns_(ns), read_write_(read_write) {
    esp_err_t err = nvs_open(ns.c_str(), read_write_ ? NVS_READWRITE : NVS_READONLY, &nvs_handle_);
    if (err != ESP_OK) {
        nvs_handle_ = 0;
        ESP_LOGW(TAG, "Failed to open namespace %s: %s", ns_.c_str(), esp_err_to_name(err));
    }
}

Settings::~Settings() {
    if (nvs_handle_ != 0) {
        if (read_write_ && dirty_) {
            // Ошибка коммита не должна поднимать panic: она означает лишь,
            // что значение не сохранено, и на следующей записи повторится попытка.
            esp_err_t err = nvs_commit(nvs_handle_);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to commit namespace %s: %s", ns_.c_str(),
                         esp_err_to_name(err));
            }
        }
        nvs_close(nvs_handle_);
    }
}

void Settings::ReportError(const char* operation, const std::string& key, esp_err_t err) const {
    ESP_LOGE(TAG, "nvs_%s failed in namespace %s (key length %zu): %s", operation, ns_.c_str(),
             key.size(), esp_err_to_name(err));
}

bool Settings::ValidateKey(const char* operation, const std::string& key) const {
    if (key.empty()) {
        ESP_LOGE(TAG, "Rejected empty key for nvs_%s in namespace %s", operation, ns_.c_str());
        return false;
    }
    if (key.size() > kMaxKeyLength) {
        ESP_LOGE(TAG, "Rejected key of length %zu for nvs_%s in namespace %s (limit %zu)",
                 key.size(), operation, ns_.c_str(), kMaxKeyLength);
        return false;
    }
    if (key.find('\0') != std::string::npos) {
        ESP_LOGE(TAG, "Rejected key with NUL byte for nvs_%s in namespace %s", operation,
                 ns_.c_str());
        return false;
    }
    if (nvs_handle_ == 0) {
        ESP_LOGE(TAG, "Namespace %s is not open for nvs_%s", ns_.c_str(), operation);
        return false;
    }
    return true;
}

std::string Settings::GetString(const std::string& key, const std::string& default_value) {
    if (nvs_handle_ == 0) {
        return default_value;
    }

    size_t length = 0;
    if (nvs_get_str(nvs_handle_, key.c_str(), nullptr, &length) != ESP_OK) {
        return default_value;
    }

    if (length == 0 || length > 4096) {
        ESP_LOGW(TAG, "Invalid NVS string length for %s: %zu", key.c_str(), length);
        return default_value;
    }

    std::string value;
    value.resize(length);
    auto err = nvs_get_str(nvs_handle_, key.c_str(), value.data(), &length);
    if (err != ESP_OK) {
        return default_value;
    }
    if (length > 0 && value[length - 1] != '\0') {
        ESP_LOGW(TAG, "NVS string %s is not NUL-terminated", key.c_str());
    }
    while (!value.empty() && value.back() == '\0') {
        value.pop_back();
    }
    return value;
}

bool Settings::SetString(const std::string& key, const std::string& value) {
    if (!read_write_) {
        ESP_LOGW(TAG, "Namespace %s is not open for writing", ns_.c_str());
        return false;
    }
    if (!ValidateKey("set_str", key)) {
        return false;
    }
    esp_err_t err = nvs_set_str(nvs_handle_, key.c_str(), value.c_str());
    if (err != ESP_OK) {
        ReportError("set_str", key, err);
        return false;
    }
    dirty_ = true;
    return true;
}

int32_t Settings::GetInt(const std::string& key, int32_t default_value) {
    if (nvs_handle_ == 0) {
        return default_value;
    }

    int32_t value;
    if (nvs_get_i32(nvs_handle_, key.c_str(), &value) != ESP_OK) {
        return default_value;
    }
    return value;
}

bool Settings::SetInt(const std::string& key, int32_t value) {
    if (!read_write_) {
        ESP_LOGW(TAG, "Namespace %s is not open for writing", ns_.c_str());
        return false;
    }
    if (!ValidateKey("set_i32", key)) {
        return false;
    }
    esp_err_t err = nvs_set_i32(nvs_handle_, key.c_str(), value);
    if (err != ESP_OK) {
        ReportError("set_i32", key, err);
        return false;
    }
    dirty_ = true;
    return true;
}

bool Settings::GetBool(const std::string& key, bool default_value) {
    if (nvs_handle_ == 0) {
        return default_value;
    }

    uint8_t value;
    if (nvs_get_u8(nvs_handle_, key.c_str(), &value) != ESP_OK) {
        return default_value;
    }
    return value != 0;
}

bool Settings::SetBool(const std::string& key, bool value) {
    if (!read_write_) {
        ESP_LOGW(TAG, "Namespace %s is not open for writing", ns_.c_str());
        return false;
    }
    if (!ValidateKey("set_u8", key)) {
        return false;
    }
    esp_err_t err = nvs_set_u8(nvs_handle_, key.c_str(), value ? 1 : 0);
    if (err != ESP_OK) {
        ReportError("set_u8", key, err);
        return false;
    }
    dirty_ = true;
    return true;
}

void Settings::EraseKey(const std::string& key) {
    if (!read_write_) {
        ESP_LOGW(TAG, "Namespace %s is not open for writing", ns_.c_str());
        return;
    }
    if (!ValidateKey("erase_key", key)) {
        return;
    }
    esp_err_t err = nvs_erase_key(nvs_handle_, key.c_str());
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ReportError("erase_key", key, err);
    }
}

void Settings::EraseAll() {
    if (!read_write_) {
        ESP_LOGW(TAG, "Namespace %s is not open for writing", ns_.c_str());
        return;
    }
    if (nvs_handle_ == 0) {
        ESP_LOGE(TAG, "Namespace %s is not open for erase_all", ns_.c_str());
        return;
    }
    esp_err_t err = nvs_erase_all(nvs_handle_);
    if (err != ESP_OK) {
        ReportError("erase_all", std::string("*"), err);
    }
}
