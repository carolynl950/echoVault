// echovault: ingest notes from Obsidian / Notion into Atlas.
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string_view>

#include <curl/curl.h>

#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/uri.hpp>

#include "splitter.hpp"

namespace {

int usage() {
    std::cerr << "usage:\n"
                 "  echovault ping                 check the Atlas connection (needs MONGODB_URI)\n"
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

int ping() {
    const char* uri = std::getenv("MONGODB_URI");
    if (uri == nullptr || *uri == '\0') {
        std::cerr << "MONGODB_URI is not set\n";
        return 1;
    }

    using bsoncxx::builder::basic::kvp;
    using bsoncxx::builder::basic::make_document;

    mongocxx::client client{mongocxx::uri{uri}};
    auto admin = client["admin"];
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
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << '\n';
        return 1;
    }
    return usage();
}
