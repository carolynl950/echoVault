// Vault reader: finds every Note in an Obsidian vault folder.
#pragma once

#include <filesystem>
#include <vector>

#include "note.hpp"

namespace echovault {

// All .md files under `root`, skipping hidden folders (.obsidian, .trash, .git). Sorted by noteId.
std::vector<Note> readVault(const std::filesystem::path& root);

}  // namespace echovault
