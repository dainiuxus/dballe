/*
 * db/station - station table management
 *
 * Copyright (C) 2005--2014  ARPA-SIM <urpsim@smr.arpa.emr.it>
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

#include "station.h"
#include "dballe/db/postgresql/internals.h"
#include "dballe/core/var.h"
#include "dballe/core/record.h"
#include <wreport/var.h>

using namespace wreport;
using namespace dballe::db;
using namespace std;

namespace dballe {
namespace db {

namespace postgresql {
StationBase::StationBase(PostgreSQLConnection& conn)
    : conn(conn)
{
    // Precompile our statements
    conn.prepare("v5_station_select_fixed", "SELECT id FROM station WHERE lat=$1::int4 AND lon=$2::int4 AND ident IS NULL");
    conn.prepare("v5_station_select_mobile", "SELECT id FROM station WHERE lat=$1::int4 AND lon=$2::int4 AND ident=$3::text");
    conn.prepare("v5_station_insert", "INSERT INTO station (id, lat, lon, ident) VALUES (DEFAULT, $1::int4, $2::int4, $3::text);");
}

StationBase::~StationBase()
{
}

bool StationBase::maybe_get_id(int lat, int lon, const char* ident, int* id)
{
    using namespace postgresql;

    Result res;
    if (ident)
        res = move(conn.exec_prepared("v5_station_select_mobile", lat, lon, ident));
    else
        res = move(conn.exec_prepared("v5_station_select_fixed", lat, lon));

    unsigned rows = res.rowcount();
    switch (rows)
    {
        case 0: return false;
        case 1: return res.get_int4(0, 0);
        default: error_consistency::throwf("select station ID query returned %u results", rows);
    }
}

int StationBase::get_id(int lat, int lon, const char* ident)
{
    int id;
    if (maybe_get_id(lat, lon, ident, &id))
        return id;
    throw error_notfound("station not found in the database");
}

int StationBase::obtain_id(int lat, int lon, const char* ident, bool* inserted)
{
    using namespace postgresql;
    int id;
    if (maybe_get_id(lat, lon, ident, &id))
    {
        if (inserted) *inserted = false;
        return id;
    }

    // If no station was found, insert a new one
    Result res(conn.exec_prepared_one_row("v5_station_insert", lat, lon, ident));
    if (inserted) *inserted = true;
    return res.get_int4(0, 0);
}

void StationBase::get_station_vars(int id_station, int id_report, std::function<void(std::unique_ptr<wreport::Var>)> dest)
{
    // Perform the query
    using namespace postgresql;
    TRACE("fill_ana_layer Performing query: %s with idst %d idrep %d\n", query, id_station, id_report);
    Result res(conn.exec_prepared("v5_station_get_station_vars", id_station, id_report));

    // Retrieve results
    Varcode last_varcode = 0;
    unique_ptr<Var> var;

    for (unsigned row = 0; row < res.rowcount(); ++row)
    {
        Varcode code = res.get_int4(row, 0);
        TRACE("fill_ana_layer Got B%02ld%03ld %s\n", WR_VAR_X(code), WR_VAR_Y(code), out_value);

        // First process the variable, possibly inserting the old one in the message
        if (last_varcode != code)
        {
            TRACE("fill_ana_layer new var\n");
            if (var.get())
            {
                TRACE("fill_ana_layer inserting old var B%02d%03d\n", WR_VAR_X(var->code()), WR_VAR_Y(var->code()));
                dest(move(var));
            }
            var = newvar(code, res.get_string(row, 1));
            last_varcode = code;
        }

        if (!res.is_null(row, 2))
        {
            TRACE("fill_ana_layer new attribute\n");
            var->seta(ap_newvar(res.get_int4(row, 2), res.get_string(row, 3)));
        }
    };

    if (var.get())
    {
        TRACE("fill_ana_layer inserting leftover old var B%02d%03d\n", WR_VAR_X(var->code()), WR_VAR_Y(var->code()));
        dest(move(var));
    }
}

void StationBase::add_station_vars(int id_station, Record& rec)
{
    using namespace postgresql;
    Result res(conn.exec_prepared("v5_station_add_station_vars", id_station));
    for (unsigned row = 0; row < res.rowcount(); ++row)
        rec.var(res.get_int4(row, 0)).setc(res.get_string(row, 1));
}

void StationBase::dump(FILE* out)
{
    int count = 0;
    fprintf(out, "dump of table station:\n");

    auto res = conn.exec("SELECT id, lat, lon, ident FROM station");
    for (unsigned row = 0; row < res.rowcount(); ++row)
    {
        fprintf(out, " %d, %.5f, %.5f",
                res.get_int4(row, 0),
                res.get_int4(row, 1) / 100000.0,
                res.get_int4(row, 2) / 100000.0);
        if (res.is_null(row, 3))
            putc('\n', out);
        else
        {
            string ident = res.get_string(row, 3);
            fprintf(out, ", %.*s\n", ident.size(), ident.data());
        }
        ++count;
    }
    fprintf(out, "%d element%s in table station\n", count, count != 1 ? "s" : "");
}

}


namespace v5 {

PostgreSQLStation::PostgreSQLStation(PostgreSQLConnection& conn)
    : StationBase(conn)
{
    conn.prepare("v5_station_get_station_vars", R"(
        SELECT d.id_var, d.value, a.type, a.value
         FROM context c, data d
         LEFT JOIN attr a ON a.id_context = d.id_context AND a.id_var = d.id_var
        WHERE d.id_context = c.id AND c.id_ana = $1::int4 AND c.id_report = $2::int4
          AND c.datetime = TIMESTAMP '1000-01-01 00:00:00+00'
        ORDER BY d.id_var, a.type
    )");
    conn.prepare("v5_station_add_station_vars", R"(
        SELECT d.id_var, d.value
          FROM context c, data d, repinfo ri
         WHERE c.id = d.id_context AND ri.id = c.id_report AND c.id_ana=$1::int4
           AND c.datetime=TIMESTAMP '1000-01-01 00:00:00+00'
         AND ri.prio=(
          SELECT MAX(sri.prio) FROM repinfo sri
            JOIN context sc ON sri.id=sc.id_report
            JOIN data sd ON sc.id=sd.id_context
          WHERE sc.id_ana=c.id_ana
            AND sc.ltype1=c.ltype1 AND sc.l1=c.l1 AND sc.ltype2=c.ltype2 AND sc.l2=c.l2
            AND sc.ptype=c.ptype AND sc.p1=c.p1 AND sc.p2=c.p2
            AND sc.datetime=c.datetime AND sd.id_var=d.id_var)
    )");
}
PostgreSQLStation::~PostgreSQLStation()
{
}

}


namespace v6 {

PostgreSQLStation::PostgreSQLStation(PostgreSQLConnection& conn)
    : StationBase(conn)
{
    conn.prepare("v5_station_get_station_vars", R"(
        SELECT d.id_var, d.value, a.type, a.value
          FROM data d
          LEFT JOIN attr a ON a.id_data = d.id
         WHERE d.id_station=? AND d.id_report=?
           AND d.id_lev_tr = -1
         ORDER BY d.id_var, a.type
    )");
    conn.prepare("v5_station_add_station_vars", R"(
        SELECT d.id_var, d.value
          FROM data d, repinfo ri
         WHERE d.id_lev_tr = -1 AND ri.id = d.id_report AND d.id_station = ?
         AND ri.prio=(
          SELECT MAX(sri.prio) FROM repinfo sri
            JOIN data sd ON sri.id=sd.id_report
          WHERE sd.id_station=d.id_station AND sd.id_lev_tr = -1
            AND sd.id_var=d.id_var)
    )");
}
PostgreSQLStation::~PostgreSQLStation()
{
}

}

}
}
