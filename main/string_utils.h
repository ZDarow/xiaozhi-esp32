#ifndef STRING_UTILS_H
#define STRING_UTILS_H

#include <cerrno>
#include <climits>
#include <cstdlib>
#include <string>

// Разбор целого из строки без исключений. В проекте исключения выключены
// (CONFIG_COMPILER_CXX_EXCEPTIONS=n в sdkconfig.defaults), поэтому std::stoi на
// некорректных данных из ответа сервера приводит к abort() и перезагрузке.
// Возвращает false и не изменяет out, если строка не является числом или число
// вне диапазона [min_value, max_value].
inline bool ParseInt(const std::string& text, int& out, int min_value = INT_MIN,
                     int max_value = INT_MAX) {
    if (text.empty()) {
        return false;
    }

    // Разрешаем только десятичную запись с необязательным знаком: пробелы, пустые
    // сегменты и мусор вроде "0x10" или "12abc" отклоняем.
    size_t digits_start = (text[0] == '-' || text[0] == '+') ? 1 : 0;
    if (digits_start >= text.size()) {
        return false;
    }
    for (size_t i = digits_start; i < text.size(); ++i) {
        if (text[i] < '0' || text[i] > '9') {
            return false;
        }
    }

    errno = 0;
    char* end = nullptr;
    long value = std::strtol(text.c_str(), &end, 10);
    if (errno == ERANGE || end == nullptr || *end != '\0') {
        return false;
    }
    if (value < min_value || value > max_value) {
        return false;
    }

    out = static_cast<int>(value);
    return true;
}

#endif  // STRING_UTILS_H