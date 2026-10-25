// Sync planning: decides which Passages to insert, delete, or keep. Pure: no database.
#pragma once

#include <set>
#include <string>
#include <vector>

#include "splitter.hpp"

namespace echovault {

struct HashedPassage {
    std::string hash;
    Passage passage;
};

struct PassageDiff {
    std::vector<HashedPassage> toInsert;  // new content
    std::vector<HashedPassage> toKeep;    // same content; metadata (line numbers, location) may need refreshing
    std::vector<std::string> toDelete;    // hashes no longer in the note
};

// Passage identity: hash of its heading path and text.
std::string passageHash(const Passage& p);

// Hashes each Passage; identical Passages within one Note collapse into the first.
std::vector<HashedPassage> hashPassages(const std::vector<Passage>& passages);

PassageDiff diffPassages(const std::set<std::string>& existing, const std::vector<HashedPassage>& current);

}  // namespace echovault
