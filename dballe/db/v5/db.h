/*
 * dballe/v5/db - Archive for point-based meteorological data, db layout version 5
 *
 * Copyright (C) 2005--2015  ARPA-SIM <urpsim@smr.arpa.emr.it>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 *
 * Author: Enrico Zini <enrico@enricozini.com>
 */

#ifndef DBA_DB_V5_H
#define DBA_DB_V5_H

#include <dballe/db/db.h>
#include <wreport/varinfo.h>
#include <string>
#include <vector>
#include <memory>

/** @file
 * @ingroup db
 *
 * Functions used to connect to DB-All.e and insert, query and delete data.
 */

/**
 * Constants used to define what values we should retrieve from a query
 */
/** Retrieve latitude and longitude */
#define DBA_DB_WANT_COORDS		(1 << 0)
/** Retrieve the mobile station identifier */
#define DBA_DB_WANT_IDENT		(1 << 1)
/** Retrieve the level information */
#define DBA_DB_WANT_LEVEL		(1 << 2)
/** Retrieve the time range information */
#define DBA_DB_WANT_TIMERANGE	(1 << 3)
/** Retrieve the date and time information */
#define DBA_DB_WANT_DATETIME	(1 << 4)
/** Retrieve the variable name */
#define DBA_DB_WANT_VAR_NAME	(1 << 5)
/** Retrieve the variable value */
#define DBA_DB_WANT_VAR_VALUE	(1 << 6)
/** Retrieve the report code */
#define DBA_DB_WANT_REPCOD		(1 << 7)
/** Retrieve the station ID */
#define DBA_DB_WANT_ANA_ID		(1 << 8)
/** Retrieve the context ID */
#define DBA_DB_WANT_CONTEXT_ID	(1 << 9)

namespace dballe {
struct Msg;
struct Msgs;
struct MsgConsumer;

namespace db {
struct Connection;
struct Sequence;

namespace sql {
struct Driver;
struct Repinfo;
struct Station;
struct DataV5;
struct AttrV5;
}

namespace v5 {
struct Context;

/**
 * DB-ALLe database connection
 */
class DB : public dballe::DB
{
public:
    /// Database connection
    db::Connection* conn;

protected:
    int last_context_id;

    sql::Driver* m_driver = nullptr;

	/**
	 * Accessors for the various parts of the database.
	 *
	 * @warning Before using these 5 pointers, ensure they are initialised
	 * using one of the dba_db_need_* functions
	 * @{
	 */
    /** Report information */
    struct sql::Repinfo* m_repinfo = nullptr;
    /** Station information */
    struct sql::Station* m_station = nullptr;
	/** Variable context */
	struct Context* m_context = nullptr;
    /** Variable data */
    struct sql::DataV5* m_data = nullptr;
    /** Variable attributes */
    struct sql::AttrV5* m_attr = nullptr;
    /** @} */

    int _last_station_id = 0;

	void init_after_connect();

	/**
	 * Fill a message station info layer with information from the given
	 * station and report IDs
	 */
	void fill_ana_layer(Msg& msg, int id_station, int id_report);

    DB(std::unique_ptr<ODBCConnection> conn);

public:
    ~DB();

    db::Format format() const { return V5; }

    /// Access the repinfo table
    sql::Repinfo& repinfo();

    /// Access the station table
    sql::Station& station();

	/// Access the context table
	Context& context();

    /// Access the data table
    sql::DataV5& data();

    /// Access the data table
    sql::AttrV5& attr();

    void disappear();

	/**
	 * Reset the database, removing all existing DBALLE tables and re-creating them
	 * empty.
	 *
	 * @param repinfo_file
	 *   The name of the CSV file with the report type information data to load.
	 *   The file is in CSV format with 6 columns: report code, mnemonic id,
	 *   description, priority, descriptor, table A category.
	 *   If repinfo_file is NULL, then the default of /etc/dballe/repinfo.csv is
	 *   used.
	 */
	void reset(const char* repinfo_file = 0);

	/**
	 * Delete all the DB-ALLe tables from the database.
	 */
	void delete_tables();

	/**
	 * Update the repinfo table in the database, with the data found in the given
	 * file.
	 *
	 * @param repinfo_file
	 *   The name of the CSV file with the report type information data to load.
	 *   The file is in CSV format with 6 columns: report code, mnemonic id,
	 *   description, priority, descriptor, table A category.
	 *   If repinfo_file is NULL, then the default of /etc/dballe/repinfo.csv is
	 *   used.
	 * @retval added
	 *   The number of repinfo entryes that have been added
	 * @retval deleted
	 *   The number of repinfo entryes that have been deleted
	 * @retval updated
	 *   The number of repinfo entryes that have been updated
	 */
	void update_repinfo(const char* repinfo_file, int* added, int* deleted, int* updated);

    virtual std::map<std::string, int> get_repinfo_priorities();

	/**
	 * Get the report id from this record.
	 *
	 * If rep_memo is specified instead, the corresponding report id is queried in
	 * the database and set as "rep_cod" in the record.
	 */
	int get_rep_cod(const Record& rec);

