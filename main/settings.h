#ifndef SETTINGS_H
#define SETTINGS_H

#include <cstddef>
#include <string>
#include <nvs_flash.h>

class Settings {
public:
    Settings(const std::string& ns, bool read_write = false);
    ~Settings();

    std::string GetString(const std::string& key, const std::string& default_value = "");
    bool SetString(const std::string& key, const std::string& value);
    int32_t GetInt(const std::string& key, int32_t default_value = 0);
    bool SetInt(const std::string& key, int32_t value);
    bool GetBool(const std::string& key, bool default_value = false);
    bool SetBool(const std::string& key, bool value);
    void EraseKey(const std::string& key);
    void EraseAll();

private:
    // NVS принимает имя ключа длиной не более NVS_KEY_NAME_MAX_SIZE-1 символов.
    static constexpr size_t kMaxKeyLength = NVS_KEY_NAME_MAX_SIZE - 1;

    bool ValidateKey(const char* operation, const std::string& key) const;
    void ReportError(const char* operation, const std::string& key, esp_err_t err) const;

    std::string ns_;
    nvs_handle_t nvs_handle_ = 0;
    bool read_write_ = false;
    bool dirty_ = false;
};

#endif
