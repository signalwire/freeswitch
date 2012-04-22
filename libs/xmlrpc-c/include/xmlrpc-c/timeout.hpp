#ifndef XMLRPC_TIMEOUT_H_INCLUDED
#define XMLRPC_TIMEOUT_H_INCLUDED

namespace xmlrpc_c {

struct timeout {

    timeout() : finite(false) {}

    timeout(unsigned int const duration) : duration(duration) {}
        // 'duration' is the timeout time in milliseconds

    bool finite;
    unsigned int duration;  // in milliseconds
};


} // namespace

#endif
