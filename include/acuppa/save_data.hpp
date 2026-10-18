#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace acuppa {

struct SaveKey {
    bool integer = false;
    int64_t number = 0;
    std::string text;
};

struct SaveValue {
    enum class Type : uint8_t { Boolean = 1, Integer = 2, Number = 3, String = 4, Table = 5 };
    Type type = Type::Table;
    bool boolean = false;
    int64_t integer = 0;
    double number = 0.0;
    std::string text;
    std::vector<std::pair<SaveKey, SaveValue>> entries;
};

void write_save(const std::filesystem::path& project_root, const std::string& slot,
                const SaveValue& value);
std::optional<SaveValue> read_save(const std::filesystem::path& project_root,
                                   const std::string& slot);

}  // namespace acuppa
