#include "api.h"
#include <limits>
#include <cstring>

using namespace std;

namespace dballe {
namespace fortran {

const signed char API::missing_byte = numeric_limits<signed char>::max();
const int API::missing_int = MISSING_INT;
const float API::missing_float = numeric_limits<float>::max();
const double API::missing_double = numeric_limits<double>::max();

const char* API::test_enqc(const char* param, unsigned len)
{
    static std::string res;

    char buf[len];
    if (!enqc(param, buf, len))
        return nullptr;

    if (buf[0] == 0)
        return nullptr;

    if (len)
    {
        --len;
        while (len > 0 && buf[len] == ' ')
            --len;
    }

    if (!len && buf[0] == ' ')
        res.clear();
    else
        res.assign(buf, len + 1);

    return res.c_str();
}

void API::to_fortran(const char* str, char* buf, unsigned buf_len)
{
    // Copy the result values
    size_t len;
    if (buf_len == 0)
        len = 0;
    else if (str)
    {
        len = strlen(str);
        if (len > buf_len)
            len = buf_len;
        memcpy(buf, str, len);
    } else {
        // The missing string value has been defined as a
        // null byte plus blank padding.
        buf[0] = 0;
        len = 1;
    }

    if (len < buf_len)
        memset(buf + len, ' ', buf_len - len);
}

void API::to_fortran(const std::string& str, char* buf, unsigned buf_len)
{
    // Copy the result values
    size_t len;
    if (buf_len == 0)
        len = 0;
    else
    {
        len = str.size();
        if (len > buf_len)
            len = buf_len;
        memcpy(buf, str.data(), len);
    }

    if (len < buf_len)
        memset(buf + len, ' ', buf_len - len);
}

}
}
