#ifndef DBALLE_DB_SUMMARY_H
#define DBALLE_DB_SUMMARY_H

#include <dballe/core/fwd.h>
#include <dballe/core/defs.h>
#include <dballe/core/query.h>
#include <dballe/core/smallset.h>
#include <dballe/db/db.h>
#include <vector>
#include <set>
#include <functional>

namespace dballe {
namespace db {
namespace summary {

/**
 * Description of a variable, independent of where and when it was measured
 */
struct VarDesc
{
    dballe::Level level;
    dballe::Trange trange;
    wreport::Varcode varcode;

    VarDesc() = default;

    VarDesc(const dballe::Level& level, const dballe::Trange& trange, wreport::Varcode varcode)
        : level(level), trange(trange), varcode(varcode) {}

    VarDesc(const VarDesc&) = default;

    bool operator==(const VarDesc& o) const { return std::tie(level, trange, varcode) == std::tie(o.level, o.trange, o.varcode); }
    bool operator!=(const VarDesc& o) const { return std::tie(level, trange, varcode) != std::tie(o.level, o.trange, o.varcode); }
    bool operator< (const VarDesc& o) const { return std::tie(level, trange, varcode) <  std::tie(o.level, o.trange, o.varcode); }
    bool operator<=(const VarDesc& o) const { return std::tie(level, trange, varcode) <= std::tie(o.level, o.trange, o.varcode); }
    bool operator> (const VarDesc& o) const { return std::tie(level, trange, varcode) >  std::tie(o.level, o.trange, o.varcode); }
    bool operator>=(const VarDesc& o) const { return std::tie(level, trange, varcode) >= std::tie(o.level, o.trange, o.varcode); }
};

/**
 * Statistics about a variable
 */
struct VarEntry
{
    VarDesc var;

    dballe::DatetimeRange dtrange;
    size_t count = 0;

    VarEntry() = default;

    VarEntry(const VarDesc& var, const dballe::DatetimeRange& dtrange, size_t count)
        : var(var), dtrange(dtrange), count(count)
    {
    }

    VarEntry(const VarEntry&) = default;

    bool operator==(const VarEntry& o) const { return std::tie(var, dtrange, count) == std::tie(o.var, o.dtrange, o.count); }
    bool operator!=(const VarEntry& o) const { return std::tie(var, dtrange, count) != std::tie(o.var, o.dtrange, o.count); }

    void merge(const dballe::DatetimeRange& dtrange, size_t count)
    {
        this->dtrange.merge(dtrange);
        this->count += count;
    }

    void to_json(core::JSONWriter& writer) const;
    static VarEntry from_json(core::json::Stream& in);

    DBALLE_TEST_ONLY void dump(FILE* out) const;
};


inline const VarDesc& station_entry_get_value(const VarEntry& item) { return item.var; }

/**
 * Information about a station, and statistics about its variables.
 *
 * It behaves similarly to a std::vector<VarEntry>
 */
template<typename Station>
struct StationEntry : protected core::SmallSet<VarEntry, VarDesc, station_entry_get_value>
{
    using SmallSet::iterator;
    using SmallSet::const_iterator;
    using SmallSet::reverse_iterator;
    using SmallSet::const_reverse_iterator;
    using SmallSet::begin;
    using SmallSet::end;
    using SmallSet::rbegin;
    using SmallSet::rend;
    using SmallSet::size;
    using SmallSet::empty;
    using SmallSet::add;
    bool operator==(const StationEntry& o) const { return SmallSet::operator==(o); }
    bool operator!=(const StationEntry& o) const { return SmallSet::operator!=(o); }

    Station station;

    StationEntry() = default;

    template<typename OStation>
    StationEntry(const Station& station, const StationEntry<OStation>& entry)
        : station(station)
    {
        for (const auto& item: entry)
            this->add(item);
    }

    StationEntry(const Station& station, const VarDesc& vd, const dballe::DatetimeRange& dtrange, size_t count)
        : station(station)
    {
        add(vd, dtrange, count);
    }

    StationEntry(const StationEntry& entries, const dballe::Query& query)
        : station(entries.station)
    {
        add_filtered(entries, query);
    }

