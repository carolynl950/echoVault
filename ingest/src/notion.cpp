#include "notion.hpp"

#include <algorithm>
#include <cctype>
#include <map>
#include <optional>
#include <set>
#include <thread>

#include <curl/curl.h>

#include <bsoncxx/json.hpp>
#include <bsoncxx/types.hpp>

#include "hash.hpp"
#include "notion_blocks.hpp"

namespace echovault {

namespace {

constexpr const char* kApi = "https://api.notion.com/v1";
constexpr const char* kVersion = "2022-06-28";
constexpr auto kMinInterval = std::chrono::milliseconds(350);

std::string str(bsoncxx::document::view doc, const char* key) {
    auto el = doc[key];
    return el && el.type() == bsoncxx::type::k_string ? std::string(el.get_string().value) : "";
}

bool flag(bsoncxx::document::view doc, const char* key) {
    auto el = doc[key];
    return el && el.type() == bsoncxx::type::k_bool && el.get_bool().value;
}

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

std::string trimmed(const std::string& s) {
    auto a = s.find_first_not_of(" \t\n");
    if (a == std::string::npos) return "";
    return s.substr(a, s.find_last_not_of(" \t\n") - a + 1);
}

size_t collect(char* data, size_t size, size_t n, void* out) {
    static_cast<std::string*>(out)->append(data, size * n);
    return size * n;
}

size_t readHeader(char* data, size_t size, size_t n, void* out) {
    std::string line(data, size * n);
    if (lower(line).rfind("retry-after:", 0) == 0) *static_cast<std::string*>(out) = trimmed(line.substr(12));
    return size * n;
}

// What the search endpoint tells us about one page or database.
struct Object {
    std::string id;
    bool isDatabase = false;
    std::string title;
    std::string parentType;  // "workspace" | "page_id" | "database_id" | "block_id"
    std::string parentId;
    std::chrono::system_clock::time_point lastEdited;
};

std::string pageTitle(bsoncxx::document::view page) {
    auto props = page["properties"];
    if (!props || props.type() != bsoncxx::type::k_document) return "";
    for (const auto& prop : props.get_document().value) {
        auto p = prop.get_document().value;
        if (str(p, "type") == "title") return trimmed(richText(p, "title"));
    }
    return "";
}

Object toObject(bsoncxx::document::view o) {
    Object obj;
    obj.id = str(o, "id");
    obj.isDatabase = str(o, "object") == "database";
    obj.title = obj.isDatabase ? trimmed(richText(o, "title")) : pageTitle(o);
    if (obj.title.empty() && !obj.isDatabase) obj.title = "Untitled";  // unnamed databases stay "" and are left out of Locations
    auto parent = o["parent"].get_document().value;
    obj.parentType = str(parent, "type");
    obj.parentId = str(parent, obj.parentType.c_str());
    obj.lastEdited = parseNotionTime(str(o, "last_edited_time"));
    return obj;
}

class Reader {
public:
    Reader(NotionClient& client, NotionReadStats& stats) : client_(client), stats_(stats) {}

