#include <string>

#include "xmlrpc-c/string_int.h"
#include "xmlrpc-c/girerr.hpp"

using namespace std;

namespace girerr {

void
throwf(const char * const format, ...) {

    va_list varargs;
    va_start(varargs, format);

    const char * value;
    xmlrpc_vasprintf(&value, format, varargs);
    
    string const valueString(value);

    xmlrpc_strfree(value);

    throw(girerr::error(valueString));

    va_end(varargs);
}

} // namespace
