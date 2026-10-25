// Notion reader: finds the pages under chosen roots and fetches the ones that changed.
#pragma once

#include <chrono>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#include <bsoncxx/document/value.hpp>

#include "note.hpp"

namespace echovault {

struct NotionError : std::runtime_error {
    long status;
    NotionError(long status, const std::string& message) : std::runtime_error(message), status(status) {}
};

// Minimal Notion API client over libcurl. Paces requests under Notion's ~3/s limit and retries 429/5xx.
class NotionClient {
public:
    explicit NotionClient(std::string token);
    ~NotionClient();
    NotionClient(const NotionClient&) = delete;
    NotionClient& operator=(const NotionClient&) = delete;

    bsoncxx::document::value get(const std::string& path);
    bsoncxx::document::value post(const std::string& path, const std::string& body);
    int requestCount() const { return requests_; }

private:
    bsoncxx::document::value request(const std::string& method, const std::string& path, const std::string& body);

    void* curl_;  // CURL*, kept opaque so callers don't need curl.h
    std::string token_;
    int requests_ = 0;
    std::chrono::steady_clock::time_point last_{};
};

struct NotionReadStats {
    int pagesVisible = 0;      // everything the token can see
    int pagesUnderRoots = 0;
    int pagesFetched = 0;      // content downloaded (new or edited)
    int pagesUnchanged = 0;    // skipped by last_edited_time
    int duplicatesSkipped = 0; // identical bodies (unfilled templates)
};

// Notes for every page under `roots` (page titles, case-insensitive). `stored` maps page ID → last
// edited time from the previous sync; those pages come back with `unchanged = true` and no content.
std::vector<Note> readNotion(NotionClient& client, const std::vector<std::string>& roots,
                             const std::map<std::string, std::chrono::system_clock::time_point>& stored,
                             NotionReadStats& stats);

}  // namespace echovault
