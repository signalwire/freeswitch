/*=============================================================================
                                libwww.cpp
===============================================================================
  This is the Libwww XML transport of the C++ XML-RPC client library for
  Xmlrpc-c.
=============================================================================*/

#include <stdlib.h>
#include <cassert>
#include <string>

#include "xmlrpc-c/girerr.hpp"
using girerr::error;
using girerr::throwf;
#include "xmlrpc-c/girmem.hpp"
using girmem::autoObjectPtr;
using girmem::autoObject;
#include "env_wrap.hpp"
#include "xmlrpc-c/base.h"
#include "xmlrpc-c/client.h"
#include "xmlrpc-c/transport.h"
#include "xmlrpc-c/base_int.h"

/* transport_config.h defines MUST_BUILD_LIBWWW_CLIENT */
#include "transport_config.h"

#include "xmlrpc-c/client_transport.hpp"


using namespace std;
using namespace xmlrpc_c;


namespace {

class globalConstant {
public:
    globalConstant();
    ~globalConstant();
};



globalConstant::globalConstant() {

    // Not thread safe

    xmlrpc_transport_setup setupFn;

#if MUST_BUILD_LIBWWW_CLIENT
    setupFn = xmlrpc_libwww_transport_ops.setup_global_const;
#else
    setupFn = NULL;
#endif
    if (setupFn) {
        env_wrap env;

        setupFn(&env.env_c); // Not thread safe

        if (env.env_c.fault_occurred)
            throwf("Failed to do global initialization "
                   "of Libwww transport code.  %s", env.env_c.fault_string);
    }
}



globalConstant::~globalConstant() {

    // Not thread safe

    xmlrpc_transport_teardown teardownFn;

#if MUST_BUILD_LIBWWW_CLIENT
    teardownFn = xmlrpc_libwww_transport_ops.teardown_global_const;
#else
    teardownFn = NULL;
#endif
    if (teardownFn)
        teardownFn();  // not thread safe
}


globalConstant globalConst;
    // This object is never accessed.  Its whole purpose to to be born and
    // to die, which it does automatically as part of C++ program
    // program initialization and termination.

} // namespace


namespace xmlrpc_c {

carriageParm_libwww0::carriageParm_libwww0(
    string const serverUrl
    ) {

    this->instantiate(serverUrl);
}



carriageParm_libwww0Ptr::carriageParm_libwww0Ptr() {
    // Base class constructor will construct pointer that points to nothing
}



carriageParm_libwww0Ptr::carriageParm_libwww0Ptr(
    carriageParm_libwww0 * const carriageParmP) {
    this->point(carriageParmP);
}



carriageParm_libwww0 *
carriageParm_libwww0Ptr::operator->() const {

    autoObject * const p(this->objectP);
    return dynamic_cast<carriageParm_libwww0 *>(p);
}



#if MUST_BUILD_LIBWWW_CLIENT

clientXmlTransport_libwww::clientXmlTransport_libwww(
    string const appname,
    string const appversion) {

    this->c_transportOpsP = &xmlrpc_libwww_transport_ops;

    env_wrap env;

    xmlrpc_libwww_transport_ops.create(
        &env.env_c, 0, appname.c_str(), appversion.c_str(), NULL, 0,
        &this->c_transportP);

    if (env.env_c.fault_occurred)
        throw(error(env.env_c.fault_string));
}

#else  // MUST_BUILD_LIBWWW_CLIENT
 clientXmlTransport_libwww::clientXmlTransport_libwww(string, string) {

    throw(error("There is no Libwww client XML transport "
                "in this XML-RPC client library"));
}

#endif


clientXmlTransport_libwww::~clientXmlTransport_libwww() {

    this->c_transportOpsP->destroy(this->c_transportP);
}

} // namespace
