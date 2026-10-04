#include "ota.h"
#include "system_info.h"
#include "settings.h"
#include "string_utils.h"
#include "assets/lang_config.h"
#include "cjson_utils.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cJSON.h>
#include <esp_log.h>
#include <esp_partition.h>
#include <esp_ota_ops.h>
#include <esp_app_format.h>
#include <esp_efuse.h>
#include <esp_efuse_table.h>
#include <esp_heap_caps.h>
#include <esp_app_format.h>
#include <esp_system.h>
#include <esp_err.h>
#include <string_view>
#ifdef SOC_HMAC_SUPPORTED
#include <esp_hmac.h>
#endif

// Проверка подписи образа в формате ГОСТ 2012 в этом дереве не реализована:
// прототип вынесен в ветку feature/gost-crypto. Включение CONFIG_USE_GOST_CRYPTO
// останавливает конфигурацию в main/CMakeLists.txt с явным сообщением.

#include <cstring>
#include <vector>
#include <algorithm>

#define TAG "Ota"

namespace {

// Протоколы читают из NVS фиксированный набор ключей (docs/mqtt-udp.md, раздел 6.1;
// docs/websocket.md). Ключи из ответа сервера пишем только из этого списка:
// иначе сервер может заполнить namespace, записать ключ длиннее предела NVS или
// подменить значения, которые устройство не ожидает.
const std::vector<std::string_view> kAllowedMqttKeys = {"endpoint", "client_id", "username",
                                                        "password", "keepalive", "publish_topic"};

const std::vector<std::string_view> kAllowedWebsocketKeys = {"url", "token", "version"};

// Серийный номер и HMAC активации нельзя печатать целиком: значения попадают в
// HTTP-заголовок и тело запроса, а логи уходят в консоль и в Issue-шаблоны.
std::string MaskSecret(const std::string& value) {
    if (value.size() <= 8) {
        return "********";
    }
    return value.substr(0, 4) + "****" + value.substr(value.size() - 4);
}

bool IsAllowedServerKey(const char* key, const std::vector<std::string_view>& allowed) {
    if (key == nullptr) {
        return false;
    }
    std::string_view name(key);
    for (std::string_view candidate : allowed) {
        if (name == candidate) {
            return true;
        }
    }
    ESP_LOGW(TAG, "Ignoring unexpected key in OTA response: %s", key);
    return false;
}

}  // namespace

Ota::Ota() {
#ifdef ESP_EFUSE_BLOCK_USR_DATA
    // Read Serial Number from efuse user_data
    uint8_t serial_number[33] = {0};
    if (esp_efuse_read_field_blob(ESP_EFUSE_USER_DATA, serial_number, 32 * 8) == ESP_OK) {
        if (serial_number[0] == 0) {
            has_serial_number_ = false;
        } else {
            serial_number_ = std::string(reinterpret_cast<char*>(serial_number), 32);
            has_serial_number_ = true;
        }
    }
#endif
}

Ota::~Ota() {
}

// Проверка схемы OTA-URL. Подпись образа и HTTPS — не одно и то же: сервер может
// отдать корректный ответ по http://, и тогда трафик подделывается по дороге.
// Поэтому схема проверяется до любого запроса, независимо от настроек подписи.
static bool IsSafeOtaUrl(const std::string& url) {
    auto starts = [&url](const char* prefix) { return url.rfind(prefix, 0) == 0; };
    if (starts("https://")) {
        return true;
    }
#ifdef CONFIG_COMPILER_OPTIMIZATION_DEBUG
    // http допускается только в отладочных сборках (локальная разработка).
    if (starts("http://")) {
        return true;
    }
#endif
    return false;
}

