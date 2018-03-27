#ifndef DBALLE_CORE_STRING_H
#define DBALLE_CORE_STRING_H

#include <string>

namespace dballe {

/**
 * Look for the given argument in the URL query string, remove it from the url
 * string and return its value.
 *
 * If the argument is not found, returns the empty string
 */
std::string url_pop_query_string(std::string& url, const std::string& name);

}

#endif
