#ifndef XMLRPC_TIMEOUT_H_INCLUDED
#define XMLRPC_TIMEOUT_H_INCLUDED

namespace xmlrpc_c {

struct timeout {

    timeout() : finite(false) {}

    timeout(unsigned int const duration) : duration(duration) {}

    bool finite;
    unsigned int duration;
};


} // namespace

#endif