std::string Ota::GetCheckVersionUrl() {
#ifdef CONFIG_ALLOW_OTA_ENV_OVERRIDE
    // 1. Переменная окружения OTA_URL — только для отладочных сборок. В release
    //    getenv() на ESP32 всё равно возвращает nullptr, но явный гейт не даёт
    //    случайно собрать dev-обход в релиз.
    const char* env_url = getenv("OTA_URL");
    if (env_url != nullptr && strlen(env_url) > 10) {
        if (IsSafeOtaUrl(env_url)) {
            return std::string(env_url);
        }
        ESP_LOGE(TAG, "OTA_URL from environment rejected: only https allowed");
    }
#endif
    // 2. NVS-ключ ota_url (runtime override)
    Settings settings("wifi", false);
    std::string url = settings.GetString("ota_url");
    if (!url.empty()) {
        if (IsSafeOtaUrl(url)) {
            return url;
        }
        ESP_LOGE(TAG, "NVS ota_url rejected: only https allowed, using Kconfig default");
    }
    // 3. Fallback на Kconfig
    return CONFIG_OTA_URL;
}

std::unique_ptr<Http> Ota::SetupHttp() {
    auto& board = Board::GetInstance();
    auto network = board.GetNetwork();
    auto http = network->CreateHttp(0);
    auto user_agent = SystemInfo::GetUserAgent();
    http->SetHeader("Activation-Version", has_serial_number_ ? "2" : "1");
    http->SetHeader("Device-Id", SystemInfo::GetMacAddress().c_str());
    http->SetHeader("Client-Id", board.GetUuid());
    if (has_serial_number_) {
        http->SetHeader("Serial-Number", serial_number_.c_str());
        ESP_LOGI(TAG, "Setup HTTP, User-Agent: %s, Serial-Number: %s", user_agent.c_str(),
                 MaskSecret(serial_number_).c_str());
    }
    http->SetHeader("User-Agent", user_agent);
    http->SetHeader("Accept-Language", Lang::CODE);
    http->SetHeader("Content-Type", "application/json");

    return http;
}

/* 
 * Specification: https://ccnphfhqs21z.feishu.cn/wiki/FjW6wZmisimNBBkov6OcmfvknVd
 */
