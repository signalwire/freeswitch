#ifndef ENV_INT_HPP_INCLUDED
#define ENV_INT_HPP_INCLUDED

#include "xmlrpc-c/c_util.h"
#include "xmlrpc-c/util.h"

namespace xmlrpc_c {

class XMLRPC_DLLEXPORT env_wrap {
/*----------------------------------------------------------------------------
   A wrapper to assist in using the Xmlrpc-c C libraries in
   Xmlrpc-c C++ code.

   To use the C libraries, you have to use type xmlrpc_env, but that type
   does not have an automatic destructor (because it's C), so it's hard
   to throw an error from a context in which a variable of that type
   exists.  This wrapper provides that automatic destructor.
-----------------------------------------------------------------------------*/
public:
    env_wrap();
    ~env_wrap();
    xmlrpc_env env_c;
};


} // namespace
#endif
