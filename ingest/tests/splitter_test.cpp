#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "splitter.hpp"

using echovault::split;
using echovault::toLines;
using Path = std::vector<std::string>;

TEST_CASE("note without headings is one passage") {
    auto r = split(toLines("Just some thoughts.\nOn two lines."));
    REQUIRE(r.passages.size() == 1);
    CHECK(r.passages[0].headingPath.empty());
    CHECK(r.passages[0].text == "Just some thoughts.\nOn two lines.");
    CHECK(r.passages[0].lineStart == 1);
    CHECK(r.passages[0].lineEnd == 2);
}

TEST_CASE("empty note has no passages") {
    CHECK(split(toLines("")).passages.empty());
    CHECK(split(toLines("\n\n   \n")).passages.empty());
}

TEST_CASE("splits at headings and tracks the heading path") {
    auto r = split(toLines(
        "Intro text.\n"        // 1
        "# Biology\n"          // 2
        "Cells are small.\n"   // 3
        "## Krebs cycle\n"     // 4
        "Makes ATP.\n"         // 5
        "## Glycolysis\n"      // 6
        "Splits glucose.\n"    // 7
        "# History\n"          // 8
        "War of 1812.\n"));    // 9
    REQUIRE(r.passages.size() == 5);
    CHECK(r.passages[0].headingPath.empty());
    CHECK(r.passages[0].text == "Intro text.");
    CHECK(r.passages[1].headingPath == Path{"Biology"});
    CHECK(r.passages[2].headingPath == Path{"Biology", "Krebs cycle"});
    CHECK(r.passages[2].text == "Makes ATP.");
    CHECK(r.passages[2].lineStart == 5);
    CHECK(r.passages[3].headingPath == Path{"Biology", "Glycolysis"});
    CHECK(r.passages[4].headingPath == Path{"History"});
}

TEST_CASE("heading with no body produces no passage") {
    auto r = split(toLines("# Empty\n## Also empty\nBody.\n"));
    REQUIRE(r.passages.size() == 1);
    CHECK(r.passages[0].headingPath == Path{"Empty", "Also empty"});
}

TEST_CASE("skipped heading levels still nest correctly") {
    auto r = split(toLines("# A\n### C\nx\n## B\ny\n"));
    REQUIRE(r.passages.size() == 2);
    CHECK(r.passages[0].headingPath == Path{"A", "C"});
    CHECK(r.passages[1].headingPath == Path{"A", "B"});
}

TEST_CASE("'#tag' without a space is not a heading") {
    auto r = split(toLines("#todo buy milk\n"));
    REQUIRE(r.passages.size() == 1);
    CHECK(r.passages[0].headingPath.empty());
}

TEST_CASE("frontmatter is excluded and tags are kept") {
    SECTION("inline list") {
        auto r = split(toLines("---\ntitle: Cells\ntags: [bio101, \"#exam\"]\n---\nBody.\n"));
        CHECK(r.tags == Path{"bio101", "exam"});
        REQUIRE(r.passages.size() == 1);
        CHECK(r.passages[0].text == "Body.");
        CHECK(r.passages[0].lineStart == 5);
    }
    SECTION("block list") {
        auto r = split(toLines("---\ntags:\n  - bio101\n  - exam\nsource: class\n---\nBody.\n"));
        CHECK(r.tags == Path{"bio101", "exam"});
    }
    SECTION("single value") {
        auto r = split(toLines("---\ntags: bio101\n---\nBody.\n"));
        CHECK(r.tags == Path{"bio101"});
    }
    SECTION("'---' later in the note is not frontmatter") {
        auto r = split(toLines("Body.\n---\nMore.\n"));
        CHECK(r.tags.empty());
        REQUIRE(r.passages.size() == 1);
        CHECK(r.passages[0].text == "Body.\n---\nMore.");
    }
}

TEST_CASE("'#' inside a code fence is not a heading") {
    auto r = split(toLines(
        "# Python\n"
        "```python\n"
        "# this is a comment\n"
        "x = 1\n"
        "```\n"
        "After code.\n"));
    REQUIRE(r.passages.size() == 1);
    CHECK(r.passages[0].headingPath == Path{"Python"});
    CHECK(r.passages[0].text == "```python\n# this is a comment\nx = 1\n```\nAfter code.");
}

TEST_CASE("wikilinks become plain text, embeds and images are dropped") {
    auto r = split(toLines(
        "See [[Cell respiration]] and [[Krebs cycle|the Krebs cycle]].\n"
        "Also [[Notes#Week 2]].\n"
        "![[diagram.png]]\n"
        "![a cell](cell.png) Cells!\n"));
    REQUIRE(r.passages.size() == 1);
    CHECK(r.passages[0].text ==
          "See Cell respiration and the Krebs cycle.\n"
          "Also Notes.\n"
          "\n"
          " Cells!");
}

TEST_CASE("wikilinks inside code fences are left alone") {
    auto r = split(toLines("```\n[[not a link]]\n```\n"));
    REQUIRE(r.passages.size() == 1);
    CHECK(r.passages[0].text == "```\n[[not a link]]\n```");
}

TEST_CASE("passage that is only an image is dropped") {
    auto r = split(toLines("# Pics\n![[photo.jpg]]\n# Words\nHello.\n"));
    REQUIRE(r.passages.size() == 1);
    CHECK(r.passages[0].headingPath == Path{"Words"});
}

TEST_CASE("long sections split at paragraph breaks") {
    // Each paragraph is ~10 tokens (40 chars); limit 25 fits two per passage.
    std::string para(40, 'a');
    auto r = split(toLines(para + "\n\n" + para + "\n\n" + para + "\n\n" + para + "\n\n" + para), 25);
    REQUIRE(r.passages.size() == 3);
    CHECK(r.passages[0].text == para + "\n\n" + para);
    CHECK(r.passages[0].lineStart == 1);
    CHECK(r.passages[0].lineEnd == 3);
    CHECK(r.passages[1].lineStart == 5);
    CHECK(r.passages[2].text == para);
    CHECK(r.passages[2].lineStart == 9);
}

TEST_CASE("a single paragraph over the limit stays whole") {
    std::string para(400, 'a');
    auto r = split(toLines(para), 25);
    REQUIRE(r.passages.size() == 1);
    CHECK(r.passages[0].text == para);
}

TEST_CASE("blank lines inside a code fence don't split it") {
    std::string big(80, 'x');
    auto r = split(toLines("```\n" + big + "\n\n" + big + "\n```\n"), 25);
    REQUIRE(r.passages.size() == 1);
}

TEST_CASE("CRLF line endings") {
    auto r = split(toLines("# A\r\nBody.\r\n"));
    REQUIRE(r.passages.size() == 1);
    CHECK(r.passages[0].headingPath == Path{"A"});
    CHECK(r.passages[0].text == "Body.");
}
