// Toolchain smoke test: bsoncxx parses Notion-style JSON, which the Notion reader relies on.
#include <catch2/catch_test_macros.hpp>

#include <bsoncxx/json.hpp>
#include <bsoncxx/types.hpp>
#include <string>

TEST_CASE("bsoncxx parses Notion block JSON") {
    auto doc = bsoncxx::from_json(R"({
        "object": "block",
        "type": "heading_2",
        "heading_2": { "rich_text": [ { "plain_text": "Krebs cycle" } ] }
    })");
    auto text = doc.view()["heading_2"]["rich_text"][0]["plain_text"].get_string().value;
    REQUIRE(std::string(text) == "Krebs cycle");
}
