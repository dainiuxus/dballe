/*
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

#include "db/test-utils-db.h"
#include "db/v5/db.h"
#include "db/v6/db.h"
#include "db/sql.h"
#include "db/sql/station.h"
#include "config.h"

using namespace dballe;
using namespace dballe::tests;
using namespace wreport;
using namespace wibble::tests;
using namespace std;

namespace {

struct Fixture : dballe::tests::DriverFixture
{
    unique_ptr<db::sql::Station> station;

    Fixture()
    {
        reset_station();
    }

    void reset_station()
    {
        if (conn->has_table("station"))
            driver->exec_no_data("DELETE FROM station");

        switch (format)
        {
            case db::V5:
                station = driver->create_stationv5();
                break;
            case db::V6:
                station = driver->create_stationv6();
                break;
            default:
                throw error_consistency("cannot test station on the current DB format");
        }
    }

    void reset()
    {
        dballe::tests::DriverFixture::reset();
        reset_station();
    }
};

typedef dballe::tests::driver_test_group<Fixture> test_group;
typedef test_group::Test Test;

std::vector<Test> tests {
    Test("insert", [](Fixture& f) {
        // Insert some values and try to read them again
        auto& st = *f.station;
        bool inserted;

        // Insert a mobile station
        wassert(actual(st.obtain_id(4500000, 1100000, "ciao", &inserted)) == 1);
        wassert(actual(inserted).istrue());
        wassert(actual(st.obtain_id(4500000, 1100000, "ciao", &inserted)) == 1);
        wassert(actual(inserted).isfalse());

        // Insert a fixed station
        wassert(actual(st.obtain_id(4600000, 1200000, NULL, &inserted)) == 2);
        wassert(actual(inserted).istrue());
        wassert(actual(st.obtain_id(4600000, 1200000, NULL, &inserted)) == 2);
        wassert(actual(inserted).isfalse());

        // Get the ID of the first station
        wassert(actual(st.get_id(4500000, 1100000, "ciao")) == 1);

        // Get the ID of the second station
        wassert(actual(st.get_id(4600000, 1200000)) == 2);
    }),
};

test_group tg1("db_sql_station_v5_sqlite", "SQLITE", db::V5, tests);
test_group tg2("db_sql_station_v6_sqlite", "SQLITE", db::V6, tests);
#ifdef HAVE_ODBC
test_group tg3("db_sql_station_v5_odbc", "ODBC", db::V5, tests);
test_group tg4("db_sql_station_v6_odbc", "ODBC", db::V6, tests);
#endif
#ifdef HAVE_LIBPQ
test_group tg5("db_sql_station_v5_postgresql", "POSTGRESQL", db::V5, tests);
test_group tg6("db_sql_station_v6_postgresql", "POSTGRESQL", db::V6, tests);
#endif
#ifdef HAVE_MYSQL
test_group tg7("db_sql_station_v5_mysql", "MYSQL", db::V5, tests);
test_group tg8("db_sql_station_v6_mysql", "MYSQL", db::V6, tests);
#endif

}