	/*
	 * Lookup, insert or replace data in station taking the values from
	 * rec.
	 *
	 * If rec did not contain ana_id, it will be set by this function.
	 *
	 * @param rec
	 *   The record with the station information
	 * @param can_add
	 *   If true we can insert new stations in the database, if false we
	 *   only look up existing records and raise an exception if missing
	 * @returns
	 *   The station ID
	 */
	int obtain_station(const Record& rec, bool can_add=true);

	/*
	 * Lookup, insert or replace data in station taking the values from
	 * rec.
	 *
	 * If rec did not contain context_id, it will be set by this function.
	 *
	 * @param rec
	 *   The record with the context information
	 * @returns
	 *   The context ID
	 */
	int obtain_context(const Record& rec);

    void insert(const Record& rec, bool can_replace, bool station_can_add);

    int last_station_id() const;

	/**
	 * Remove data from the database
	 *
	 * @param rec
	 *   The record with the query data (see technical specifications, par. 1.6.4
	 *   "parameter output/input") to select the items to be deleted
	 */
	void remove(const Query& rec);

    void remove_all();

	/**
	 * Remove orphan values from the database.
	 *
	 * Orphan values are currently:
	 * \li context values for which no data exists
	 * \li station values for which no context exists
	 *
	 * Depending on database size, this routine can take a few minutes to execute.
	 */
	void vacuum();

	/**
	 * Create and execute a database query.
	 *
	 * The results are retrieved by iterating the cursor.
	 *
	 * @param query
	 *   The record with the query data (see technical specifications, par. 1.6.4
	 *   "parameter output/input"
	 * @param wanted
	 *   The values wanted in output
	 * @param modifiers
	 *   Optional modifiers to ask for special query behaviours
	 * @return
	 *   The cursor to use to iterate over the results
	 */
	std::unique_ptr<db::Cursor> query(const Query& query, unsigned int wanted, unsigned int modifiers);

	/**
	 * Start a query on the station variables archive
	 *
	 * @param query
	 *   The record with the query data (see @ref dba_record_keywords)
	 * @return
	 *   The cursor to use to iterate over the results
	 */
	std::unique_ptr<db::Cursor> query_stations(const Query& query);

	/**
	 * Query the database.
	 *
	 * When multiple values per variable are present, the results will be presented
	 * in increasing order of priority.
	 *
	 * @param query
	 *   The record with the query data (see technical specifications, par. 1.6.4
	 *   "parameter output/input")
	 * @return
	 *   The cursor to use to iterate over the results
	 */
	std::unique_ptr<db::Cursor> query_data(const Query& rec);

    virtual std::unique_ptr<db::Cursor> query_summary(const Query& rec);

    void query_datetime_extremes(const Query& query, Record& result);

    /**
     * Query attributes
     *
     * @param reference_id
     *   The database id of the context related to the attributes to retrieve
     * @param id_var
     *   The varcode of the variable related to the attributes to retrieve.  See @ref vartable.h
     * @param qcs
     *   The WMO codes of the QC values requested.  If it is empty, then all values
     *   are returned.
     * @param dest
     *   The function that will be called on each attribute retrieved
     * @return
     *   Number of attributes returned in attrs
     */
    void query_attrs(int reference_id, wreport::Varcode id_var,
            std::function<void(std::unique_ptr<wreport::Var>)> dest) override;

    void attr_insert(wreport::Varcode id_var, const Record& attrs);
    void attr_insert(int reference_id, wreport::Varcode id_var, const Record& attrs);

	/**
	 * Delete QC data for the variable `var' in record `rec' (coming from a previous
	 * dba_query)
	 *
	 * @param reference_id
	 *   The database id of the context related to the attributes to remove
	 * @param id_var
	 *   The varcode of the variable related to the attributes to remove.  See @ref vartable.h
	 * @param qcs
	 *   Array of WMO codes of the attributes to delete.  If empty, all attributes
	 *   associated to id_data will be deleted.
	 */
	void attr_remove(int reference_id, wreport::Varcode id_var, const db::AttrList& qcs);

	/**
	 * Import a Msg message into the DB-All.e database
	 *
	 * @param db
	 *   The DB-All.e database to write the data into
	 * @param msg
	 *   The Msg containing the data to import
	 * @param repmemo
	 *   Report mnemonic to which imported data belong.  If NULL is passed, then it
	 *   will be chosen automatically based on the message type.
	 * @param flags
	 *   Customise different aspects of the import process.  It is a bitmask of the
	 *   various DBA_IMPORT_* macros.
	 */
	void import_msg(const Msg& msg, const char* repmemo, int flags);

	/**
	 * Perform the query in `query', and return the results as a NULL-terminated
	 * array of dba_msg.
	 *
	 * @param query
	 *   The query to perform
	 * @param cons
	 *   The MsgsConsumer that will handle the resulting messages
	 */
	void export_msgs(const Query& query, MsgConsumer& cons);

        /**
         * Dump the entire contents of the database to an output stream
         */
	void dump(FILE* out);

    friend class dballe::DB;
};

} // namespace v5
} // namespace db
} // namespace dballe

/* vim:set ts=4 sw=4: */
#endif