NetworkResult<> Ota::CheckVersion() {
    auto& board = Board::GetInstance();
    auto app_desc = esp_app_get_description();

    // Check if there is a new firmware version available
    current_version_ = app_desc->version;
    ESP_LOGI(TAG, "Current version: %s", current_version_.c_str());

    std::string url = GetCheckVersionUrl();
    if (url.length() < 10) {
        ESP_LOGE(TAG, "Check version URL is not properly set");
        return std::unexpected(NetworkError::InvalidArgument());
    }

    auto http = SetupHttp();

    std::string data = board.GetSystemInfoJson();
    std::string method = data.length() > 0 ? "POST" : "GET";
    http->SetContent(std::move(data));

    if (auto opened = http->Open(method, url); !opened) {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", opened.error().ToString().c_str());
        return opened;
    }

    auto status_code = http->GetStatusCode();
    if (!status_code) {
        ESP_LOGE(TAG, "Failed to read HTTP status: %s", status_code.error().ToString().c_str());
        return std::unexpected(status_code.error());
    }
    if (*status_code != 200) {
        ESP_LOGE(TAG, "Failed to check version, status code: %d", *status_code);
        return std::unexpected(NetworkError::HttpFailed(*status_code));
    }

    size_t content_length = http->GetBodyLength();
    if (content_length == 0) {
        ESP_LOGE(TAG, "Failed to get content length");
        http->Close();
        return std::unexpected(NetworkError::ProtocolError());
    }

    data = http->ReadAll();
    http->Close();
    if (data.size() > content_length * 2) {
        ESP_LOGE(TAG, "Response body exceeds expected length");
        return std::unexpected(NetworkError::ProtocolError());
    }

    // Response: { "firmware": { "version": "1.0.0", "url": "http://" } }
    // Parse the JSON response and check if the version is newer
    // If it is, set has_new_version_ to true and store the new version and URL

    CJsonUniquePtr root(cJSON_Parse(data.c_str()));
    if (root == nullptr) {
        ESP_LOGE(TAG, "Failed to parse JSON response");
        return std::unexpected(NetworkError::ProtocolError());
    }

    has_activation_code_ = false;
    has_activation_challenge_ = false;
    cJSON *activation = cJSON_GetObjectItem(root.get(), "activation");
    if (cJSON_IsObject(activation)) {
        cJSON* message = cJSON_GetObjectItem(activation, "message");
        if (cJSON_IsString(message)) {
            activation_message_ = message->valuestring;
        }
        cJSON* code = cJSON_GetObjectItem(activation, "code");
        if (cJSON_IsString(code)) {
            activation_code_ = code->valuestring;
            has_activation_code_ = true;
        }
        cJSON* challenge = cJSON_GetObjectItem(activation, "challenge");
        if (cJSON_IsString(challenge)) {
            activation_challenge_ = challenge->valuestring;
            has_activation_challenge_ = true;
        }
        cJSON* timeout_ms = cJSON_GetObjectItem(activation, "timeout_ms");
        if (cJSON_IsNumber(timeout_ms)) {
            activation_timeout_ms_ = timeout_ms->valueint;
        }
    }

    has_mqtt_config_ = false;
    cJSON *mqtt = cJSON_GetObjectItem(root.get(), "mqtt");
    if (cJSON_IsObject(mqtt)) {
        Settings settings("mqtt", true);
        cJSON *item = NULL;
        cJSON_ArrayForEach(item, mqtt) {
            if (!IsAllowedServerKey(item->string, kAllowedMqttKeys)) {
                continue;
            }
            if (cJSON_IsString(item)) {
                if (settings.GetString(item->string) != item->valuestring) {
                    settings.SetString(item->string, item->valuestring);
                }
            } else if (cJSON_IsNumber(item)) {
                if (settings.GetInt(item->string) != item->valueint) {
                    settings.SetInt(item->string, item->valueint);
                }
            }
        }
        has_mqtt_config_ = true;
    } else {
        ESP_LOGI(TAG, "No mqtt section found !");
    }

    has_websocket_config_ = false;
    cJSON *websocket = cJSON_GetObjectItem(root.get(), "websocket");
    if (cJSON_IsObject(websocket)) {
        Settings settings("websocket", true);
        cJSON *item = NULL;
        cJSON_ArrayForEach(item, websocket) {
            if (!IsAllowedServerKey(item->string, kAllowedWebsocketKeys)) {
                continue;
            }
            if (cJSON_IsString(item)) {
                if (settings.GetString(item->string) != item->valuestring) {
                    settings.SetString(item->string, item->valuestring);
                }
            } else if (cJSON_IsNumber(item)) {
                if (settings.GetInt(item->string) != item->valueint) {
                    settings.SetInt(item->string, item->valueint);
                }
            }
        }
        has_websocket_config_ = true;
    } else {
        ESP_LOGI(TAG, "No websocket section found!");
    }

    has_server_time_ = false;
    cJSON* server_time = cJSON_GetObjectItem(root.get(), "server_time");
    if (cJSON_IsObject(server_time)) {
        cJSON *timestamp = cJSON_GetObjectItem(server_time, "timestamp");
        cJSON *timezone_offset = cJSON_GetObjectItem(server_time, "timezone_offset");
        
        if (cJSON_IsNumber(timestamp)) {
            // 设置系统时间
            struct timeval tv;
            double ts = timestamp->valuedouble;
            
            // 如果有时区偏移，计算本地时间
            if (cJSON_IsNumber(timezone_offset)) {
                ts += (timezone_offset->valueint * 60 * 1000); // 转换分钟为毫秒
            }
            
            tv.tv_sec = (time_t)(ts / 1000);  // 转换毫秒为秒
            tv.tv_usec = (suseconds_t)((long long)ts % 1000) * 1000;  // 剩余的毫秒转换为微秒
            settimeofday(&tv, NULL);
            has_server_time_ = true;
        }
    } else {
        ESP_LOGW(TAG, "No server_time section found!");
    }

    has_new_version_ = false;
    cJSON* firmware = cJSON_GetObjectItem(root.get(), "firmware");
    if (cJSON_IsObject(firmware)) {
        cJSON *version = cJSON_GetObjectItem(firmware, "version");
        if (cJSON_IsString(version)) {
            firmware_version_ = version->valuestring;
        }
        cJSON *url = cJSON_GetObjectItem(firmware, "url");
        if (cJSON_IsString(url)) {
            firmware_url_ = url->valuestring;
        }

        if (cJSON_IsString(version) && cJSON_IsString(url)) {
            // Check if the version is newer, for example, 0.1.0 is newer than 0.0.1
            has_new_version_ = IsNewVersionAvailable(current_version_, firmware_version_);
            if (has_new_version_) {
                ESP_LOGI(TAG, "New version available: %s", firmware_version_.c_str());
            } else {
                ESP_LOGI(TAG, "Current is the latest version");
            }
            // If the force flag is set to 1, the given version is forced to be installed
            cJSON *force = cJSON_GetObjectItem(firmware, "force");
            if (cJSON_IsNumber(force) && force->valueint == 1) {
                has_new_version_ = true;
            }
        }
    } else {
        ESP_LOGW(TAG, "No firmware section found!");
    }

    // root освобождается деструктором CJsonUniquePtr; явный cJSON_Delete был бы
    // двойным освобождением.
    return {};
}

