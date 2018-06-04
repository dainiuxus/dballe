#ifndef DBALLE_DB_V7_CACHE_H
#define DBALLE_DB_V7_CACHE_H

#include <dballe/types.h>
#include <unordered_map>
#include <memory>
#include <vector>
#include <iosfwd>

namespace dballe {
struct Station;

namespace db {
namespace v7 {

struct StationCache
{
    std::unordered_map<int, dballe::Station*> by_id;

    StationCache() = default;
    StationCache(const StationCache&) = delete;
    StationCache(StationCache&&) = delete;
    StationCache& operator=(const StationCache&) = delete;
    StationCache& operator=(StationCache&&) = delete;
    ~StationCache();

    const dballe::Station* find_entry(int id) const;

    const dballe::Station* insert(const dballe::Station& e);
    const dballe::Station* insert(const dballe::Station& e, int id);
    const dballe::Station* insert(std::unique_ptr<dballe::Station> e);

    void clear();
};


struct LevTrEntry
{
    // Database ID
    int id = MISSING_INT;

    /// Vertical level or layer
    Level level;

    /// Time range
    Trange trange;

    LevTrEntry() = default;
    LevTrEntry(int id, const Level& level, const Trange& trange) : id(id), level(level), trange(trange) {}
    LevTrEntry(const Level& level, const Trange& trange) : level(level), trange(trange) {}
    LevTrEntry(const LevTrEntry&) = default;
    LevTrEntry(LevTrEntry&&) = default;
    LevTrEntry& operator=(const LevTrEntry&) = default;
    LevTrEntry& operator=(LevTrEntry&&) = default;

    bool operator==(const LevTrEntry& o) const;
    bool operator!=(const LevTrEntry& o) const;
};

std::ostream& operator<<(std::ostream&, const LevTrEntry&);

struct LevTrReverseIndex : public std::unordered_map<Level, std::vector<const LevTrEntry*>>
{
    int find_id(const LevTrEntry& st) const;
    void add(const LevTrEntry* st);
};


struct LevTrCache
{
    std::unordered_map<int, LevTrEntry*> by_id;
    LevTrReverseIndex reverse;

    LevTrCache() = default;
    LevTrCache(const LevTrCache&) = delete;
    LevTrCache(LevTrCache&&) = delete;
    LevTrCache& operator=(const LevTrCache&) = delete;
    LevTrCache& operator=(LevTrCache&&) = delete;
    ~LevTrCache();

    const LevTrEntry* find_entry(int id) const;

    const LevTrEntry* insert(const LevTrEntry& e);
    const LevTrEntry* insert(const LevTrEntry& e, int id);
    const LevTrEntry* insert(std::unique_ptr<LevTrEntry> e);

    int find_id(const LevTrEntry& e) const;

    void clear();
};

}
}
}

#endif
