// A Note as read from any Source, ready to be split and synced.
#pragma once

#include <chrono>
#include <string>
#include <vector>

namespace echovault {

struct Note {
    std::string noteId;                 // Obsidian: path relative to the vault root. Notion: page ID
    std::string title;
    std::vector<std::string> location;  // Obsidian: folders. Notion: parent page/database titles
    std::string content;                // markdown-style text the splitter understands
    std::chrono::system_clock::time_point mtime;

    // Notion only: the block each line of `content` came from ("" for blank lines).
    std::vector<std::string> lineBlockIds;

    // Known unchanged since the last sync, so content wasn't fetched; sync leaves it as is.
    bool unchanged = false;
};

}  // namespace echovault
