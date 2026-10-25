#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <vector>

#include "diff.hpp"
#include "hash.hpp"
#include "vault.hpp"

using namespace echovault;
namespace fs = std::filesystem;

namespace {

Passage passage(std::vector<std::string> path, std::string text, int line = 1) {
    return Passage{std::move(path), line, line, std::move(text)};
}

std::set<std::string> hashesOf(const std::vector<HashedPassage>& hps) {
    std::set<std::string> out;
    for (const auto& hp : hps) out.insert(hp.hash);
    return out;
}

void writeFile(const fs::path& p, const std::string& content) {
    fs::create_directories(p.parent_path());
    std::ofstream{p} << content;
}

}  // namespace

TEST_CASE("sha256Hex matches a known digest") {
    CHECK(sha256Hex("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

TEST_CASE("passage hash depends on heading path and text, not line numbers") {
    auto a = passageHash(passage({"Bio"}, "ATP", 3));
    CHECK(a == passageHash(passage({"Bio"}, "ATP", 40)));
    CHECK(a != passageHash(passage({"History"}, "ATP")));
    CHECK(a != passageHash(passage({"Bio"}, "ATP!")));
    CHECK(passageHash(passage({"a", "b"}, "c")) != passageHash(passage({"ab"}, "c")));
}

TEST_CASE("identical passages in one note collapse") {
    auto hps = hashPassages({passage({}, "same", 1), passage({}, "same", 5), passage({}, "other")});
    REQUIRE(hps.size() == 2);
    CHECK(hps[0].passage.lineStart == 1);
}

TEST_CASE("diff: first ingest inserts everything") {
    auto current = hashPassages({passage({}, "a"), passage({}, "b")});
    auto d = diffPassages({}, current);
    CHECK(d.toInsert.size() == 2);
    CHECK(d.toKeep.empty());
    CHECK(d.toDelete.empty());
}

TEST_CASE("diff: edited passage is delete + insert, others kept") {
    auto before = hashPassages({passage({}, "a"), passage({}, "b")});
    auto after = hashPassages({passage({}, "a"), passage({}, "b edited")});
    auto d = diffPassages(hashesOf(before), after);
    REQUIRE(d.toInsert.size() == 1);
    CHECK(d.toInsert[0].passage.text == "b edited");
    REQUIRE(d.toKeep.size() == 1);
    CHECK(d.toKeep[0].passage.text == "a");
    REQUIRE(d.toDelete.size() == 1);
    CHECK(d.toDelete[0] == before[1].hash);
}

TEST_CASE("diff: emptied note deletes everything") {
    auto before = hashPassages({passage({}, "a")});
    auto d = diffPassages(hashesOf(before), {});
    CHECK(d.toDelete.size() == 1);
    CHECK(d.toInsert.empty());
}

TEST_CASE("readVault finds notes, skips hidden folders and non-markdown") {
    auto root = fs::temp_directory_path() / "echovault_test_vault";
    fs::remove_all(root);
    writeFile(root / "Top.md", "top");
    writeFile(root / "Classes" / "Biology" / "Cells.md", "cells");
    writeFile(root / ".obsidian" / "workspace.md", "hidden");
    writeFile(root / ".trash" / "Old.md", "trashed");
    writeFile(root / "Classes" / "diagram.png", "png");

    auto notes = readVault(root);
    REQUIRE(notes.size() == 2);
    CHECK(notes[0].noteId == "Classes/Biology/Cells.md");
    CHECK(notes[0].title == "Cells");
    CHECK(notes[0].location == std::vector<std::string>{"Classes", "Biology"});
    CHECK(notes[0].content == "cells");
    CHECK(notes[1].noteId == "Top.md");
    CHECK(notes[1].location.empty());

    fs::remove_all(root);
}

TEST_CASE("readVault rejects a missing folder") {
    CHECK_THROWS(readVault("/definitely/not/a/vault"));
}
