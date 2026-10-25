// Store: everything that talks to Atlas.
#pragma once

#include <chrono>
#include <map>
#include <string>
#include <vector>

#include <mongocxx/database.hpp>

#include "vault.hpp"

namespace echovault {

struct SourceRef {
    std::string kind;  // "obsidian" | "notion"
    std::string name;  // e.g. "demo-vault"
};

struct SyncStats {
    int notesSeen = 0;
    int notesUnchanged = 0;
    int notesRemoved = 0;
    int notesRelocated = 0;  // unchanged content, but renamed or moved
    int passagesInserted = 0;
    int passagesDeleted = 0;
    int passagesKept = 0;
};

// Creates the passages collection, its unique index, and both search indexes if missing.
// Returns one line per index describing what happened.
std::vector<std::string> initIndexes(mongocxx::database& db);

// noteId → lastEdited for every Note of this Source already in Atlas.
std::map<std::string, std::chrono::system_clock::time_point> storedLastEdited(mongocxx::database& db,
                                                                             const std::string& sourceName);

// Makes Atlas match `notes` for this Source: Notes marked unchanged (or whose content hash matches)
// are skipped, changed ones diffed, missing ones removed.
SyncStats syncSource(mongocxx::database& db, const SourceRef& source, const std::vector<Note>& notes);

}  // namespace echovault
