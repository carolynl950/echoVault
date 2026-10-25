#include "store.hpp"

#include <map>
#include <set>

#include <bsoncxx/builder/basic/array.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/types.hpp>
#include <mongocxx/bulk_write.hpp>
#include <mongocxx/collection.hpp>
#include <mongocxx/model/delete_many.hpp>
#include <mongocxx/model/insert_one.hpp>
#include <mongocxx/model/update_one.hpp>
#include <mongocxx/options/index.hpp>
#include <mongocxx/options/replace.hpp>
#include <mongocxx/search_index_model.hpp>
#include <mongocxx/search_index_view.hpp>

#include "diff.hpp"
#include "hash.hpp"
#include "splitter.hpp"

namespace echovault {

namespace {

using bsoncxx::builder::basic::kvp;
using bsoncxx::builder::basic::make_array;
using bsoncxx::builder::basic::make_document;

// Kept in sync with docs/ARCHITECTURE.md → Indexes.
constexpr const char* kVectorIndex = "passages_vector";
constexpr const char* kVectorDefinition = R"({
  "fields": [
    { "type": "autoEmbed", "modality": "text", "path": "text", "model": "voyage-4-lite" }
  ]
})";

constexpr const char* kTextIndex = "passages_text";
constexpr const char* kTextDefinition = R"({
  "mappings": {
    "dynamic": false,
    "fields": {
      "text": { "type": "string" },
      "noteTitle": { "type": "string" },
      "headingPath": { "type": "string" }
    }
  }
})";

bsoncxx::array::value toArray(const std::vector<std::string>& items) {
    bsoncxx::builder::basic::array arr;
    for (const auto& s : items) arr.append(s);
    return arr.extract();
}

bsoncxx::document::value sourceDoc(const SourceRef& source) {
    return make_document(kvp("kind", source.kind), kvp("name", source.name));
}

// Notion Passages are cited by the block they start in (Notion has no line numbers).
std::string firstBlockId(const Note& note, const Passage& p) {
    for (auto i = static_cast<std::size_t>(p.lineStart - 1); i < note.lineBlockIds.size(); ++i) {
        if (!note.lineBlockIds[i].empty()) return note.lineBlockIds[i];
    }
    return "";
}

// Fields that can change without the Passage's content changing (e.g. a paragraph added above it).
bsoncxx::document::value passageMetadata(const Note& note, const std::vector<std::string>& tags,
                                         const Passage& p) {
    bsoncxx::builder::basic::document doc;
    doc.append(kvp("noteTitle", note.title), kvp("location", toArray(note.location)), kvp("tags", toArray(tags)),
               kvp("mtime", bsoncxx::types::b_date{note.mtime}));
    if (note.lineBlockIds.empty()) {
        doc.append(kvp("lines", make_document(kvp("start", p.lineStart), kvp("end", p.lineEnd))));
    } else {
        doc.append(kvp("blockId", firstBlockId(note, p)));
    }
    return doc.extract();
}

}  // namespace

std::vector<std::string> initIndexes(mongocxx::database& db) {
    std::vector<std::string> report;

    if (!db.has_collection("passages")) {
        db.create_collection("passages");
        report.push_back("created collection passages");
    }
    auto passages = db["passages"];

    mongocxx::options::index unique;
    unique.unique(true);
    passages.create_index(make_document(kvp("source.name", 1), kvp("noteId", 1), kvp("hash", 1)), unique);
    report.push_back("unique index {source.name, noteId, hash}: ok");

    auto searchIndexes = passages.search_indexes();
    std::set<std::string> existing;
    for (const auto& idx : searchIndexes.list()) existing.insert(std::string(idx["name"].get_string().value));

    auto ensure = [&](const char* name, const char* definition, const char* type) {
        if (existing.count(name)) {
            report.push_back(std::string(name) + ": already exists");
            return;
        }
        mongocxx::search_index_model model{name, bsoncxx::from_json(definition)};
        model.type(type);
        searchIndexes.create_one(model);
        report.push_back(std::string(name) + ": created (builds in the background, ~1 min)");
    };
    ensure(kVectorIndex, kVectorDefinition, "vectorSearch");
    ensure(kTextIndex, kTextDefinition, "search");

    return report;
}

std::map<std::string, std::chrono::system_clock::time_point> storedLastEdited(mongocxx::database& db,
                                                                             const std::string& sourceName) {
    std::map<std::string, std::chrono::system_clock::time_point> out;
    for (const auto& doc : db["notes"].find(make_document(kvp("source.name", sourceName)))) {
        out[std::string(doc["noteId"].get_string().value)] =
            std::chrono::system_clock::time_point{doc["lastEdited"].get_date().value};
    }
    return out;
}

