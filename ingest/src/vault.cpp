#include "vault.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace fs = std::filesystem;

namespace echovault {

namespace {

bool isHidden(const fs::path& p) {
    auto name = p.filename().string();
    return !name.empty() && name.front() == '.';
}

std::string readFile(const fs::path& p) {
    std::ifstream in{p, std::ios::binary};
    if (!in) throw std::runtime_error("cannot read " + p.string());
    std::stringstream buf;
    buf << in.rdbuf();
    return buf.str();
}

}  // namespace

std::vector<Note> readVault(const fs::path& root) {
    if (!fs::is_directory(root)) throw std::runtime_error("not a folder: " + root.string());

    std::vector<Note> notes;
    for (auto it = fs::recursive_directory_iterator(root); it != fs::recursive_directory_iterator(); ++it) {
        const auto& entry = *it;
        if (isHidden(entry.path())) {
            if (entry.is_directory()) it.disable_recursion_pending();
            continue;
        }
        if (!entry.is_regular_file() || entry.path().extension() != ".md") continue;

        auto rel = fs::relative(entry.path(), root);
        Note note;
        note.noteId = rel.generic_string();
        note.title = rel.stem().string();
        for (const auto& part : rel.parent_path()) note.location.push_back(part.string());
        note.content = readFile(entry.path());
        note.mtime = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            std::chrono::file_clock::to_sys(entry.last_write_time()));
        notes.push_back(std::move(note));
    }

    std::sort(notes.begin(), notes.end(), [](const auto& a, const auto& b) { return a.noteId < b.noteId; });
    return notes;
}

}  // namespace echovault
