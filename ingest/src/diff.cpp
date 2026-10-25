#include "diff.hpp"

#include "hash.hpp"

namespace echovault {

std::string passageHash(const Passage& p) {
    // Unit/record separators can't appear in note text, so fields can't run together.
    std::string key;
    for (const auto& h : p.headingPath) {
        key += h;
        key += '\x1f';
    }
    key += '\x1e';
    key += p.text;
    return sha256Hex(key);
}

std::vector<HashedPassage> hashPassages(const std::vector<Passage>& passages) {
    std::vector<HashedPassage> out;
    std::set<std::string> seen;
    for (const auto& p : passages) {
        auto hash = passageHash(p);
        if (seen.insert(hash).second) out.push_back(HashedPassage{hash, p});
    }
    return out;
}

PassageDiff diffPassages(const std::set<std::string>& existing, const std::vector<HashedPassage>& current) {
    PassageDiff diff;
    std::set<std::string> currentHashes;
    for (const auto& hp : current) {
        currentHashes.insert(hp.hash);
        (existing.count(hp.hash) ? diff.toKeep : diff.toInsert).push_back(hp);
    }
    for (const auto& hash : existing) {
        if (!currentHashes.count(hash)) diff.toDelete.push_back(hash);
    }
    return diff;
}

}  // namespace echovault