SyncStats syncSource(mongocxx::database& db, const SourceRef& source, const std::vector<Note>& notes) {
    SyncStats stats;
    auto passagesCol = db["passages"];
    auto notesCol = db["notes"];

    // What Atlas currently has for this Source.
    struct StoredNote {
        std::string contentHash;
        std::string title;
        std::vector<std::string> location;
    };
    std::map<std::string, StoredNote> stored;
    for (const auto& doc : notesCol.find(make_document(kvp("source.name", source.name)))) {
        StoredNote sn{std::string(doc["contentHash"].get_string().value), std::string(doc["title"].get_string().value), {}};
        for (const auto& part : doc["location"].get_array().value) sn.location.emplace_back(part.get_string().value);
        stored[std::string(doc["noteId"].get_string().value)] = std::move(sn);
    }

    // An unchanged Note can still have been renamed or moved (Notion); refresh just those fields.
    auto refreshPlacement = [&](const Note& note, const StoredNote& sn) {
        if (note.title == sn.title && note.location == sn.location) return;
        auto set = make_document(kvp("$set", make_document(kvp("noteTitle", note.title), kvp("location", toArray(note.location)))));
        passagesCol.update_many(make_document(kvp("source.name", source.name), kvp("noteId", note.noteId)), set.view());
        notesCol.update_one(make_document(kvp("_id", source.name + ":" + note.noteId)),
                            make_document(kvp("$set", make_document(kvp("title", note.title), kvp("location", toArray(note.location))))));
        ++stats.notesRelocated;
    };

    std::set<std::string> seen;
    for (const auto& note : notes) {
        ++stats.notesSeen;
        seen.insert(note.noteId);
        auto it = stored.find(note.noteId);

        if (note.unchanged) {
            ++stats.notesUnchanged;
            if (it != stored.end()) refreshPlacement(note, it->second);
            continue;
        }

        auto contentHash = sha256Hex(note.content);
        if (it != stored.end() && it->second.contentHash == contentHash) {
            ++stats.notesUnchanged;
            refreshPlacement(note, it->second);
            continue;
        }

        auto split = echovault::split(toLines(note.content));
        auto current = hashPassages(split.passages);

        std::set<std::string> existing;
        for (const auto& doc : passagesCol.find(make_document(kvp("source.name", source.name), kvp("noteId", note.noteId)))) {
            existing.insert(std::string(doc["hash"].get_string().value));
        }
        auto diff = diffPassages(existing, current);

        auto bulk = passagesCol.create_bulk_write();
        bool hasOps = false;
        if (!diff.toDelete.empty()) {
            bsoncxx::builder::basic::array hashes;
            for (const auto& h : diff.toDelete) hashes.append(h);
            bulk.append(mongocxx::model::delete_many{make_document(
                kvp("source.name", source.name), kvp("noteId", note.noteId),
                kvp("hash", make_document(kvp("$in", hashes.extract()))))});
            hasOps = true;
        }
        for (const auto& hp : diff.toInsert) {
            bsoncxx::builder::basic::document doc;
            doc.append(kvp("source", sourceDoc(source)), kvp("noteId", note.noteId),
                       kvp("headingPath", toArray(hp.passage.headingPath)), kvp("text", hp.passage.text),
                       kvp("hash", hp.hash));
            auto metadata = passageMetadata(note, split.tags, hp.passage);  // must outlive the loop's view
            for (const auto& field : metadata.view()) {
                doc.append(kvp(field.key(), field.get_value()));
            }
            bulk.append(mongocxx::model::insert_one{doc.extract()});
            hasOps = true;
        }
        for (const auto& hp : diff.toKeep) {
            bulk.append(mongocxx::model::update_one{
                make_document(kvp("source.name", source.name), kvp("noteId", note.noteId), kvp("hash", hp.hash)),
                make_document(kvp("$set", passageMetadata(note, split.tags, hp.passage)))});
            hasOps = true;
        }
        if (hasOps) bulk.execute();

        mongocxx::options::replace upsert;
        upsert.upsert(true);
        notesCol.replace_one(
            make_document(kvp("_id", source.name + ":" + note.noteId)),
            make_document(kvp("source", sourceDoc(source)), kvp("noteId", note.noteId), kvp("title", note.title),
                          kvp("location", toArray(note.location)), kvp("content", note.content),
                          kvp("contentHash", contentHash), kvp("lastEdited", bsoncxx::types::b_date{note.mtime})),
            upsert);

        stats.passagesInserted += static_cast<int>(diff.toInsert.size());
        stats.passagesDeleted += static_cast<int>(diff.toDelete.size());
        stats.passagesKept += static_cast<int>(diff.toKeep.size());
    }

    // Notes that disappeared from the Source.
    bsoncxx::builder::basic::array removedIds, removedNoteIds;
    for (const auto& [noteId, _] : stored) {
        if (seen.count(noteId)) continue;
        removedIds.append(source.name + ":" + noteId);
        removedNoteIds.append(noteId);
        ++stats.notesRemoved;
    }
    if (stats.notesRemoved > 0) {
        auto deleted = passagesCol.delete_many(make_document(
            kvp("source.name", source.name), kvp("noteId", make_document(kvp("$in", removedNoteIds.extract())))));
        if (deleted) stats.passagesDeleted += static_cast<int>(deleted->deleted_count());
        notesCol.delete_many(make_document(kvp("_id", make_document(kvp("$in", removedIds.extract())))));
    }

    return stats;
}

}  // namespace echovault
