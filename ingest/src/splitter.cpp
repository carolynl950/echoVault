#include "splitter.hpp"

#include <algorithm>
#include <regex>
#include <utility>

namespace echovault {

namespace {

struct Line {
    int number;        // 1-based
    std::string text;  // cleaned
    bool fenced;       // inside (or opening/closing) a code fence
};

struct Section {
    std::vector<std::string> headingPath;
    std::vector<Line> lines;
};

std::string trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t");
    return s.substr(start, end - start + 1);
}

bool isBlank(const std::string& s) { return trim(s).empty(); }

bool startsWith(const std::string& s, const std::string& prefix) {
    return s.compare(0, prefix.size(), prefix) == 0;
}

// Strips quotes and a leading '#' from a tag value.
std::string cleanTag(std::string tag) {
    tag = trim(tag);
    if (tag.size() >= 2 && (tag.front() == '"' || tag.front() == '\'') && tag.back() == tag.front()) {
        tag = tag.substr(1, tag.size() - 2);
    }
    if (!tag.empty() && tag.front() == '#') tag.erase(0, 1);
    return trim(tag);
}

void addCommaSeparated(const std::string& list, std::vector<std::string>& out) {
    std::size_t pos = 0;
    while (pos <= list.size()) {
        auto comma = list.find(',', pos);
        if (comma == std::string::npos) comma = list.size();
        auto tag = cleanTag(list.substr(pos, comma - pos));
        if (!tag.empty()) out.push_back(tag);
        pos = comma + 1;
    }
}

// Parses `tags:` from frontmatter lines: inline list, block list, or single value.
std::vector<std::string> parseTags(const std::vector<std::string>& fm) {
    std::vector<std::string> tags;
    for (std::size_t i = 0; i < fm.size(); ++i) {
        if (!startsWith(fm[i], "tags:")) continue;
        auto value = trim(fm[i].substr(5));
        if (value.empty()) {
            for (std::size_t j = i + 1; j < fm.size(); ++j) {
                auto item = trim(fm[j]);
                if (!startsWith(item, "-")) break;
                auto tag = cleanTag(item.substr(1));
                if (!tag.empty()) tags.push_back(tag);
            }
        } else if (value.front() == '[') {
            auto close = value.find(']');
            addCommaSeparated(value.substr(1, close == std::string::npos ? std::string::npos : close - 1), tags);
        } else {
            addCommaSeparated(value, tags);
        }
        break;
    }
    return tags;
}

// Returns the heading level (1-6) and sets `title`, or 0 if the line is not a heading.
int parseHeading(const std::string& line, std::string& title) {
    std::size_t level = 0;
    while (level < line.size() && line[level] == '#') ++level;
    if (level == 0 || level > 6) return 0;
    if (level < line.size() && line[level] != ' ' && line[level] != '\t') return 0;  // "#tag"
    title = trim(line.substr(level));
    // Optional closing hashes: "## Title ##"
    auto end = title.find_last_not_of('#');
    if (end != std::string::npos && end + 1 < title.size() && (title[end] == ' ' || title[end] == '\t')) {
        title = trim(title.substr(0, end));
    }
    return static_cast<int>(level);
}

bool isFence(const std::string& line, std::string& marker) {
    auto t = trim(line);
    if (startsWith(t, "```")) { marker = "```"; return true; }
    if (startsWith(t, "~~~")) { marker = "~~~"; return true; }
    return false;
}

// Obsidian syntax → plain text: drops embeds/images, unwraps [[wikilinks]].
std::string cleanLine(const std::string& line) {
    static const std::regex embed(R"(!\[\[[^\]]*\]\])");
    static const std::regex image(R"(!\[[^\]]*\]\([^)]*\))");
    static const std::regex wikilink(R"(\[\[([^\]|#]*)(?:#[^\]|]*)?(?:\|([^\]]*))?\]\])");

    std::string s = std::regex_replace(line, embed, "");
    s = std::regex_replace(s, image, "");

    std::string out;
    auto last = s.cbegin();
    for (std::sregex_iterator it(s.begin(), s.end(), wikilink), end; it != end; ++it) {
        const auto& m = *it;
        out.append(last, m[0].first);
        out += m[2].matched ? m[2].str() : m[1].str();
        last = m[0].second;
    }
    out.append(last, s.cend());
    return out;
}

std::string joinLines(const std::vector<Line>& lines, std::size_t from, std::size_t to) {
    std::string text;
    for (std::size_t i = from; i <= to; ++i) {
        if (i > from) text += '\n';
        text += lines[i].text;
    }
    return text;
}

// Breaks a section into paragraphs, then groups paragraphs into Passages up to maxTokens.
void flushSection(const Section& section, std::size_t maxTokens, std::vector<Passage>& out) {
    const auto& lines = section.lines;

    // Paragraphs as [first, last] line indices; blank lines outside fences separate them.
    std::vector<std::pair<std::size_t, std::size_t>> paragraphs;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        bool isBreak = !lines[i].fenced && isBlank(lines[i].text);
        if (isBreak) continue;
        if (!paragraphs.empty() && paragraphs.back().second + 1 == i) {
            paragraphs.back().second = i;
        } else {
            paragraphs.emplace_back(i, i);
        }
    }