void Ota::MarkCurrentVersionValid() {
    auto partition = esp_ota_get_running_partition();
    if (strcmp(partition->label, "factory") == 0) {
        ESP_LOGI(TAG, "Running from factory partition, skipping");
        return;
    }

    ESP_LOGI(TAG, "Running partition: %s", partition->label);
    esp_ota_img_states_t state;
    if (esp_ota_get_state_partition(partition, &state) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get state of partition");
        return;
    }

    if (state == ESP_OTA_IMG_PENDING_VERIFY) {
        ESP_LOGI(TAG, "Marking firmware as valid");
        esp_ota_mark_app_valid_cancel_rollback();
    }
}

bool Ota::Upgrade(const std::string& firmware_url, std::function<void(int progress, size_t speed)> callback) {
    ESP_LOGI(TAG, "Upgrading firmware from %s", firmware_url.c_str());
    esp_ota_handle_t update_handle = 0;
    auto update_partition = esp_ota_get_next_update_partition(NULL);
    if (update_partition == NULL) {
        ESP_LOGE(TAG, "Failed to get update partition");
        return false;
    }

    ESP_LOGI(TAG, "Writing to partition %s at offset 0x%lx", update_partition->label, update_partition->address);
    bool image_header_checked = false;
    std::string image_header;

    auto network = Board::GetInstance().GetNetwork();
    auto http = network->CreateHttp(0);
    if (auto opened = http->Open("GET", firmware_url); !opened) {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", opened.error().ToString().c_str());
        return false;
    }

    auto status_code = http->GetStatusCode();
    if (!status_code) {
        ESP_LOGE(TAG, "Failed to read HTTP status: %s", status_code.error().ToString().c_str());
        return false;
    }
    if (*status_code != 200) {
        ESP_LOGE(TAG, "Failed to get firmware, status code: %d", *status_code);
        return false;
    }

    size_t content_length = http->GetBodyLength();
    if (content_length == 0) {
        ESP_LOGE(TAG, "Failed to get content length");
        return false;
    }
    // Образ должен помещаться в раздел обновления: иначе esp_ota_write выйдет за его
    // границы и запись будет повреждена либо прошивка не запустится после активации.
    const size_t partition_size = update_partition->size;
    if (content_length > partition_size) {
        ESP_LOGE(TAG, "Firmware image too large: %u bytes, partition %s holds %u bytes",
                 static_cast<unsigned>(content_length), update_partition->label,
                 static_cast<unsigned>(partition_size));
        return false;
    }

    constexpr size_t PAGE_SIZE = 4096;
    char* buffer = (char*)heap_caps_malloc(PAGE_SIZE, MALLOC_CAP_INTERNAL);
    if (buffer == nullptr) {
        ESP_LOGE(TAG, "Failed to allocate buffer");
        return false;
    }

    size_t buffer_offset = 0;  // Current data size in buffer
    size_t total_read = 0, recent_read = 0;
    auto last_calc_time = esp_timer_get_time();
    while (true) {
        auto ret = http->Read(buffer + buffer_offset, PAGE_SIZE - buffer_offset);
        if (!ret) {
            ESP_LOGE(TAG, "Failed to read HTTP data: %s", ret.error().ToString().c_str());
            heap_caps_free(buffer);
            return false;
        }
        int n = *ret;

        // Сервер может отдать больше данных, чем объявлено в Content-Length: без этой
        // проверки лишние байты уйдут за пределы раздела обновления.
        if (total_read + static_cast<size_t>(n) > content_length) {
            ESP_LOGE(TAG, "Firmware body longer than Content-Length: got %u bytes, declared %u",
                     static_cast<unsigned>(total_read + n), static_cast<unsigned>(content_length));
            esp_ota_abort(update_handle);
            heap_caps_free(buffer);
            return false;
        }

        // Calculate speed and progress every second
        recent_read += n;
        total_read += n;
        buffer_offset += n;
        if (esp_timer_get_time() - last_calc_time >= 1000000 || n == 0) {
            size_t progress = total_read * 100 / content_length;
            ESP_LOGI(TAG, "Progress: %u%% (%u/%u), Speed: %uB/s", progress, total_read, content_length, recent_read);
            if (callback) {
                callback(progress, recent_read);
            }
            last_calc_time = esp_timer_get_time();
            recent_read = 0;
        }

        if (!image_header_checked) {
            image_header.append(buffer, buffer_offset);
            if (image_header.size() >= sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t)) {
                esp_app_desc_t new_app_info;
                memcpy(&new_app_info, image_header.data() + sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t), sizeof(esp_app_desc_t));

                if (esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES, &update_handle)) {
                    esp_ota_abort(update_handle);
                    ESP_LOGE(TAG, "Failed to begin OTA");
                    heap_caps_free(buffer);
                    return false;
                }

                image_header_checked = true;
                std::string().swap(image_header);
            }
        }

        // Write to flash when buffer is full (4KB) or it's the last chunk
        bool is_last_chunk = (n == 0);
        if (buffer_offset == PAGE_SIZE || (is_last_chunk && buffer_offset > 0)) {
            auto err = esp_ota_write(update_handle, buffer, buffer_offset);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to write OTA data: %s", esp_err_to_name(err));
                esp_ota_abort(update_handle);
                heap_caps_free(buffer);
                return false;
            }

            buffer_offset = 0;
        }

        if (is_last_chunk) {
            break;
        }
    }
    http->Close();
    heap_caps_free(buffer);

    esp_err_t err = esp_ota_end(update_handle);
    if (err != ESP_OK) {
        if (err == ESP_ERR_OTA_VALIDATE_FAILED) {
            ESP_LOGE(TAG, "Image validation failed, image is corrupted");
        } else {
            ESP_LOGE(TAG, "Failed to end OTA: %s", esp_err_to_name(err));
        }
        return false;
    }

    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set boot partition: %s", esp_err_to_name(err));
        return false;
    }

    ESP_LOGI(TAG, "Firmware upgrade successful");
    return true;
}

