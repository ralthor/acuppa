#pragma once

#include <filesystem>

void export_game(const std::filesystem::path& project,
                 const std::filesystem::path& destination,
                 const std::filesystem::path& player_binary);
