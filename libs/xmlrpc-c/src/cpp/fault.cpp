#include <string>

#include "xmlrpc-c/girerr.hpp"
using girerr::error;
#include "xmlrpc-c/base.hpp"

using namespace std;

namespace xmlrpc_c {

fault::fault() : valid(false) {};
    
fault::fault(string                  const _description,
             xmlrpc_c::fault::code_t const _code
             ) :
    valid(true),
    code(_code),
    description(_description)
    {}

xmlrpc_c::fault::code_t
fault::getCode() const {
    if (!valid)
        throw(error("Attempt to access placeholder xmlrpc_c::fault object"));
    return this->code;
}

string
fault::getDescription() const {
    if (!valid)
        throw(error("Attempt to access placeholder xmlrpc_c::fault object"));
    return this->description;
}

} // namespace
