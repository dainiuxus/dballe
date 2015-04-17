/*
 * db/postgresql/attrv6 - attribute table management
 *
 * Copyright (C) 2015  ARPA-SIM <urpsim@smr.arpa.emr.it>
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
#include "attrv6.h"
#include "dballe/db/sql/internals.h"
#include "dballe/db/querybuf.h"
#include "dballe/core/var.h"
#include <cstring>

using namespace std;
using namespace wreport;

namespace dballe {
namespace db {
namespace postgresql {

PostgreSQLAttrV6::PostgreSQLAttrV6(PostgreSQLConnection& conn)
    : conn(conn)
{
    conn.prepare("attrv6_select", R"(
        SELECT type, value FROM attr WHERE id_data=$1::int4
    )");
    conn.prepare("attrv6_select_existing", R"(
        SELECT type, value FROM attr WHERE id_data=$1::int4
    )");
    conn.prepare("attrv6_insert", R"(
        INSERT INTO attr (id_data, type, value) VALUES ($1::int4, $2::int4, $3::text)
    )");
    conn.prepare("attrv6_update", R"(
        UPDATE attr SET value=$3::text WHERE id_data=$1::int4 AND type=$2::int4
    )");
}

PostgreSQLAttrV6::~PostgreSQLAttrV6()
{
}

void PostgreSQLAttrV6::insert(Transaction& t, sql::bulk::InsertAttrsV6& attrs, UpdateMode update_mode)
{
    Querybuf select_query;
    select_query.append("SELECT id_data, type, value FROM attr WHERE id_data IN (");
    select_query.start_list(",");
    int last_data_id = -1;
    for (const auto& a: attrs)
    {
        if (a.id_data == last_data_id) continue;
        select_query.append_listf("%d", a.id_data);
        last_data_id = a.id_data;
    }
    select_query.append(") ORDER BY id_data, type");

    t.lock_table("attr");

    // Get the current status of attributes for these variables
    Result res_current(conn.exec(select_query));
    sql::bulk::AnnotateAttrsV6 todo(attrs);
    for (unsigned row = 0; row < res_current.rowcount(); ++row)
    {
        todo.annotate(
                res_current.get_int4(row, 0),
                res_current.get_int4(row, 1),
                res_current.get_string(row, 2));
    }
    todo.annotate_end();

    // We now have a todo-list

    switch (update_mode)
    {
        case UPDATE:
            if (todo.do_update)
            {
                Querybuf dq(512);
                dq.append("UPDATE attr as a SET value=i.value FROM (values ");
                dq.start_list(",");
                for (auto& a: attrs)
                {
                    if (!a.needs_update()) continue;
                    char* escaped_val = PQescapeLiteral(conn, a.attr->value(), strlen(a.attr->value()));
                    if (!escaped_val)
                        throw error_postgresql(conn, string("escaping string '") + a.attr->value() + "'");
                    dq.append_listf("(%d, %d, %s)", a.id_data, (int)a.attr->code(), escaped_val);
                    PQfreemem(escaped_val);
                    a.set_updated();
                }
                dq.append(") AS i(id_data, type, value) WHERE a.id_data = i.id_data and a.type = i.type");
                //fprintf(stderr, "Update query: %s\n", dq.c_str());
                conn.exec_no_data(dq);
            }
            break;
        case IGNORE:
            break;
        case ERROR:
            if (todo.do_update)
                throw error_consistency("refusing to overwrite existing data");
    }

    if (todo.do_insert)
    {
        Querybuf dq(512);
        dq.append("INSERT INTO attr (id_data, type, value) VALUES ");
        dq.start_list(",");
        for (auto& a: attrs)
        {
            if (!a.needs_insert()) continue;
            char* escaped_val = PQescapeLiteral(conn, a.attr->value(), strlen(a.attr->value()));
            if (!escaped_val)
                throw error_postgresql(conn, string("escaping string '") + a.attr->value() + "'");
            dq.append_listf("(%d, %d, %s)", a.id_data, (int)a.attr->code(), escaped_val);
            PQfreemem(escaped_val);
            a.set_inserted();
        }
        //fprintf(stderr, "Insert query: %s\n", dq.c_str());

        // Run the insert query and read back the new IDs
        conn.exec_no_data(dq);
    }
}

void PostgreSQLAttrV6::read(int id_data, function<void(unique_ptr<Var>)> dest)
{
    using namespace postgresql;
    Result res = conn.exec_prepared("attrv6_select", id_data);
    for (unsigned row = 0; row < res.rowcount(); ++row)
    {
        if (res.is_null(row, 1))
            dest(newvar(res.get_int4(row, 0)));
        else
            dest(newvar(res.get_int4(row, 0), res.get_string(row, 1)));
    }
}

void PostgreSQLAttrV6::dump(FILE* out)
{
    int count = 0;
    fprintf(out, "dump of table attr:\n");
    auto res = conn.exec("SELECT id_data, type, value FROM attr");
    for (unsigned row = 0; row < res.rowcount(); ++row)
    {
        Varcode type = res.get_int4(row, 1);
        const char* val = res.get_string(row, 2);
        fprintf(out, " %4d, %01d%02d%03d %s",
                res.get_int4(row, 0),
                WR_VAR_F(type), WR_VAR_X(type), WR_VAR_Y(type),
                val);
        ++count;
    }
    fprintf(out, "%d element%s in table attr\n", count, count != 1 ? "s" : "");
}

}
}
}
