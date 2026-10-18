#include "threepoint/save_data.hpp"

#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace fs = std::filesystem;

void expect(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

int main(int argc, char* argv[]) {
    if (argc != 2) throw std::runtime_error("save test directory required");
    const fs::path work = argv[1];
    fs::remove_all(work);
    fs::create_directories(work / "project");
    threepoint::SaveValue root;
    threepoint::SaveValue number;
    number.type = threepoint::SaveValue::Type::Integer;
    number.integer = -42;
    threepoint::SaveKey key;
    key.text = "score";
    root.entries.emplace_back(key, number);
    threepoint::write_save(work / "project", "slot_1", root);
    auto restored = threepoint::read_save(work / "project", "slot_1");
    expect(restored && restored->entries.size() == 1 &&
           restored->entries[0].second.integer == -42, "save did not round trip integer data");
    number.integer = 99;
    root.entries[0].second = number;
    threepoint::write_save(work / "project", "slot_1", root);
    restored = threepoint::read_save(work / "project", "slot_1");
    expect(restored && restored->entries[0].second.integer == 99,
           "replacing a save did not preserve the new value");
    expect(!threepoint::read_save(work / "project", "missing"),
           "missing save slot did not return empty");
    bool rejected = false;
    try { threepoint::write_save(work / "project", "../escape", root); }
    catch (const std::invalid_argument&) { rejected = true; }
    expect(rejected, "unsafe save slot accepted");
    std::ofstream(work / "project/saves/broken.tpsave", std::ios::binary) << "bad";
    rejected = false;
    try { threepoint::read_save(work / "project", "broken"); }
    catch (const std::runtime_error&) { rejected = true; }
    expect(rejected, "malformed save file accepted");
    std::ofstream(work / "project/saves/oversized.tpsave", std::ios::binary)
        << std::string(1024 * 1024 + 1, 'x');
    rejected = false;
    try { threepoint::read_save(work / "project", "oversized"); }
    catch (const std::runtime_error&) { rejected = true; }
    expect(rejected, "oversized save file accepted");
    fs::create_symlink(work / "project/saves/slot_1.tpsave",
                       work / "project/saves/linked.tpsave");
    rejected = false;
    try { threepoint::read_save(work / "project", "linked"); }
    catch (const std::runtime_error&) { rejected = true; }
    expect(rejected, "symlinked save file accepted");
    fs::create_directories(work / "outside");
    fs::create_directories(work / "symlink_project");
    fs::create_directory_symlink(work / "outside", work / "symlink_project/saves");
    rejected = false;
    try { threepoint::write_save(work / "symlink_project", "slot", root); }
    catch (const std::runtime_error&) { rejected = true; }
    expect(rejected, "symlinked save directory accepted");
}