    std::vector<Note> read(const std::vector<std::string>& roots,
                           const std::map<std::string, std::chrono::system_clock::time_point>& stored) {
        loadAll();

        std::set<std::string> rootIds;
        for (const auto& root : roots) rootIds.insert(findRoot(root));

        std::vector<Note> notes;
        for (const auto& [id, obj] : objects_) {
            if (obj.isDatabase) continue;
            auto location = locationUnder(id, rootIds);
            if (!location) continue;
            ++stats_.pagesUnderRoots;

            Note note;
            note.noteId = id;
            note.title = obj.title;
            note.location = *location;
            note.mtime = obj.lastEdited;

            auto it = stored.find(id);
            if (it != stored.end() && it->second == obj.lastEdited) {
                note.unchanged = true;
                ++stats_.pagesUnchanged;
            } else {
                auto lines = blocksToLines(fetchChildren(id));
                for (std::size_t i = 0; i < lines.lines.size(); ++i) {
                    if (i) note.content += '\n';
                    note.content += lines.lines[i];
                }
                note.lineBlockIds = std::move(lines.blockIds);
                ++stats_.pagesFetched;
            }
            notes.push_back(std::move(note));
        }
        return dropDuplicates(std::move(notes));
    }

private:
    // Every page and database the token can see, via paginated search.
    void loadAll() {
        std::optional<std::string> cursor;
        do {
            std::string body = R"({"page_size":100)";
            if (cursor) body += R"(,"start_cursor":")" + *cursor + "\"";
            body += "}";
            auto res = client_.post("/search", body);
            auto view = res.view();
            for (const auto& el : view["results"].get_array().value) {
                auto o = el.get_document().value;
                if (flag(o, "archived") || flag(o, "in_trash")) continue;
                auto obj = toObject(o);
                if (!obj.isDatabase) ++stats_.pagesVisible;
                objects_[obj.id] = std::move(obj);
            }
            cursor.reset();
            if (flag(view, "has_more")) cursor = str(view, "next_cursor");
        } while (cursor);
    }

    std::string findRoot(const std::string& title) {
        std::vector<std::string> matches;
        for (const auto& [id, obj] : objects_) {
            if (!obj.isDatabase && lower(obj.title) == lower(trimmed(title))) matches.push_back(id);
        }
        if (matches.empty()) throw std::runtime_error("no Notion page titled \"" + title + "\" is visible to the token");
        if (matches.size() > 1) {
            throw std::runtime_error(std::to_string(matches.size()) + " Notion pages are titled \"" + title +
                                     "\"; rename one or pass a more specific root");
        }
        return matches.front();
    }

    // Page or database ID that a block lives in (pages can sit inside toggles/columns).
    std::string blockOwner(const std::string& blockId) {
        auto cached = blockOwner_.find(blockId);
        if (cached != blockOwner_.end()) return cached->second;
        std::string id = blockId;
        for (int hops = 0; hops < 20; ++hops) {
            auto block = client_.get("/blocks/" + id);
            auto parent = block.view()["parent"].get_document().value;
            auto type = str(parent, "type");
            id = str(parent, type.c_str());
            if (type != "block_id") break;
        }
        return blockOwner_[blockId] = id;
    }

    // Titles from the root down to this page's parent, or nullopt if the page isn't under a root.
    std::optional<std::vector<std::string>> locationUnder(const std::string& pageId, const std::set<std::string>& rootIds) {
        std::vector<std::string> chain;  // parent titles, nearest first
        std::string id = pageId;
        for (int depth = 0; depth < 50; ++depth) {
            if (rootIds.count(id)) {
                std::reverse(chain.begin(), chain.end());
                return chain;
            }
            auto it = objects_.find(id);
            if (it == objects_.end()) return std::nullopt;
            const auto& obj = it->second;
            if (id != pageId && !obj.title.empty()) chain.push_back(obj.title);
            if (obj.parentType == "workspace") return std::nullopt;
            id = obj.parentType == "block_id" ? blockOwner(obj.parentId) : obj.parentId;
        }
        return std::nullopt;
    }

    // A page's content blocks, recursively; child pages/databases are Notes of their own.
    std::vector<Block> fetchChildren(const std::string& id) {
        std::vector<Block> blocks;
        std::optional<std::string> cursor;
        do {
            std::string path = "/blocks/" + id + "/children?page_size=100";
            if (cursor) path += "&start_cursor=" + *cursor;
            auto res = client_.get(path);
            auto view = res.view();
            for (const auto& el : view["results"].get_array().value) {
                auto b = el.get_document().value;
                auto type = str(b, "type");
                Block block{bsoncxx::document::value{b}, {}};
                if (flag(b, "has_children") && type != "child_page" && type != "child_database") {
                    block.children = fetchChildren(str(b, "id"));
                }
                blocks.push_back(std::move(block));
            }
            cursor.reset();
            if (flag(view, "has_more")) cursor = str(view, "next_cursor");
        } while (cursor);
        return blocks;
    }

