#ifndef GIRERR_HPP_INCLUDED
#define GIRERR_HPP_INCLUDED

#include <string>
#include <exception>

#define HAVE_GIRERR_ERROR

namespace girerr {

class error : public std::exception {
public:
    error(std::string const& what_arg) : _what(what_arg) {}

    ~error() throw() {}

    virtual const char *
    what() const throw() { return this->_what.c_str(); };

private:
    std::string _what;
};

// throwf() always throws a girerr::error .

void
throwf(const char * const format, ...);

} // namespace

#endif
