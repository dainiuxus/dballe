#include "levtr.h"
#include "dballe/core/defs.h"
#include "dballe/msg/msg.h"
#include "dballe/sql/sqlite.h"
#include <map>
#include <sstream>
#include <cstring>
#include <sqltypes.h>
#include <sql.h>

using namespace wreport;
using namespace std;
using dballe::sql::SQLiteConnection;
using dballe::sql::SQLiteStatement;

namespace dballe {
namespace db {
namespace v6 {
namespace sqlite {

namespace {

Level to_level(SQLiteStatement& stm, int first_id=0)
{
    return Level(
            stm.column_int(first_id),
            stm.column_int(first_id + 1),
            stm.column_int(first_id + 2),
            stm.column_int(first_id + 3));
}

Trange to_trange(SQLiteStatement& stm, int first_id=0)
{
    return Trange(
            stm.column_int(first_id),
            stm.column_int(first_id + 1),
            stm.column_int(first_id + 2));
}

}

SQLiteLevTrV6::SQLiteLevTrV6(SQLiteConnection& conn)
    : conn(conn)
{
    const char* select_query =
        "SELECT id FROM lev_tr WHERE"
        "     ltype1=? AND l1=? AND ltype2=? AND l2=?"
        " AND ptype=? AND p1=? AND p2=?";
    const char* select_data_query =
        "SELECT ltype1, l1, ltype2, l2, ptype, p1, p2 FROM lev_tr WHERE id=?";
    const char* insert_query =
        "INSERT INTO lev_tr (ltype1, l1, ltype2, l2, ptype, p1, p2) VALUES (?, ?, ?, ?, ?, ?, ?)";

    // Create the statement for select fixed
    sstm = conn.sqlitestatement(select_query).release();

    // Create the statement for select data
    sdstm = conn.sqlitestatement(select_data_query).release();

    // Create the statement for insert
    istm = conn.sqlitestatement(insert_query).release();
}

SQLiteLevTrV6::~SQLiteLevTrV6()
{
    delete sstm;
    delete sdstm;
    delete istm;
}

int SQLiteLevTrV6::obtain_id(const Level& lev, const Trange& tr)
{
    sstm->bind(lev.ltype1, lev.l1, lev.ltype2, lev.l2, tr.pind, tr.p1, tr.p2);

    int id = -1;
    sstm->execute_one([&]() {
        id = sstm->column_int(0);
    });

    // If there is an existing record, use its ID and don't do an INSERT
    if (id != -1) return id;

    istm->bind(lev.ltype1, lev.l1, lev.ltype2, lev.l2, tr.pind, tr.p1, tr.p2);
    istm->execute();

    return conn.get_last_insert_id();
}

const v6::LevTr::DBRow* SQLiteLevTrV6::read(int id)
{
    sdstm->bind(id);
    bool found = false;
    sdstm->execute([&]() {
        working_row.id = id;
        working_row.ltype1 = sdstm->column_int(0);
        working_row.l1 = sdstm->column_int(1);
        working_row.ltype2 = sdstm->column_int(2);
        working_row.l2 = sdstm->column_int(3);
        working_row.pind = sdstm->column_int(4);
        working_row.p1 = sdstm->column_int(5);
        working_row.p2 = sdstm->column_int(6);
        found = true;
    });

    if (!found) return nullptr;
    return &working_row;
}

void SQLiteLevTrV6::read_all(std::function<void(const LevTr::DBRow&)> dest)
{
    auto stm = conn.sqlitestatement("SELECT id, ltype1, l1, ltype2, l2, ptype, p1, p2 FROM lev_tr");
    stm->execute([&]() {
        working_row.id = stm->column_int(0);
        working_row.ltype1 = stm->column_int(1);
        working_row.l1 = stm->column_int(2);
        working_row.ltype2 = stm->column_int(3);
        working_row.l2 = stm->column_int(4);
        working_row.pind = stm->column_int(5);
        working_row.p1 = stm->column_int(6);
        working_row.p2 = stm->column_int(7);
        dest(working_row);
    });
}

void SQLiteLevTrV6::dump(FILE* out)
{
    int count = 0;
    fprintf(out, "dump of table lev_tr:\n");
    fprintf(out, "   id   lev              tr\n");
    auto stm = conn.sqlitestatement("SELECT id, ltype1, l1, ltype2, l2, ptype, p1, p2 FROM lev_tr ORDER BY ID");
    stm->execute([&]() {
        fprintf(out, " %4d ", stm->column_int(0));
        int written = to_level(*stm, 1).print(out);
        while (written++ < 21) putc(' ', out);
        written = to_trange(*stm, 5).print(out);
        while (written++ < 11) putc(' ', out);
        ++count;
    });
    fprintf(out, "%d element%s in table lev_tr\n", count, count != 1 ? "s" : "");
}

}
}
}
}