#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <string>
#include <vector>

#include <bsoncxx/json.hpp>

#include "notion_blocks.hpp"
#include "splitter.hpp"

using namespace echovault;
using Lines = std::vector<std::string>;

namespace {

std::string rt(const std::string& text) {
    return R"({"rich_text":[{"plain_text":")" + text + R"("}]})";
}

Block block(const std::string& id, const std::string& type, const std::string& data,
            std::vector<Block> children = {}) {
    auto json = R"({"id":")" + id + R"(","type":")" + type + R"(",")" + type + R"(":)" + data + "}";
    return Block{bsoncxx::from_json(json), std::move(children)};
}

}  // namespace

TEST_CASE("paragraphs and headings become markdown with blank lines between") {
    auto r = blocksToLines({
        block("h", "heading_2", rt("Krebs cycle")),
        block("p1", "paragraph", rt("Makes ATP.")),
        block("p2", "paragraph", rt("In the matrix.")),
    });
    CHECK(r.lines == Lines{"## Krebs cycle", "", "Makes ATP.", "", "In the matrix."});
    CHECK(r.blockIds == Lines{"h", "", "p1", "", "p2"});
}

TEST_CASE("consecutive list items stay in one paragraph, nested items indent") {
    auto r = blocksToLines({
        block("a", "bulleted_list_item", rt("TCP"), {block("a1", "bulleted_list_item", rt("reliable"))}),
        block("b", "bulleted_list_item", rt("UDP")),
        block("c", "to_do", R"({"checked":true,"rich_text":[{"plain_text":"study"}]})"),
        block("p", "paragraph", rt("After list.")),
    });
    CHECK(r.lines == Lines{"- TCP", "  - reliable", "- UDP", "- [x] study", "", "After list."});
}

TEST_CASE("code blocks are fenced so '#' comments aren't headings") {
    auto r = blocksToLines({block("c", "code", R"({"language":"python","rich_text":[{"plain_text":"# hi\nx = 1"}]})")});
    CHECK(r.lines == Lines{"```python", "# hi", "x = 1", "```"});
    auto split = echovault::split(r.lines);
    REQUIRE(split.passages.size() == 1);
    CHECK(split.passages[0].headingPath.empty());
}

TEST_CASE("toggle content and toggleable headings keep their children") {
    auto r = blocksToLines({
        block("t", "toggle", rt("Details"), {block("tc", "paragraph", rt("hidden text"))}),
        block("h", "heading_1", rt("Section"), {block("hc", "paragraph", rt("under heading"))}),
    });
    CHECK(r.lines == Lines{"Details", "  hidden text", "", "# Section", "under heading"});
    auto split = echovault::split(r.lines);
    REQUIRE(split.passages.size() == 2);
    CHECK(split.passages[1].headingPath == Lines{"Section"});
    CHECK(split.passages[1].text == "under heading");
}

TEST_CASE("columns are transparent; images, dividers, child pages produce nothing") {
    auto r = blocksToLines({
        block("cl", "column_list", "{}", {block("col", "column", "{}", {block("p", "paragraph", rt("in column"))})}),
        block("img", "image", R"({"type":"external"})"),
        block("d", "divider", "{}"),
        block("cp", "child_page", R"({"title":"Sub page"})"),
        block("p2", "paragraph", rt("end")),
    });
    CHECK(r.lines == Lines{"in column", "", "end"});
}

TEST_CASE("tables become pipe rows") {
    auto row = [](const std::string& id, const std::string& a, const std::string& b) {
        return Block{bsoncxx::from_json(R"({"id":")" + id + R"(","type":"table_row","table_row":{"cells":[[{"plain_text":")" +
                                        a + R"("}],[{"plain_text":")" + b + R"("}]]}})"),
                     {}};
    };
    auto r = blocksToLines({block("t", "table", R"({"table_width":2})", {row("r1", "Layer", "Protocol"), row("r2", "4", "TCP")})});
    CHECK(r.lines == Lines{"| Layer | Protocol |", "| 4 | TCP |"});
    CHECK(r.blockIds == Lines{"r1", "r2"});
}

TEST_CASE("multi-line paragraphs map every line to the block") {
    auto r = blocksToLines({block("p", "paragraph", rt("line one\\nline two"))});
    CHECK(r.lines == Lines{"line one", "line two"});
    CHECK(r.blockIds == Lines{"p", "p"});
}

TEST_CASE("empty paragraphs produce nothing") {
    auto r = blocksToLines({block("e", "paragraph", R"({"rich_text":[]})"), block("p", "paragraph", rt("x"))});
    CHECK(r.lines == Lines{"x"});
}

TEST_CASE("parseNotionTime") {
    auto t = parseNotionTime("2026-10-20T14:03:00.000Z");
    CHECK(std::chrono::system_clock::to_time_t(t) == 1792504980);
}
