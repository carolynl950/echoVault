#include "notion_blocks.hpp"

#include <cstdio>
#include <ctime>
#include <stdexcept>

#include <bsoncxx/types.hpp>

namespace echovault {

namespace {

std::string str(bsoncxx::document::view doc, const char* key) {
    auto el = doc[key];
    return el && el.type() == bsoncxx::type::k_string ? std::string(el.get_string().value) : "";
}

bool isListItem(const std::string& type) {
    return type == "bulleted_list_item" || type == "numbered_list_item" || type == "to_do";
}

// Containers whose children read as if they were at the container's level.
bool isTransparent(const std::string& type) {
    return type == "column_list" || type == "column" || type == "synced_block";
}

std::vector<std::string> splitLines(const std::string& text) {
    std::vector<std::string> out;
    std::size_t pos = 0;
    while (true) {
        auto nl = text.find('\n', pos);
        out.push_back(text.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos));
        if (nl == std::string::npos) break;
        pos = nl + 1;
    }
    return out;
}

class Emitter {
public:
    BlockLines out;

    void emit(const std::vector<Block>& blocks, int depth) {
        for (std::size_t i = 0; i < blocks.size(); ++i) {
            const auto& block = blocks[i];
            auto view = block.json.view();
            auto type = str(view, "type");

            if (isTransparent(type)) {
                emit(block.children, depth);
                continue;
            }

            bool wrote = emitBlock(block, type, depth);

            // Blank line between top-level blocks so the splitter sees paragraphs;
            // consecutive list items stay together as one paragraph.
            if (wrote && depth == 0) {
                bool nextIsList = i + 1 < blocks.size() && isListItem(str(blocks[i + 1].json.view(), "type"));
                if (!(isListItem(type) && nextIsList)) add("", "");
            }
        }
    }

private:
    void add(std::string line, const std::string& id) {
        out.lines.push_back(std::move(line));
        out.blockIds.push_back(id);
    }

    // Writes one block (and its children). Returns false if the block produced no lines.
    bool emitBlock(const Block& block, const std::string& type, int depth) {
        auto view = block.json.view();
        auto id = str(view, "id");
        auto dataEl = view[type];
        if (!dataEl || dataEl.type() != bsoncxx::type::k_document) return false;
        auto data = dataEl.get_document().value;
        std::string indent(static_cast<std::size_t>(depth) * 2, ' ');
        auto before = out.lines.size();

        auto writeText = [&](const std::string& prefix, const std::string& text) {
            auto lines = splitLines(text);
            for (std::size_t i = 0; i < lines.size(); ++i) {
                add(indent + (i == 0 ? prefix : std::string(prefix.size(), ' ')) + lines[i], id);
            }
        };

        if (type == "heading_1" || type == "heading_2" || type == "heading_3") {
            auto text = richText(data);
            if (!text.empty()) add(std::string(static_cast<std::size_t>(type.back() - '0'), '#') + " " + text, id);
            emit(block.children, 0);  // toggleable heading: its content sits under it
            return out.lines.size() > before;
        }
        if (type == "code") {
            add(indent + "```" + str(data, "language"), id);
            for (const auto& line : splitLines(richText(data))) add(indent + line, id);
            add(indent + "```", id);
            return true;
        }
        if (type == "table") {
            for (const auto& row : block.children) {
                auto rowView = row.json.view();
                auto cellsEl = rowView["table_row"]["cells"];
                if (!cellsEl || cellsEl.type() != bsoncxx::type::k_array) continue;
                std::string line = "|";
                for (const auto& cell : cellsEl.get_array().value) {
                    std::string text;
                    for (const auto& rt : cell.get_array().value) text += str(rt.get_document().value, "plain_text");
                    line += " " + text + " |";
                }
                add(indent + line, str(rowView, "id"));
            }
            return out.lines.size() > before;
        }
        if (type == "equation") {
            auto expr = str(data, "expression");
            if (!expr.empty()) add(indent + expr, id);
            return out.lines.size() > before;
        }

        std::string prefix;
        if (type == "bulleted_list_item") prefix = "- ";
        else if (type == "numbered_list_item") prefix = "1. ";
        else if (type == "to_do") {
            auto checked = data["checked"];
            prefix = (checked && checked.get_bool().value) ? "- [x] " : "- [ ] ";
        } else if (type == "quote" || type == "callout") prefix = "> ";
        else if (type != "paragraph" && type != "toggle") return false;  // images, files, embeds, dividers…

        auto text = richText(data);
        if (!text.empty()) writeText(prefix, text);
        emit(block.children, depth + 1);
        return out.lines.size() > before;
    }
};

}  // namespace

std::string richText(bsoncxx::document::view parent, const char* field) {
    auto el = parent[field];
    if (!el || el.type() != bsoncxx::type::k_array) return "";
    std::string text;
    for (const auto& rt : el.get_array().value) text += str(rt.get_document().value, "plain_text");
    return text;
}

BlockLines blocksToLines(const std::vector<Block>& blocks) {
    Emitter e;
    e.emit(blocks, 0);
    while (!e.out.lines.empty() && e.out.lines.back().empty()) {
        e.out.lines.pop_back();
        e.out.blockIds.pop_back();
    }
    return e.out;
}

std::chrono::system_clock::time_point parseNotionTime(const std::string& iso) {
    std::tm tm{};
    int ms = 0;
    if (std::sscanf(iso.c_str(), "%d-%d-%dT%d:%d:%d.%dZ", &tm.tm_year, &tm.tm_mon, &tm.tm_mday, &tm.tm_hour,
                    &tm.tm_min, &tm.tm_sec, &ms) < 6) {
        throw std::runtime_error("bad Notion timestamp: " + iso);
    }
    tm.tm_year -= 1900;
    tm.tm_mon -= 1;
    return std::chrono::system_clock::from_time_t(timegm(&tm)) + std::chrono::milliseconds(ms);
}

}  // namespace echovault
