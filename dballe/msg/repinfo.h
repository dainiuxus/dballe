/*
 * DB-ALLe - Archive for punctual meteorological data
 *
 * Copyright (C) 2010  ARPA-SIM <urpsim@smr.arpa.emr.it>
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

#ifndef DBA_MSG_REPINFO_H
#define DBA_MSG_REPINFO_H

#ifdef  __cplusplus
extern "C" {
#endif

/** @file
 * @ingroup msg
 *
 * rep_cod/rep_memo handling functions
 */

#include <dballe/core/error.h>

/**
 * Create a new dba_msg
 *
 * Return the file name for the default repinfo.csv
 *
 * @retval fname
 *   The file name (which either points to the environment or to a static
 *   string, and does not need to be deallocated)
 * @return
 *   The error indicator for the function (See @ref error.h)
 */
dba_err dba_repinfo_default_filename(const char** fname);

#ifdef  __cplusplus
}
#endif

// vim:set ts=4 sw=4:
#endif
