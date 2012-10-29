#ifndef GIRERR_HPP_INCLUDED
#define GIRERR_HPP_INCLUDED

#include <string>
#include <exception>

#include <xmlrpc-c/c_util.h>

#define HAVE_GIRERR_ERROR

namespace girerr {

class XMLRPC_DLLEXPORT error : public std::exception {
public:
    error(std::string const& what_arg) : _what(what_arg) {}

    ~error() throw() {}

    virtual const char *
    what() const throw() { return this->_what.c_str(); };

private:
    std::string _what;
};

// throwf() always throws a girerr::error .

XMLRPC_DLLEXPORT
void
throwf(const char * const format, ...)
  XMLRPC_PRINTF_ATTR(1,2)
  XMLRPC_NORETURN_ATTR;

} // namespace

#endif