    StationEntry(const StationEntry&) = default;

    void add(const VarDesc& vd, const dballe::DatetimeRange& dtrange, size_t count);
    template<typename OStation>
    void add(const StationEntry<OStation>& entries);
    void add_filtered(const StationEntry& entries, const dballe::Query& query);

    void to_json(core::JSONWriter& writer) const;
    static StationEntry from_json(core::json::Stream& in);

    DBALLE_TEST_ONLY void dump(FILE* out) const;
};


template<typename Station>
inline const Station& station_entries_get_value(const StationEntry<Station>& item) { return item.station; }

/**
 * Index of all stations known to a summary
 *
 * It behaves similarly to a std::vector<StationEntry<Station>>
 */
template<typename Station>
struct StationEntries : protected core::SmallSet<StationEntry<Station>, Station, station_entries_get_value<Station>>
{
    typedef core::SmallSet<StationEntry<Station>, Station, station_entries_get_value<Station>> Parent;
    typedef typename Parent::iterator iterator;
    typedef typename Parent::const_iterator const_iterator;
    typedef typename Parent::reverse_iterator reverse_iterator;
    typedef typename Parent::const_reverse_iterator const_reverse_iterator;
    using Parent::begin;
    using Parent::end;
    using Parent::rbegin;
    using Parent::rend;
    using Parent::size;
    using Parent::empty;
    bool operator==(const StationEntries<Station>& o) const { return Parent::operator==(o); }
    bool operator!=(const StationEntries<Station>& o) const { return Parent::operator!=(o); }

    /// Merge the given entry
    void add(const Station& station, const VarDesc& vd, const dballe::DatetimeRange& dtrange, size_t count);

    /// Merge the given entries
    template<typename OStation>
    void add(const StationEntries<OStation>& entry);

    /// Merge the given entries
    void add(const StationEntries<Station>& entry);

    /// Merge the given entry
    void add(const StationEntry<Station>& entry);

    void add_filtered(const StationEntries& entry, const dballe::Query& query);

    bool has(const Station& station) const { return this->find(station) != this->end(); }

    const StationEntries& sorted() const { if (this->dirty) this->rearrange_dirty(); return *this; }
};


template<typename Station>
struct Cursor : public impl::CursorSummary
{
    struct Entry
    {
        const summary::StationEntry<Station>& station_entry;
        const summary::VarEntry& var_entry;
        Entry(const summary::StationEntry<Station>& station_entry, const summary::VarEntry& var_entry)
            : station_entry(station_entry), var_entry(var_entry) {}
    };
    std::vector<Entry> results;
    typename std::vector<Entry>::const_iterator cur;
    bool at_start = true;

    Cursor(const summary::StationEntries<Station>& entries, const Query& query);

    bool has_value() const { return !at_start && cur != results.end(); }

    int remaining() const override
    {
        if (at_start) return results.size();
        return results.end() - cur;
    }

    bool next() override
    {
        if (at_start)
        {
            cur = results.begin();
            at_start = false;
        }
        else if (cur != results.end())
            ++cur;
        return cur != results.end();
    }

    void discard() override
    {
        cur = results.end();
    }

    static DBStation _get_dbstation(const DBStation& s) { return s; }
    static DBStation _get_dbstation(const dballe::Station& station)
    {
        DBStation res;
        res.report = station.report;
        res.coords = station.coords;
        res.ident = station.ident;
        return res;
    }
    static int _get_station_id(const DBStation& s) { return s.id; }
    static int _get_station_id(const dballe::Station& s) { return MISSING_INT; }

    DBStation get_station() const override
    {
        return _get_dbstation(cur->station_entry.station);
    }

#if 0
    int get_station_id() const override
    {
        return _get_station_id(cur->station_entry.station);
    }

    Coords get_coords() const override { return cur->station_entry.station.coords; }
    Ident get_ident() const override { return cur->station_entry.station.ident; }
    std::string get_report() const override { return cur->station_entry.station.report; }

    unsigned test_iterate(FILE* dump=0) override
    {
        unsigned count;
        for (count = 0; next(); ++count)
            ;
#if 0
            if (dump)
                cur->dump(dump);
#endif
        return count;
    }
#endif

