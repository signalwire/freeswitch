/*=============================================================================
                                curl.cpp
===============================================================================
  This is the Curl XML transport of the C++ XML-RPC client library for
  Xmlrpc-c.

  Note that unlike most of Xmlprc-c's C++ API, this is _not_ based on the
  C client library.  This code is independent of the C client library, and
  is based directly on the client XML transport libraries (with a little
  help from internal C utility libraries).
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

/* transport_config.h defines MUST_BUILD_CURL_CLIENT */
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

#if MUST_BUILD_CURL_CLIENT
    setupFn = xmlrpc_curl_transport_ops.setup_global_const;
#else
    setupFn = NULL;
#endif
    if (setupFn) {
        env_wrap env;

        setupFn(&env.env_c); // Not thread safe
        
        if (env.env_c.fault_occurred)
            throwf("Failed to do global initialization "
                   "of Curl transport code.  %s", env.env_c.fault_string);
    }
}



globalConstant::~globalConstant() {

    // Not thread safe

    xmlrpc_transport_teardown teardownFn;

#if MUST_BUILD_CURL_CLIENT
    teardownFn = xmlrpc_curl_transport_ops.teardown_global_const;
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

carriageParm_curl0::carriageParm_curl0(
    string const serverUrl
    ) {

    this->instantiate(serverUrl);
}



carriageParm_curl0Ptr::carriageParm_curl0Ptr() {
    // Base class constructor will construct pointer that points to nothing
}



carriageParm_curl0Ptr::carriageParm_curl0Ptr(
    carriageParm_curl0 * const carriageParmP) {
    this->point(carriageParmP);
}



carriageParm_curl0 *
carriageParm_curl0Ptr::operator->() const {

    autoObject * const p(this->objectP);
    return dynamic_cast<carriageParm_curl0 *>(p);
}



clientXmlTransport_curl::constrOpt::constrOpt() {

    present.network_interface = false;
    present.no_ssl_verifypeer = false;
    present.no_ssl_verifyhost = false;
    present.user_agent = false;
    present.ssl_cert = false;
    present.sslcerttype = false;
    present.sslcertpasswd = false;
    present.sslkey = false;
    present.sslkeytype = false;
    present.sslkeypasswd = false;
    present.sslengine = false;
    present.sslengine_default = false;
    present.sslversion = false;
    present.cainfo = false;
    present.capath = false;
    present.randomfile = false;
    present.egdsocket = false;
    present.ssl_cipher_list = false;
    present.timeout = false;
}



#define DEFINE_OPTION_SETTER(OPTION_NAME, TYPE) \
clientXmlTransport_curl::constrOpt & \
clientXmlTransport_curl::constrOpt::OPTION_NAME(TYPE const& arg) { \
    this->value.OPTION_NAME = arg; \
    this->present.OPTION_NAME = true; \
    return *this; \
}

DEFINE_OPTION_SETTER(network_interface, string);
DEFINE_OPTION_SETTER(no_ssl_verifypeer, bool);
DEFINE_OPTION_SETTER(no_ssl_verifyhost, bool);
DEFINE_OPTION_SETTER(user_agent, string);
DEFINE_OPTION_SETTER(ssl_cert, string);
DEFINE_OPTION_SETTER(sslcerttype, string);
DEFINE_OPTION_SETTER(sslcertpasswd, string);
DEFINE_OPTION_SETTER(sslkey, string);
DEFINE_OPTION_SETTER(sslkeytype, string);
DEFINE_OPTION_SETTER(sslkeypasswd, string);
DEFINE_OPTION_SETTER(sslengine, string);
DEFINE_OPTION_SETTER(sslengine_default, bool);
DEFINE_OPTION_SETTER(sslversion, xmlrpc_sslversion);
DEFINE_OPTION_SETTER(cainfo, string);
DEFINE_OPTION_SETTER(capath, string);
DEFINE_OPTION_SETTER(randomfile, string);
DEFINE_OPTION_SETTER(egdsocket, string);
DEFINE_OPTION_SETTER(ssl_cipher_list, string);
DEFINE_OPTION_SETTER(timeout, unsigned int);

#undef DEFINE_OPTION_SETTER

#if MUST_BUILD_CURL_CLIENT

void
clientXmlTransport_curl::initialize(constrOpt const& opt) {
    struct xmlrpc_curl_xportparms transportParms; 

    transportParms.network_interface = opt.present.network_interface ?
        opt.value.network_interface.c_str() : NULL;
    transportParms.no_ssl_verifypeer = opt.present.no_ssl_verifypeer ? 
        opt.value.no_ssl_verifypeer         : false;
    transportParms.no_ssl_verifyhost = opt.present.no_ssl_verifyhost ? 
        opt.value.no_ssl_verifyhost         : false;
    transportParms.user_agent        = opt.present.user_agent ?
        opt.value.user_agent.c_str()        : NULL;
    transportParms.ssl_cert          = opt.present.ssl_cert ?
        opt.value.ssl_cert.c_str()          : NULL;
    transportParms.sslcerttype       = opt.present.sslcerttype ?
        opt.value.sslcerttype.c_str()       : NULL;
    transportParms.sslcertpasswd     = opt.present.sslcertpasswd ?
        opt.value.sslcertpasswd.c_str()     : NULL;
    transportParms.sslkey            = opt.present.sslkey ?
        opt.value.sslkey.c_str()            : NULL;
    transportParms.sslkeytype        = opt.present.sslkeytype ?
        opt.value.sslkeytype.c_str()        : NULL;
    transportParms.sslkeypasswd      = opt.present.sslkeypasswd ?
        opt.value.sslkeypasswd.c_str()      : NULL;
    transportParms.sslengine         = opt.present.sslengine ?
        opt.value.sslengine.c_str()         : NULL;
    transportParms.sslengine_default = opt.present.sslengine_default ? 
        opt.value.sslengine_default         : false;
    transportParms.sslversion        = opt.present.sslversion ? 
        opt.value.sslversion                : XMLRPC_SSLVERSION_DEFAULT;
    transportParms.cainfo            = opt.present.cainfo ?
        opt.value.cainfo.c_str()            : NULL;
    transportParms.capath            = opt.present.capath ?
        opt.value.capath.c_str()            : NULL;
    transportParms.randomfile        = opt.present.randomfile ? 
        opt.value.randomfile.c_str()        : NULL;
    transportParms.egdsocket         = opt.present.egdsocket ?
        opt.value.egdsocket.c_str()         : NULL;
    transportParms.ssl_cipher_list   = opt.present.ssl_cipher_list ? 
        opt.value.ssl_cipher_list.c_str()   : NULL;
    transportParms.timeout           = opt.present.timeout ? 
        opt.value.timeout                   : 0;

    this->c_transportOpsP = &xmlrpc_curl_transport_ops;

    env_wrap env;

    xmlrpc_curl_transport_ops.create(
        &env.env_c, 0, "", "",
        &transportParms, XMLRPC_CXPSIZE(timeout),
        &this->c_transportP);

    if (env.env_c.fault_occurred)
        throw(error(env.env_c.fault_string));
}

#else  // MUST_BUILD_CURL_CLIENT

void
clientXmlTransport_curl::initialize(constrOpt const& opt) {

    throw(error("There is no Curl client XML transport in this XML-RPC client "
                "library"));
}

#endif

clientXmlTransport_curl::clientXmlTransport_curl(constrOpt const& opt) {

    this->initialize(opt);
}



clientXmlTransport_curl::clientXmlTransport_curl(
    string const networkInterface,
    bool   const noSslVerifyPeer,
    bool   const noSslVerifyHost,
    string const userAgent) {

    clientXmlTransport_curl::constrOpt opt;

    if (networkInterface.size() > 0)
        opt.network_interface(networkInterface);
    opt.no_ssl_verifypeer(noSslVerifyPeer);
    opt.no_ssl_verifyhost(noSslVerifyHost);
    if (userAgent.size() > 0)
        opt.user_agent(userAgent);

    this->initialize(opt);
}



clientXmlTransport_curl::~clientXmlTransport_curl() {

    this->c_transportOpsP->destroy(this->c_transportP);
}


} // namespace
