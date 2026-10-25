// echovault: ingest notes from Obsidian / Notion into Atlas.
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <iostream>
#include <sstream>
#include <string_view>

#include <curl/curl.h>

#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/uri.hpp>

#include "notion.hpp"
#include "splitter.hpp"
#include "store.hpp"
#include "vault.hpp"

namespace {

int usage() {
    std::cerr << "usage:\n"
                 "  echovault ping                 check the Atlas connection (needs MONGODB_URI)\n"
                 "  echovault init                 create the collection and search indexes\n"
                 "  echovault ingest obsidian <vault-folder> [--name <source-name>]\n"
                 "                                 sync a vault to Atlas (name defaults to the folder name)\n"
                 "  echovault ingest notion --root <page title> [--root ...] [--name <source-name>]\n"
                 "                                 sync Notion pages under the given roots (needs NOTION_TOKEN)\n"
                 "  echovault version              print library versions\n"
                 "  echovault split <file.md>      show how a note is split into passages\n";
    return 2;
}

int version() {
    std::cout << "libcurl " << curl_version_info(CURLVERSION_NOW)->version << '\n';
    return 0;
}

int splitFile(const char* path) {
    std::ifstream in{path};
    if (!in) {
        std::cerr << "cannot open " << path << '\n';
        return 1;
    }
    std::stringstream buf;
    buf << in.rdbuf();

    auto result = echovault::split(echovault::toLines(buf.str()));
    if (!result.tags.empty()) {
        std::cout << "tags:";
        for (const auto& t : result.tags) std::cout << ' ' << t;
        std::cout << "\n";
    }
    std::cout << result.passages.size() << " passages\n";
    for (const auto& p : result.passages) {
        std::cout << "\n── lines " << p.lineStart << "-" << p.lineEnd << " · ~"
                  << echovault::estimateTokens(p.text) << " tokens · ";
        if (p.headingPath.empty()) std::cout << "(no heading)";
        for (std::size_t i = 0; i < p.headingPath.size(); ++i) std::cout << (i ? " › " : "") << p.headingPath[i];
        std::cout << "\n" << p.text << '\n';
    }
    return 0;
}

// Connects using MONGODB_URI; database name from ECHOVAULT_DB (default "echovault").
std::optional<mongocxx::client> connect() {
    const char* uri = std::getenv("MONGODB_URI");
    if (uri == nullptr || *uri == '\0') {
        std::cerr << "MONGODB_URI is not set\n";
        return std::nullopt;
    }
    return mongocxx::client{mongocxx::uri{uri}};
}

std::string dbName() {
    const char* name = std::getenv("ECHOVAULT_DB");
    return (name && *name) ? name : "echovault";
}

int init() {
    auto client = connect();
    if (!client) return 1;
    auto db = (*client)[dbName()];
    for (const auto& line : echovault::initIndexes(db)) std::cout << line << '\n';
    return 0;
}

int ingestObsidian(int argc, char** argv) {
    if (argc < 4) return usage();
    std::filesystem::path vault = argv[3];
    std::string name = std::filesystem::absolute(vault).lexically_normal().filename().string();
    if (name.empty()) name = std::filesystem::absolute(vault).parent_path().filename().string();  // trailing slash
    for (int i = 4; i + 1 < argc; ++i) {
        if (std::string_view(argv[i]) == "--name") name = argv[i + 1];
    }

    auto notes = echovault::readVault(vault);
    std::cout << "read " << notes.size() << " notes from " << vault.string() << " (source: " << name << ")\n";

    auto client = connect();
    if (!client) return 1;
    auto db = (*client)[dbName()];
    auto s = echovault::syncSource(db, {"obsidian", name}, notes);

    std::cout << "notes:    " << s.notesSeen << " seen, " << s.notesUnchanged << " unchanged, "
              << s.notesRelocated << " renamed/moved, " << s.notesRemoved << " removed\n"
              << "passages: " << s.passagesInserted << " inserted, " << s.passagesDeleted << " deleted, "
              << s.passagesKept << " kept\n";
    return 0;
}

int ingestNotion(int argc, char** argv) {
    std::vector<std::string> roots;
    std::string name = "notion";
    for (int i = 3; i + 1 < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg == "--root") roots.push_back(argv[++i]);
        else if (arg == "--name") name = argv[++i];
    }
    if (roots.empty()) return usage();

    const char* token = std::getenv("NOTION_TOKEN");
    if (token == nullptr || *token == '\0') {
        std::cerr << "NOTION_TOKEN is not set\n";
        return 1;
    }

    auto client = connect();
    if (!client) return 1;
    auto db = (*client)[dbName()];

    echovault::NotionClient notion{token};
    echovault::NotionReadStats rs;
    auto notes = echovault::readNotion(notion, roots, echovault::storedLastEdited(db, name), rs);
    std::cout << "notion:   " << rs.pagesVisible << " pages visible, " << rs.pagesUnderRoots << " under roots, "
              << rs.pagesFetched << " fetched, " << rs.pagesUnchanged << " unchanged, " << rs.duplicatesSkipped
              << " duplicates skipped (" << notion.requestCount() << " API requests)\n";

    auto s = echovault::syncSource(db, {"notion", name}, notes);
    std::cout << "notes:    " << s.notesSeen << " seen, " << s.notesUnchanged << " unchanged, "
              << s.notesRelocated << " renamed/moved, " << s.notesRemoved << " removed\n"
              << "passages: " << s.passagesInserted << " inserted, " << s.passagesDeleted << " deleted, "
              << s.passagesKept << " kept\n";
    return 0;
}

int ping() {
    using bsoncxx::builder::basic::kvp;
    using bsoncxx::builder::basic::make_document;

    auto client = connect();
    if (!client) return 1;
    auto admin = (*client)["admin"];
    admin.run_command(make_document(kvp("ping", 1)));
    auto info = admin.run_command(make_document(kvp("buildInfo", 1)));
    std::cout << "connected: MongoDB " << info.view()["version"].get_string().value << '\n';
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) return usage();
    std::string_view cmd = argv[1];

    mongocxx::instance instance{};
    try {
        if (cmd == "ping") return ping();
        if (cmd == "version") return version();
        if (cmd == "split" && argc >= 3) return splitFile(argv[2]);
        if (cmd == "init") return init();
        if (cmd == "ingest" && argc >= 3 && std::string_view(argv[2]) == "obsidian") return ingestObsidian(argc, argv);
        if (cmd == "ingest" && argc >= 3 && std::string_view(argv[2]) == "notion") return ingestNotion(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << '\n';
        return 1;
    }
    return usage();
}