    auto emit = [&](std::size_t from, std::size_t to) {
        out.push_back(Passage{section.headingPath, lines[from].number, lines[to].number, joinLines(lines, from, to)});
    };

    std::size_t i = 0;
    while (i < paragraphs.size()) {
        std::size_t from = paragraphs[i].first;
        std::size_t to = paragraphs[i].second;
        std::size_t j = i + 1;
        while (j < paragraphs.size() && estimateTokens(joinLines(lines, from, paragraphs[j].second)) <= maxTokens) {
            to = paragraphs[j].second;
            ++j;
        }
        emit(from, to);
        i = j;
    }
}

}  // namespace

std::size_t estimateTokens(const std::string& text) { return (text.size() + 3) / 4; }

std::vector<std::string> toLines(const std::string& content) {
    std::vector<std::string> lines;
    std::size_t pos = 0;
    while (pos <= content.size()) {
        auto nl = content.find('\n', pos);
        if (nl == std::string::npos) nl = content.size();
        auto line = content.substr(pos, nl - pos);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lines.push_back(std::move(line));
        pos = nl + 1;
    }
    return lines;
}

SplitResult split(const std::vector<std::string>& lines, std::size_t maxTokens) {
    SplitResult result;
    std::size_t start = 0;

    if (!lines.empty() && trim(lines[0]) == "---") {
        for (std::size_t j = 1; j < lines.size(); ++j) {
            auto t = trim(lines[j]);
            if (t == "---" || t == "...") {
                result.tags = parseTags(std::vector<std::string>(lines.begin() + 1, lines.begin() + j));
                start = j + 1;
                break;
            }
        }
    }

    std::vector<std::pair<int, std::string>> headingStack;  // (level, title)
    Section section;
    bool inFence = false;
    std::string fenceMarker;

    for (std::size_t i = start; i < lines.size(); ++i) {
        const auto& raw = lines[i];
        int number = static_cast<int>(i) + 1;

        std::string marker;
        if (isFence(raw, marker) && (!inFence || marker == fenceMarker)) {
            inFence = !inFence;
            fenceMarker = marker;
            section.lines.push_back(Line{number, raw, true});
            continue;
        }
        if (inFence) {
            section.lines.push_back(Line{number, raw, true});
            continue;
        }

        std::string title;
        if (int level = parseHeading(raw, title)) {
            flushSection(section, maxTokens, result.passages);
            while (!headingStack.empty() && headingStack.back().first >= level) headingStack.pop_back();
            headingStack.emplace_back(level, title);
            section = Section{};
            for (const auto& [_, t] : headingStack) section.headingPath.push_back(t);
            continue;
        }

        section.lines.push_back(Line{number, cleanLine(raw), false});
    }
    flushSection(section, maxTokens, result.passages);
    return result;
}

}  // namespace echovault
