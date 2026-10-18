#include "threepoint/save_data.hpp"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cmath>
#include <cstring>
#include <fcntl.h>
#include <set>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>

namespace threepoint {
namespace {

constexpr size_t max_bytes = 1024 * 1024;
constexpr unsigned max_depth = 16;
constexpr size_t max_entries = 10000;

void check_slot(const std::string& slot) {
    if (slot.empty() || slot.size() > 64 ||
        !std::all_of(slot.begin(), slot.end(), [](unsigned char c) {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                   (c >= '0' && c <= '9') || c == '_' || c == '-';
        }))
        throw std::invalid_argument("save slot must use 1-64 letters, digits, underscores, or hyphens");
}

struct FileDescriptor {
    int fd = -1;
    explicit FileDescriptor(int value) : fd(value) {}
    ~FileDescriptor() { if (fd >= 0) close(fd); }
    FileDescriptor(const FileDescriptor&) = delete;
    FileDescriptor& operator=(const FileDescriptor&) = delete;
};

int open_save_dir(const std::filesystem::path& root, bool create) {
    const auto directory = root / "saves";
    if (create) {
        std::error_code error;
        std::filesystem::create_directory(directory, error);
        if (error) throw std::runtime_error("cannot create project save directory: " + error.message());
    }
    const int fd = open(directory.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) {
        if (!create && errno == ENOENT) return -1;
        throw std::runtime_error("project save directory is missing or unsafe");
    }
    return fd;
}

void append_byte(std::string& out, uint8_t value) {
    if (out.size() >= max_bytes) throw std::invalid_argument("save data exceeds 1 MiB");
    out.push_back(char(value));
}

void append_u64(std::string& out, uint64_t value) {
    for (int i = 0; i < 8; ++i) append_byte(out, uint8_t(value >> (i * 8)));
}

void append_u32(std::string& out, uint32_t value) {
    for (int i = 0; i < 4; ++i) append_byte(out, uint8_t(value >> (i * 8)));
}

void append_string(std::string& out, const std::string& value) {
    if (value.size() > max_bytes || out.size() + value.size() + 4 > max_bytes)
        throw std::invalid_argument("save data exceeds 1 MiB");
    append_u32(out, uint32_t(value.size()));
    out.append(value);
}

void encode(std::string& out, const SaveValue& value, unsigned depth, size_t& entries) {
    if (depth > max_depth) throw std::invalid_argument("save table nesting exceeds 16 levels");
    append_byte(out, uint8_t(value.type));
    switch (value.type) {
    case SaveValue::Type::Boolean: append_byte(out, value.boolean ? 1 : 0); break;
    case SaveValue::Type::Integer: append_u64(out, uint64_t(value.integer)); break;
    case SaveValue::Type::Number: {
        if (!std::isfinite(value.number)) throw std::invalid_argument("save number must be finite");
        uint64_t bits;
        std::memcpy(&bits, &value.number, sizeof(bits));
        append_u64(out, bits);
        break;
    }
    case SaveValue::Type::String: append_string(out, value.text); break;
    case SaveValue::Type::Table:
        if (entries + value.entries.size() > max_entries)
            throw std::invalid_argument("save table exceeds 10000 entries");
        entries += value.entries.size();
        append_u32(out, uint32_t(value.entries.size()));
        for (const auto& [key, child] : value.entries) {
            append_byte(out, key.integer ? 1 : 0);
            if (key.integer) append_u64(out, uint64_t(key.number));
            else append_string(out, key.text);
            encode(out, child, depth + 1, entries);
        }
        break;
    default: throw std::invalid_argument("unsupported save value type");
    }
}

class Reader {
public:
    explicit Reader(const std::string& bytes) : bytes_(bytes) {}
    bool done() const { return offset_ == bytes_.size(); }
    uint8_t byte() {
        if (offset_ >= bytes_.size()) throw std::runtime_error("truncated save data");
        return uint8_t(bytes_[offset_++]);
    }
    uint32_t u32() {
        uint32_t value = 0;
        for (int i = 0; i < 4; ++i) value |= uint32_t(byte()) << (i * 8);
        return value;
    }
    uint64_t u64() {
        uint64_t value = 0;
        for (int i = 0; i < 8; ++i) value |= uint64_t(byte()) << (i * 8);
        return value;
    }
    int64_t i64() {
        const uint64_t bits = u64();
        int64_t value;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }
    std::string string() {
        const uint32_t size = u32();
        if (size > bytes_.size() - offset_) throw std::runtime_error("truncated save string");
        std::string result = bytes_.substr(offset_, size);
        offset_ += size;
        return result;
    }
    SaveValue value(unsigned depth, size_t& entries) {
        if (depth > max_depth) throw std::runtime_error("save table nesting exceeds 16 levels");
        SaveValue result;
        result.type = SaveValue::Type(byte());
        switch (result.type) {
        case SaveValue::Type::Boolean: {
            const uint8_t flag = byte();
            if (flag > 1) throw std::runtime_error("invalid save boolean");
            result.boolean = flag != 0;
            break;
        }
        case SaveValue::Type::Integer: result.integer = i64(); break;
        case SaveValue::Type::Number: {
            const uint64_t bits = u64();
            std::memcpy(&result.number, &bits, sizeof(bits));
            if (!std::isfinite(result.number)) throw std::runtime_error("invalid save number");
            break;
        }
        case SaveValue::Type::String: result.text = string(); break;
        case SaveValue::Type::Table: {
            const uint32_t count = u32();
            if (count > max_entries - entries) throw std::runtime_error("save table exceeds entry limit");
            entries += count;
            std::set<std::pair<bool, std::string>> keys;
            for (uint32_t i = 0; i < count; ++i) {
                SaveKey key;
                const uint8_t tag = byte();
                if (tag > 1) throw std::runtime_error("invalid save key type");
                key.integer = tag == 1;
                if (key.integer) key.number = i64();
                else key.text = string();
                const std::string token = key.integer
                    ? std::string(reinterpret_cast<const char*>(&key.number), sizeof(key.number))
                    : key.text;
                if (!keys.emplace(key.integer, token).second)
                    throw std::runtime_error("duplicate save table key");
                result.entries.emplace_back(std::move(key), value(depth + 1, entries));
            }
            break;
        }
        default: throw std::runtime_error("invalid save value type");
        }
        return result;
    }
private:
    const std::string& bytes_;
    size_t offset_ = 0;
};

}  // namespace

void write_save(const std::filesystem::path& project_root, const std::string& slot,
                const SaveValue& value) {
    check_slot(slot);
    std::string bytes = "TPS1";
    size_t entries = 0;
    encode(bytes, value, 0, entries);
    FileDescriptor directory(open_save_dir(project_root, true));
    const std::string file = slot + ".tpsave";
    static std::atomic<uint64_t> next_temp{0};
    std::string temp;
    int fd = -1;
    for (int attempt = 0; attempt < 100 && fd < 0; ++attempt) {
        temp = slot + ".tmp." + std::to_string(getpid()) + "." +
               std::to_string(next_temp++);
        fd = openat(directory.fd, temp.c_str(),
                    O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
        if (fd < 0 && errno != EEXIST) break;
    }
    if (fd < 0) throw std::runtime_error("cannot create temporary save file");
    FileDescriptor output(fd);
    try {
        size_t offset = 0;
        while (offset < bytes.size()) {
            const ssize_t written = write(output.fd, bytes.data() + offset, bytes.size() - offset);
            if (written < 0 && errno == EINTR) continue;
            if (written <= 0) throw std::runtime_error("cannot write save file");
            offset += size_t(written);
        }
        if (fsync(output.fd) != 0 || renameat(directory.fd, temp.c_str(),
                                             directory.fd, file.c_str()) != 0)
            throw std::runtime_error("cannot finish save file");
        fsync(directory.fd);
    } catch (...) {
        unlinkat(directory.fd, temp.c_str(), 0);
        throw;
    }
}

std::optional<SaveValue> read_save(const std::filesystem::path& project_root,
                                   const std::string& slot) {
    check_slot(slot);
    FileDescriptor directory(open_save_dir(project_root, false));
    if (directory.fd < 0) return std::nullopt;
    const std::string file = slot + ".tpsave";
    const int fd = openat(directory.fd, file.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) {
        if (errno == ENOENT) return std::nullopt;
        throw std::runtime_error("cannot read save file");
    }
    FileDescriptor input(fd);
    struct stat info {};
    if (fstat(input.fd, &info) != 0 || !S_ISREG(info.st_mode) ||
        info.st_size < 5 || info.st_size > off_t(max_bytes))
        throw std::runtime_error("save file is invalid or exceeds 1 MiB");
    std::string bytes(size_t(info.st_size), '\0');
    size_t offset = 0;
    while (offset < bytes.size()) {
        const ssize_t count = read(input.fd, bytes.data() + offset, bytes.size() - offset);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) throw std::runtime_error("truncated save file");
        offset += size_t(count);
    }
    if (bytes.compare(0, 4, "TPS1") != 0) throw std::runtime_error("invalid save file version");
    Reader reader(bytes);
    for (int i = 0; i < 4; ++i) reader.byte();
    size_t entries = 0;
    SaveValue result = reader.value(0, entries);
    if (!reader.done()) throw std::runtime_error("trailing save data");
    return result;
}

}  // namespace threepoint