    Level get_level() const override { return cur->var_entry.var.level; }
    Trange get_trange() const override { return cur->var_entry.var.trange; }
    wreport::Varcode get_varcode() const override { return cur->var_entry.var.varcode; }
    DatetimeRange get_datetimerange() const override { return cur->var_entry.dtrange; }
    size_t get_count() const override { return cur->var_entry.count; }

    void enq(impl::Enq& enq) const;
};

extern template class StationEntry<dballe::Station>;
extern template class StationEntry<dballe::DBStation>;

extern template class StationEntries<dballe::Station>;
extern template void StationEntries<dballe::Station>::add(const StationEntries<dballe::DBStation>&);

extern template class StationEntries<dballe::DBStation>;
extern template void StationEntries<dballe::DBStation>::add(const StationEntries<dballe::Station>&);

extern template class Cursor<dballe::Station>;
extern template class Cursor<dballe::DBStation>;

}


/**
 * High level objects for working with DB-All.e DB summaries
 */
template<typename Station>
class BaseSummary
{
public:
    BaseSummary();
    BaseSummary(const BaseSummary&) = delete;
    BaseSummary(BaseSummary&&) = delete;
    BaseSummary& operator=(const BaseSummary&) = delete;
    BaseSummary& operator=(BaseSummary&&) = delete;
    virtual ~BaseSummary();

    // virtual bool operator==(const BaseSummary<Station>& o) const = 0;

    virtual const summary::StationEntries<Station>& stations() const = 0;
    virtual const core::SortedSmallUniqueValueSet<std::string>& reports() const = 0;
    virtual const core::SortedSmallUniqueValueSet<dballe::Level>& levels() const = 0;
    virtual const core::SortedSmallUniqueValueSet<dballe::Trange>& tranges() const = 0;
    virtual const core::SortedSmallUniqueValueSet<wreport::Varcode>& varcodes() const = 0;

    /**
     * Recompute reports, levels, tranges, and varcodes.
     *
     * Call this after performing changes to the summary, to make those sets
     * valid before reading them.
     */
    virtual const Datetime& datetime_min() const = 0;
    virtual const Datetime& datetime_max() const = 0;
    virtual unsigned data_count() const = 0;

    /**
     * Query the contents of the summary
     *
     * @param query
     *   The record with the query data (see technical specifications, par. 1.6.4
     *   "parameter output/input")
     * @return
     *   The cursor to use to iterate over the results. The results are the
     *   same as DB::query_summary.
     */
    virtual std::unique_ptr<dballe::CursorSummary> query_summary(const Query& query) const = 0;

    /// Add an entry to the summary
    virtual void add(const Station& station, const summary::VarDesc& vd, const dballe::DatetimeRange& dtrange, size_t count) = 0;

    /// Add an entry to the summary taken from the current status of \a cur
    virtual void add_cursor(const dballe::CursorSummary& cur) = 0;

    /// Add the contents of a Message
    virtual void add_message(const dballe::Message& message) = 0;

    /// Add the contents of a Messages
    virtual void add_messages(const std::vector<std::shared_ptr<dballe::Message>>& messages) = 0;

    /// Merge the copy of another summary into this one
    virtual void add_summary(const BaseSummary<dballe::Station>& summary) = 0;

    /// Merge the copy of another summary into this one
    virtual void add_summary(const BaseSummary<dballe::DBStation>& summary) = 0;

    /// Merge the copy of another summary into this one
    virtual void add_filtered(const BaseSummary<Station>& summary, const dballe::Query& query) = 0;

    /// Serialize to JSON
    virtual void to_json(core::JSONWriter& writer) const = 0;

    /// Load contents from JSON, merging with the current contents
    virtual void load_json(core::json::Stream& in) = 0;

    DBALLE_TEST_ONLY virtual void dump(FILE* out) const = 0;
};


/**
 * Summary without database station IDs
 */
typedef BaseSummary<dballe::Station> Summary;

/**
 * Summary with database station IDs
 */
typedef BaseSummary<dballe::DBStation> DBSummary;

extern template class BaseSummary<dballe::Station>;
extern template class BaseSummary<dballe::DBStation>;

}
}

#endif
