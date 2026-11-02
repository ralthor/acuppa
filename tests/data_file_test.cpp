#include "acuppa/data_file.hpp"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace fs = std::filesystem;
using acuppa::SaveKey;
using acuppa::SaveValue;

namespace {

void expect(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

void expect_throw(const std::function<void()>& action, const std::string& message,
                  const std::string& needle = {}) {
    try { action(); }
    catch (const std::exception& error) {
        if (!needle.empty() && std::string(error.what()).find(needle) == std::string::npos)
            throw std::runtime_error(message + " (error was: " + error.what() + ")");
        return;
    }
    throw std::runtime_error(message);
}

SaveValue integer(int64_t v) { SaveValue r; r.type = SaveValue::Type::Integer; r.integer = v; return r; }
SaveValue number(double v) { SaveValue r; r.type = SaveValue::Type::Number; r.number = v; return r; }
SaveValue text(std::string v) { SaveValue r; r.type = SaveValue::Type::String; r.text = std::move(v); return r; }
SaveValue boolean(bool v) { SaveValue r; r.type = SaveValue::Type::Boolean; r.boolean = v; return r; }
SaveKey key(std::string v) { SaveKey k; k.text = std::move(v); return k; }
SaveKey index(int64_t v) { SaveKey k; k.integer = true; k.number = v; return k; }
SaveValue table(std::vector<std::pair<SaveKey, SaveValue>> entries) {
    SaveValue r; r.entries = std::move(entries); return r;
}

bool same_key(const SaveKey& a, const SaveKey& b) {
    return a.integer == b.integer && (a.integer ? a.number == b.number : a.text == b.text);
}

const SaveValue* find(const SaveValue& t, const SaveKey& k) {
    for (const auto& [key, value] : t.entries) if (same_key(key, k)) return &value;
    return nullptr;
}

bool equal(const SaveValue& a, const SaveValue& b) {
    if (a.type != b.type) return false;
    switch (a.type) {
    case SaveValue::Type::Boolean: return a.boolean == b.boolean;
    case SaveValue::Type::Integer: return a.integer == b.integer;
    case SaveValue::Type::Number:
        return a.number == b.number && std::signbit(a.number) == std::signbit(b.number);
    case SaveValue::Type::String: return a.text == b.text;
    case SaveValue::Type::Table:
        if (a.entries.size() != b.entries.size()) return false;
        for (const auto& [k, v] : a.entries) {
            const SaveValue* other = find(b, k);
            if (!other || !equal(v, *other)) return false;
        }
        return true;
    }
    return false;
}

std::string slurp(const fs::path& file) {
    std::ifstream input(file, std::ios::binary);
    std::stringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

void spit(const fs::path& file, const std::string& bytes) {
    fs::create_directories(file.parent_path());
    std::ofstream(file, std::ios::binary) << bytes;
}

}  // namespace

int main(int argc, char* argv[]) {
    try {
        if (argc != 2) throw std::runtime_error("temporary directory required");
        const fs::path work = argv[1];
        fs::remove_all(work);
        fs::create_directories(work / "project");
        const fs::path root = fs::canonical(work / "project");

        // Exact formatting: arrays first and inline, sorted keys, quoted odd keys.
        const SaveValue small = table({
            {key("zeta"), integer(1)}, {key("alpha"), table({{index(1), number(0.5)},
                                                             {index(2), integer(-2)}})},
            {key("odd key"), boolean(true)}, {key("end"), text("x")},
            {key("nested"), table({{index(1), table({{key("b"), integer(2)}, {key("a"), integer(1)}})},
                                   {index(5), text("sparse")}})},
            {key("empty"), table({})}});
        const std::string expected =
            "return {\n"
            "  alpha = {0.5, -2},\n"
            "  empty = {},\n"
            "  [\"end\"] = \"x\",\n"
            "  nested = {\n"
            "    {\n"
            "      a = 1,\n"
            "      b = 2,\n"
            "    },\n"
            "    [5] = \"sparse\",\n"
            "  },\n"
            "  [\"odd key\"] = true,\n"
            "  zeta = 1,\n"
            "}\n";
        expect(acuppa::serialize_data(small) == expected,
               "unexpected data file text:\n" + acuppa::serialize_data(small));
        expect(equal(acuppa::parse_data(expected, "small.lua"), small), "small table did not round-trip");

        // Numbers and strings round-trip exactly with shortest text.
        const std::vector<double> numbers = {0.1, -0.0, 1e300, 5e-324, 9007199254740992.0,
                                             123456789.123, -2.5e-8, 1.0 / 3.0, 2.0};
        std::vector<std::pair<SaveKey, SaveValue>> list;
        for (double value : numbers) list.emplace_back(index(int64_t(list.size() + 1)), number(value));
        list.emplace_back(index(int64_t(list.size() + 1)), integer(std::numeric_limits<int64_t>::min()));
        list.emplace_back(index(int64_t(list.size() + 1)), integer(std::numeric_limits<int64_t>::max()));
        list.emplace_back(index(int64_t(list.size() + 1)),
                          text(std::string("q\"b\\s\n\r\t\0\x01\x7f caf\xc3\xa9 ]]", 20)));
        const SaveValue mixed = table(list);
        const std::string mixed_text = acuppa::serialize_data(mixed);
        expect(mixed_text.find("0.1, -0.0, 1e+300, 5e-324, 9007199254740992.0, 123456789.123") !=
                   std::string::npos, "numbers are not in shortest form: " + mixed_text);
        expect(equal(acuppa::parse_data(mixed_text, "mixed.lua"), mixed), "numbers or strings did not round-trip");

        // Serializer rejections.
        expect_throw([] { acuppa::serialize_data(integer(1)); }, "non-table data accepted");
        expect_throw([&] { acuppa::serialize_data(table({{key("x"), number(NAN)}})); }, "NaN accepted");
        expect_throw([&] { acuppa::serialize_data(table({{key("x"), integer(1)}, {key("x"), integer(2)}})); },
                     "duplicate key accepted");
        SaveValue deep = table({});
        for (int i = 0; i < 20; ++i) deep = table({{key("d"), deep}});
        expect_throw([&] { acuppa::serialize_data(deep); }, "deep table accepted");

        // Parser rejections.
        expect_throw([] { acuppa::parse_data("return {", "levels/bad.lua"); }, "malformed accepted", "levels/bad.lua");
        expect_throw([] { acuppa::parse_data("\x1bLua\x54\x00", "levels/code.lua"); }, "bytecode accepted", "levels/code.lua");
        expect_throw([] { acuppa::parse_data("return 5", "levels/five.lua"); }, "non-table accepted", "must return a table");
        expect_throw([] { acuppa::parse_data("return {require('os')}", "levels/req.lua"); }, "require reachable", "levels/req.lua");
        expect(equal(acuppa::parse_data("return {g = print == nil and string == nil and _G == nil}", "g.lua"),
                     table({{key("g"), boolean(true)}})), "globals reachable from data");
        expect_throw([] { acuppa::parse_data("return {f = function() end}", "levels/fn.lua"); }, "function accepted");
        expect_throw([] { acuppa::parse_data("while true do end", "levels/loop.lua"); }, "loop accepted", "instruction limit");
        expect_throw([] { acuppa::parse_data("local s = 'x' for i = 1, 40 do s = s .. s end return {}", "levels/mem.lua"); },
                     "memory bomb accepted", "memory");
        expect_throw([] { acuppa::parse_data("return " + std::string(1024 * 1024, ' ') + "{}", "levels/big.lua"); },
                     "oversized text accepted", "1 MiB");
        std::string nested = "return ";
        for (int i = 0; i < 20; ++i) nested += "{";
        for (int i = 0; i < 20; ++i) nested += "}";
        expect_throw([&] { acuppa::parse_data(nested, "levels/deep.lua"); }, "deep data accepted");

        // Atomic write with permission preservation.
        acuppa::DataWritePolicy policy;
        policy.writable = {"levels"};
        policy.protected_files = {"levels/scene.lua"};
        acuppa::write_data_file(root, "levels/one.lua", small, policy);
        expect(slurp(root / "levels/one.lua") == expected, "written file differs from serializer");
        fs::permissions(root / "levels/one.lua", fs::perms::owner_read | fs::perms::owner_write |
                                                     fs::perms::group_read);
        acuppa::write_data_file(root, "levels/one.lua", mixed, policy);
        expect(fs::status(root / "levels/one.lua").permissions() ==
                   (fs::perms::owner_read | fs::perms::owner_write | fs::perms::group_read),
               "overwrite did not preserve permissions");
        expect(equal(*acuppa::read_data_file(root, "levels/one.lua"), mixed), "read after overwrite differs");
        acuppa::write_data_file(root, "levels/deep/two.lua", small, policy);
        expect(slurp(root / "levels/deep/two.lua") == expected, "nested directory write failed");
        for (const auto& entry : fs::directory_iterator(root / "levels"))
            expect(entry.path().filename().string().find(".tmp.") == std::string::npos,
                   "temporary file left behind");

        // Every rejection leaves the existing file untouched.
        const std::string before = slurp(root / "levels/one.lua");
        spit(root / "game.lua", "return {}\n");
        spit(root / "levels/scene.lua", "return {entities = {}}\n");
        spit(root / "levels/script.lua", "return {start = function() end}\n");
        fs::create_directories(work / "outside");
        fs::create_directory_symlink(work / "outside", root / "levels/link");
        fs::create_symlink(work / "outside/target.lua", root / "levels/file_link.lua");
        const std::vector<std::pair<std::string, std::string>> rejected = {
            {"/tmp/abs.lua", "relative"}, {"../escape.lua", "'..'"}, {"levels/../game.lua", "'..'"},
            {"game.lua", "game.lua"}, {"data/x.lua", "writable"}, {"levels/scene.lua", "scene"},
            {"levels/x.txt", ".lua"}, {"levels/.hidden.lua", "hidden"}, {"levels//x.lua", "empty"},
            {"levels/link/x.lua", "symlink"}, {"levels/file_link.lua", "regular"},
            {"levels/script.lua", "non-data"}, {"", "non-empty"}, {"levels", ".lua"}};
        for (const auto& [path, needle] : rejected)
            expect_throw([&] { acuppa::write_data_file(root, path, small, policy); },
                         "write accepted " + path, needle);
        expect_throw([&] { acuppa::write_data_file(root, "levels/one.lua",
                                                   table({{key("x"), number(INFINITY)}}), policy); },
                     "infinite value accepted");
        expect(slurp(root / "levels/one.lua") == before, "failed write changed the existing file");
        expect(slurp(root / "levels/script.lua") == "return {start = function() end}\n", "script changed");
        expect(!fs::exists(work / "outside/x.lua") && !fs::exists(work / "outside/target.lua"),
               "write escaped through a symlink");
        expect(slurp(root / "game.lua") == "return {}\n", "game.lua changed");

        // Writable directory declarations.
        expect(acuppa::normalize_writable_directory("levels") == "levels", "levels rejected");
        expect(acuppa::normalize_writable_directory("data/levels") == "data/levels", "nested dir rejected");
        for (const char* bad : {"", "/abs", "../up", "a/../b", ".git", "saves", "tests/x", "build", "a\\b"})
            expect_throw([&] { acuppa::normalize_writable_directory(bad); },
                         std::string("writable accepted ") + bad);

        // Reads.
        expect(!acuppa::read_data_file(root, "levels/missing.lua"), "missing file was not nil");
        expect(!acuppa::read_data_file(root, "nowhere/missing.lua"), "missing directory was not nil");
        expect(acuppa::read_data_file(root, "game.lua").has_value(), "project data file unreadable");
        spit(root / "levels/broken.lua", "return {");
        spit(root / "levels/code.lua", std::string("\x1bLua\x54\x00\x19\x93", 8));
        spit(root / "levels/number.lua", "return 7\n");
        spit(root / "levels/huge.lua", "return {}" + std::string(1024 * 1024, ' '));
        for (const char* bad : {"levels/broken.lua", "levels/code.lua", "levels/number.lua",
                                "levels/huge.lua", "levels/script.lua"})
            expect_throw([&] { acuppa::read_data_file(root, bad); }, std::string("read accepted ") + bad, bad);
        expect_throw([&] { acuppa::read_data_file(root, "levels/link/x.lua"); }, "read followed dir symlink");
        expect_throw([&] { acuppa::read_data_file(root, "levels/file_link.lua"); }, "read followed file symlink");
        expect_throw([&] { acuppa::read_data_file(root, "../escape.lua"); }, "read escaped project");
        expect_throw([&] { acuppa::read_data_file(root, "/etc/passwd.lua"); }, "read absolute path");
        std::cout << "Data file serializer, sandbox, and path checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "data_file_test: " << error.what() << '\n';
        return 1;
    }
}