    // Pages whose body is identical to another page's are unfilled template copies; skip all of them.
    std::vector<Note> dropDuplicates(std::vector<Note> notes) {
        std::map<std::string, int> counts;
        for (const auto& n : notes) {
            if (!n.unchanged && !n.content.empty()) ++counts[sha256Hex(n.content)];
        }
        std::vector<Note> kept;
        for (auto& n : notes) {
            if (!n.unchanged && !n.content.empty() && counts[sha256Hex(n.content)] > 1) {
                ++stats_.duplicatesSkipped;
                continue;
            }
            kept.push_back(std::move(n));
        }
        return kept;
    }

    NotionClient& client_;
    NotionReadStats& stats_;
    std::map<std::string, Object> objects_;
    std::map<std::string, std::string> blockOwner_;
};

}  // namespace

NotionClient::NotionClient(std::string token) : token_(std::move(token)) {
    static const bool initialized = (curl_global_init(CURL_GLOBAL_DEFAULT), true);
    (void)initialized;
    curl_ = curl_easy_init();
    if (!curl_) throw std::runtime_error("curl_easy_init failed");
}

NotionClient::~NotionClient() { curl_easy_cleanup(static_cast<CURL*>(curl_)); }

bsoncxx::document::value NotionClient::get(const std::string& path) { return request("GET", path, ""); }

bsoncxx::document::value NotionClient::post(const std::string& path, const std::string& body) {
    return request("POST", path, body);
}

bsoncxx::document::value NotionClient::request(const std::string& method, const std::string& path,
                                               const std::string& body) {
    auto* curl = static_cast<CURL*>(curl_);
    for (int attempt = 0;; ++attempt) {
        auto wait = last_ + kMinInterval - std::chrono::steady_clock::now();
        if (wait > std::chrono::steady_clock::duration::zero()) std::this_thread::sleep_for(wait);
        last_ = std::chrono::steady_clock::now();
        ++requests_;

        std::string response, retryAfter;
        curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, ("Authorization: Bearer " + token_).c_str());
        headers = curl_slist_append(headers, (std::string("Notion-Version: ") + kVersion).c_str());
        headers = curl_slist_append(headers, "Content-Type: application/json");

        curl_easy_reset(curl);
        curl_easy_setopt(curl, CURLOPT_URL, (std::string(kApi) + path).c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, collect);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, readHeader);
        curl_easy_setopt(curl, CURLOPT_HEADERDATA, &retryAfter);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);
        if (method == "POST") {
            curl_easy_setopt(curl, CURLOPT_POST, 1L);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
        }

        auto rc = curl_easy_perform(curl);
        long status = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
        curl_slist_free_all(headers);

        bool retryable = rc != CURLE_OK || status == 429 || status >= 500;
        if (retryable && attempt < 5) {
            int seconds = retryAfter.empty() ? (1 << attempt) : std::max(1, std::atoi(retryAfter.c_str()));
            std::this_thread::sleep_for(std::chrono::seconds(seconds));
            continue;
        }
        if (rc != CURLE_OK) throw NotionError(0, std::string("network error: ") + curl_easy_strerror(rc));
        if (status < 200 || status >= 300) {
            std::string message = response;
            try {
                message = str(bsoncxx::from_json(response).view(), "message");
            } catch (...) {
            }
            throw NotionError(status, "Notion " + std::to_string(status) + " on " + path + ": " + message);
        }
        return bsoncxx::from_json(response);
    }
}

std::vector<Note> readNotion(NotionClient& client, const std::vector<std::string>& roots,
                             const std::map<std::string, std::chrono::system_clock::time_point>& stored,
                             NotionReadStats& stats) {
    return Reader(client, stats).read(roots, stored);
}

}  // namespace echovault
