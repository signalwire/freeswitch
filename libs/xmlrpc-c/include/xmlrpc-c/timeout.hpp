#ifndef XMLRPC_TIMEOUT_H_INCLUDED
#define XMLRPC_TIMEOUT_H_INCLUDED

#include <xmlrpc-c/c_util.h>

namespace xmlrpc_c {

struct XMLRPC_DLLEXPORT timeout {

    timeout() : finite(false) {}

    timeout(unsigned int const duration) :
        finite(true), duration(duration) {}
        // 'duration' is the timeout time in milliseconds

    bool finite;
    unsigned int duration;  // in milliseconds
};


} // namespace

#endif
