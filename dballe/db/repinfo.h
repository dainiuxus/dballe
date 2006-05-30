#ifndef DBALLE_DB_REPINFO_H
#define DBALLE_DB_REPINFO_H

#ifdef  __cplusplus
extern "C" {
#endif

/** @file
 * @ingroup db
 *
 * Repinfo table management used by the db module, but not
 * exported as official API.
 */

#include <dballe/db/internals.h>

struct _dba_db;

struct _dba_db_repinfo_cache {
	int id;

	char* memo;
	char* desc;
	int	  prio;
	char* descriptor;
	int   tablea;

	char* new_memo;
	char* new_desc;
	int	  new_prio;
	char* new_descriptor;
	int	  new_tablea;
};
typedef struct _dba_db_repinfo_cache* dba_db_repinfo_cache;

struct _dba_db_repinfo_memoidx {
	char memo[30];
	int id;
};
typedef struct _dba_db_repinfo_memoidx* dba_db_repinfo_memoidx;

/**
 * Precompiled query to insert a value in repinfo
 */
struct _dba_db_repinfo
{
	struct _dba_db* db;

	dba_db_repinfo_cache cache;
	int cache_size;
	int cache_alloc_size;

	dba_db_repinfo_memoidx memo_idx;

	/*
	SQLHSTMT sfstm;
	SQLHSTMT smstm;
	SQLHSTMT istm;
	SQLHSTMT ustm;

	int lat;
	int lon;
	char ident[64];
	SQLINTEGER ident_ind;
	*/
};
typedef struct _dba_db_repinfo* dba_db_repinfo;


dba_err dba_db_repinfo_create(dba_db db, dba_db_repinfo* ins);
void dba_db_repinfo_delete(dba_db_repinfo ins);
dba_err dba_db_repinfo_get_id(dba_db_repinfo ri, const char* memo, int* id);
dba_err dba_db_repinfo_has_id(dba_db_repinfo ri, int id, int* exists);
dba_err dba_db_repinfo_update(dba_db_repinfo ri, const char* deffile);

#if 0
void dba_db_repinfo_set_ident(dba_db_repinfo ins, const char* ident);
dba_err dba_db_repinfo_get_id(dba_db_repinfo ins, int *id);
dba_err dba_db_repinfo_insert(dba_db_repinfo ins, int *id);
dba_err dba_db_repinfo_update(dba_db_repinfo ins);
#endif


#ifdef  __cplusplus
}
#endif

/* vim:set ts=4 sw=4: */
#endif
