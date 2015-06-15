#include "config.h"
#include "db.h"
#include "sql.h"
#include "v6/db.h"
#include "mem/db.h"
#include "sqlite/internals.h"
#ifdef HAVE_ODBC
#include "odbc/internals.h"
#endif
#include "dballe/msg/msgs.h"
#include "dballe/core/record.h"
#include "dballe/core/values.h"
#include <wreport/error.h>
#include <cstring>
#include <cstdlib>

using namespace dballe::db;
using namespace std;
using namespace wreport;

namespace dballe {
namespace db {

#ifdef HAVE_ODBC
static Format default_format = V6;
#else
static Format default_format = MEM;
#endif

Cursor::~Cursor()
{
}

unsigned Cursor::test_iterate(FILE* dump)
{
    unsigned count;
    for (count = 0; next(); ++count)
        ;
    return count;
}

}

DB::~DB()
{
}

void DB::import_msgs(const Msgs& msgs, const char* repmemo, int flags)
{
    for (Msgs::const_iterator i = msgs.begin(); i != msgs.end(); ++i)
        import_msg(**i, repmemo, flags);
}

Format DB::get_default_format() { return default_format; }
void DB::set_default_format(Format format) { default_format = format; }

bool DB::is_url(const char* str)
{
    if (strncmp(str, "mem:", 4) == 0) return true;
    if (strncmp(str, "sqlite:", 7) == 0) return true;
    if (strncmp(str, "postgresql:", 11) == 0) return true;
    if (strncmp(str, "mysql:", 6) == 0) return true;
    if (strncmp(str, "odbc://", 7) == 0) return true;
    if (strncmp(str, "test:", 5) == 0) return true;
    return false;
}

unique_ptr<DB> DB::create(unique_ptr<Connection> conn)
{
    // Autodetect format
    Format format = default_format;

    bool found = true;

    // Try with reading it from the settings table
    string version = conn->get_setting("version");
    if (version == "V5")
        format = V5;
    else if (version == "V6")
        format = V6;
    else if (version == "")
        found = false;// Some other key exists, but the version has not been set
    else
        error_consistency::throwf("unsupported database version: '%s'", version.c_str());

    // If it failed, try looking at the existing table structure
    if (!found)
    {
        if (conn->has_table("lev_tr"))
            format = V6;
        else if (conn->has_table("context"))
            format = V5;
        else
            format = default_format;
    }

    switch (format)
    {
        case V5: throw error_unimplemented("V5 format is not supported anymore by this version of DB-All.e");
        case V6: return unique_ptr<DB>(new v6::DB(unique_ptr<Connection>(conn.release())));
        default: error_consistency::throwf("requested unknown format %d", (int)format);
    }
}

unique_ptr<DB> DB::connect(const char* dsn, const char* user, const char* password)
{
#ifdef HAVE_ODBC
    unique_ptr<ODBCConnection> conn(new ODBCConnection);
    conn->connect(dsn, user, password);
    return create(move(conn));
#else
    throw error_unimplemented("ODBC support is not available");
#endif
}

unique_ptr<DB> DB::connect_from_file(const char* pathname)
{
    unique_ptr<SQLiteConnection> conn(new SQLiteConnection);
    conn->open_file(pathname);
    return create(unique_ptr<Connection>(conn.release()));
}

unique_ptr<DB> DB::connect_from_url(const char* url)
{
    if (strncmp(url, "mem:", 4) == 0)
    {
        return connect_memory(url + 4);
    } else {
        unique_ptr<Connection> conn(Connection::create_from_url(url));
        return create(move(conn));
    }
}

unique_ptr<DB> DB::connect_memory(const std::string& arg)
{
    if (arg.empty())
        return unique_ptr<DB>(new mem::DB());
    else
        return unique_ptr<DB>(new mem::DB(arg));
}

unique_ptr<DB> DB::connect_test()
{
    if (default_format == MEM)
        return connect_memory();

    const char* envurl = getenv("DBA_DB");
    if (envurl != NULL)
        return connect_from_url(envurl);
    else
        return connect_from_file("test.sqlite");
}

const char* DB::default_repinfo_file()
{
    const char* repinfo_file = getenv("DBA_REPINFO");
    if (repinfo_file == 0 || repinfo_file[0] == 0)
        repinfo_file = TABLE_DIR "/repinfo.csv";
    return repinfo_file;
}

void DB::insert(const Record& rec, bool can_replace, bool station_can_add)
{
    // Obtain values
    const auto& r = core::Record::downcast(rec);
    Datetime datetime = r.get_datetime();
    if (datetime.is_missing())
    {
        StationValues sv;
        sv.from_record(rec);
        insert_station_data(sv, can_replace, station_can_add);
    } else {
        DataValues dv;
        dv.from_record(rec);
        insert_data(dv, can_replace, station_can_add);
    }
}

}
