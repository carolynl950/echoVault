// Notion blocks → markdown-style lines the splitter understands. Pure: no network.
#pragma once

#include <chrono>
#include <string>
#include <vector>

#include <bsoncxx/document/value.hpp>
#include <bsoncxx/document/view.hpp>

namespace echovault {

struct Block {
    bsoncxx::document::value json;  // one block object from the Notion API
    std::vector<Block> children;    // fetched separately; empty for child pages/databases
};

struct BlockLines {
    std::vector<std::string> lines;
    std::vector<std::string> blockIds;  // same length as lines; "" for separator lines
};

BlockLines blocksToLines(const std::vector<Block>& blocks);

// Concatenated plain_text of a Notion rich_text array.
std::string richText(bsoncxx::document::view parent, const char* field = "rich_text");

// Parses Notion timestamps like "2026-10-20T14:03:00.000Z".
std::chrono::system_clock::time_point parseNotionTime(const std::string& iso);

}  // namespace echovault
