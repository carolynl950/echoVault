// Splitter: turns a Note's lines into Passages. Pure: no I/O, no database.
#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace echovault {

struct Passage {
    std::vector<std::string> headingPath;  // enclosing headings, outermost first; empty if none
    int lineStart = 0;                     // 1-based, inclusive, in the original note
    int lineEnd = 0;
    std::string text;                      // cleaned passage text (headings excluded)
};

struct SplitResult {
    std::vector<std::string> tags;  // from frontmatter, without leading '#'
    std::vector<Passage> passages;
};

// Rough token estimate (~4 characters per token), good enough for sizing Passages.
std::size_t estimateTokens(const std::string& text);

// Splits at headings, then splits sections over maxTokens at paragraph breaks.
SplitResult split(const std::vector<std::string>& lines, std::size_t maxTokens = 400);

// Splits raw note content into lines (handles \n and \r\n).
std::vector<std::string> toLines(const std::string& content);

}  // namespace echovault