bool Ota::StartUpgrade(std::function<void(int progress, size_t speed)> callback) {
    return Upgrade(firmware_url_, callback);
}


std::vector<int> Ota::ParseVersion(const std::string& version) {
    std::vector<int> versionNumbers;
    size_t start = 0;

    // Сегменты разбираем строго: пустой сегмент или нечисловой ("1.x", "latest")
    // означают некорректный ответ сервера, а не ноль.
    while (start <= version.size()) {
        size_t dot = version.find('.', start);
        std::string segment =
            dot == std::string::npos ? version.substr(start) : version.substr(start, dot - start);

        int value = 0;
        if (!ParseInt(segment, value, 0, 999999)) {
            ESP_LOGW(TAG, "Malformed version string: %s", version.c_str());
            return {};
        }
        versionNumbers.push_back(value);

        if (dot == std::string::npos) {
            break;
        }
        start = dot + 1;
    }

    return versionNumbers;
}

bool Ota::IsNewVersionAvailable(const std::string& currentVersion, const std::string& newVersion) {
    std::vector<int> current = ParseVersion(currentVersion);
    std::vector<int> newer = ParseVersion(newVersion);
    
    for (size_t i = 0; i < std::min(current.size(), newer.size()); ++i) {
        if (newer[i] > current[i]) {
            return true;
        } else if (newer[i] < current[i]) {
            return false;
        }
    }
    
    return newer.size() > current.size();
}

