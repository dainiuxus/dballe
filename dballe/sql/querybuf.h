/** @file
 * Buffer used to build SQL queries
 */
#ifndef DBA_SQL_QUERYBUF_H
#define DBA_SQL_QUERYBUF_H

#include <wreport/varinfo.h>
#include <string>
#include <set>

namespace dballe {
namespace sql {

/// String buffer for composing database queries
struct Querybuf : public std::string
{
	bool list_first;
	char list_sep[10];

	/**
	 * @param reserve
	 *   Initial preallocated size for the buffer. If this is chosen
	 *   wisely, there is no need to reallocate space while composing the
	 *   query.
	 */
	Querybuf(size_t reserve = 512);
	~Querybuf();

	/// Reset the querybuf to contain the empty string
	void clear();

	/**
	 * Begin a list of items separated by the given separator.  Items are added
	 * using append_list().
	 *
	 * @param sep
	 *   The separator to add between every list item
	 */
	void start_list(const char* sep);

	/**
	 * Append a formatted string to the querybuf
	 *
	 * @param fmt
	 *   The string to append, which will be formatted in printf style
	 */
	void appendf(const char* fmt, ...) __attribute__((format(printf, 2, 3)));

	/**
	 * Append a string to the querybuf, as part of a list.
	 *
	 * This function will prepend str with the current list separator, unless it is
	 * the first item added to the list.
	 *
	 * @param str
	 *   The string to append
	 */
	void append_list(const char* str);

    /**
     * Append a formatted string to the querybuf, as part of a list.
     *
     * This function will prepend str with the current list separator, unless it is
     * the first item added to the list.
     *
     * @param fmt printf-style format string.
     */
    void append_listf(const char* fmt, ...) __attribute__((format(printf, 2, 3)));

    /**
     * Append a comma-separated list of integer varcodes parsed from a
     * varlist=B12101,B12013 query parameter
     */
    void append_varlist(const std::string& varlist);

    /// Append a comma-separated list of integer varcodes
    void append_varlist(const std::set<wreport::Varcode>& varlist);
};

}
}
#endif
