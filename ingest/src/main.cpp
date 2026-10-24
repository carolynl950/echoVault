// echovault: ingest notes from Obsidian / Notion into Atlas.
#include <cstdlib>
#include <iostream>
#include <string_view>

#include <curl/curl.h>

#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/uri.hpp>

namespace {

int usage() {
    std::cerr << "usage:\n"
                 "  echovault ping                 check the Atlas connection (needs MONGODB_URI)\n"
                 "  echovault version              print library versions\n";
    return 2;
}

int version() {
    std::cout << "libcurl " << curl_version_info(CURLVERSION_NOW)->version << '\n';
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
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << '\n';
        return 1;
    }
    return usage();
}