std::string Ota::GetActivationPayload() {
    if (!has_serial_number_) {
        return "{}";
    }

    std::string hmac_hex;
#ifdef SOC_HMAC_SUPPORTED
    uint8_t hmac_result[32]; // SHA-256 输出为32字节
    
    // 使用Key0计算HMAC
    esp_err_t ret = esp_hmac_calculate(HMAC_KEY0, (uint8_t*)activation_challenge_.data(), activation_challenge_.size(), hmac_result);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "HMAC calculation failed: %s", esp_err_to_name(ret));
        return "{}";
    }

    for (size_t i = 0; i < sizeof(hmac_result); i++) {
        char buffer[3];
        sprintf(buffer, "%02x", hmac_result[i]);
        hmac_hex += buffer;
    }
#endif

    CJsonUniquePtr payload(cJSON_CreateObject());
    if (payload == nullptr) {
        return "{}";
    }
    cJSON_AddStringToObject(payload.get(), "algorithm", "hmac-sha256");
    cJSON_AddStringToObject(payload.get(), "serial_number", serial_number_.c_str());
    cJSON_AddStringToObject(payload.get(), "challenge", activation_challenge_.c_str());
    cJSON_AddStringToObject(payload.get(), "hmac", hmac_hex.c_str());
    auto json_str = cJSON_PrintUnformatted(payload.get());
    std::string json(json_str != nullptr ? json_str : "");
    if (json_str != nullptr) {
        cJSON_free(json_str);
    }

    // Тело payload содержит serial_number и hmac, поэтому в лог идёт только длина,
    // алгоритм и маскированный HMAC.
    ESP_LOGI(TAG, "Activation payload prepared: %u bytes, algorithm=hmac-sha256, hmac=%s",
             static_cast<unsigned>(json.size()), MaskSecret(hmac_hex).c_str());
    return json;
}

esp_err_t Ota::Activate() {
    if (!has_activation_challenge_) {
        ESP_LOGW(TAG, "No activation challenge found");
        return ESP_FAIL;
    }

    std::string url = GetCheckVersionUrl();
    if (url.empty()) {
        ESP_LOGE(TAG, "Check version URL is empty");
        return ESP_FAIL;
    }
    if (url.back() != '/') {
        url += "/activate";
    } else {
        url += "activate";
    }

    auto http = SetupHttp();

    std::string data = GetActivationPayload();
    http->SetContent(std::move(data));

    if (auto opened = http->Open("POST", url); !opened) {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", opened.error().ToString().c_str());
        return ESP_FAIL;
    }

    auto status_code = http->GetStatusCode();
    if (!status_code) {
        ESP_LOGE(TAG, "Failed to read HTTP status: %s", status_code.error().ToString().c_str());
        return ESP_FAIL;
    }
    if (*status_code == 202) {
        return ESP_ERR_TIMEOUT;
    }
    if (*status_code != 200) {
        ESP_LOGE(TAG, "Failed to activate, code: %d, body: %s", *status_code, http->ReadAll().c_str());
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Activation successful");
    return ESP_OK;
}
