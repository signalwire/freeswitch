/*=============================================================================
                                client.cpp
===============================================================================
  This is the C++ XML-RPC client library for Xmlrpc-c.

  Note that unlike most of Xmlprc-c's C++ API, this is _not_ based on the
  C client library.  This code is independent of the C client library, and
  is based directly on the client XML transport libraries (with a little
  help from internal C utility libraries).
=============================================================================*/

#include <cassert>
#include <string>

#include "xmlrpc-c/girerr.hpp"
using girerr::error;
#include "xmlrpc-c/girmem.hpp"
using girmem::autoObjectPtr;
using girmem::autoObject;
#include "xmlrpc-c/base.h"
#include "xmlrpc-c/client.h"
#include "xmlrpc-c/transport.h"
/* transport_config.h defines XMLRPC_DEFAULT_TRANSPORT,
    MUST_BUILD_WININET_CLIENT, MUST_BUILD_CURL_CLIENT,
    MUST_BUILD_LIBWWW_CLIENT 
*/
#include "transport_config.h"

#if MUST_BUILD_WININET_CLIENT
#include "xmlrpc_wininet_transport.h"
#endif
#if MUST_BUILD_CURL_CLIENT
#include "xmlrpc_curl_transport.h"
#endif
#if MUST_BUILD_LIBWWW_CLIENT
#include "xmlrpc_libwww_transport.h"
#endif

#include "xmlrpc-c/base.hpp"
#include "xmlrpc-c/xml.hpp"
#include "xmlrpc-c/client.hpp"

using namespace std;
using namespace xmlrpc_c;


namespace {

class memblockStringWrapper {

public:    
    memblockStringWrapper(string const value) {

        xmlrpc_env env;
        xmlrpc_env_init(&env);

        this->memblockP = XMLRPC_MEMBLOCK_NEW(char, &env, 0);

        if (env.fault_occurred)
            throw(error(env.fault_string));

        XMLRPC_MEMBLOCK_APPEND(char, &env, this->memblockP,
                               value.c_str(), value.size());
        if (env.fault_occurred)
            throw(error(env.fault_string));
    }
    
    memblockStringWrapper(xmlrpc_mem_block * const memblockP) :
        memblockP(memblockP) {};

    ~memblockStringWrapper() {
        XMLRPC_MEMBLOCK_FREE(char, this->memblockP);
    }

    xmlrpc_mem_block * memblockP;
};

} // namespace

namespace xmlrpc_c {

carriageParm::carriageParm() {}



carriageParm::~carriageParm() {}



carriageParm_http0::carriageParm_http0() :
    c_serverInfoP(NULL) {}



carriageParm_http0::carriageParm_http0(string const serverUrl) {
    this->c_serverInfoP = NULL;

    this->instantiate(serverUrl);
}


carriageParm_http0::~carriageParm_http0() {

    if (this->c_serverInfoP)
        xmlrpc_server_info_free(this->c_serverInfoP);
}



void
carriageParm_http0::instantiate(string const serverUrl) {

    if (c_serverInfoP)
        throw(error("object already instantiated"));
    
    xmlrpc_env env;
    xmlrpc_env_init(&env);

    this->c_serverInfoP = xmlrpc_server_info_new(&env, serverUrl.c_str());

    if (env.fault_occurred)
        throw(error(env.fault_string));
}



void
carriageParm_http0::setBasicAuth(string const username,
                                 string const password) {

    if (!c_serverInfoP)
        throw(error("object not instantiated"));
    
    xmlrpc_env env;
    xmlrpc_env_init(&env);

    xmlrpc_server_info_set_basic_auth(
        &env, this->c_serverInfoP, username.c_str(), password.c_str());

    if (env.fault_occurred)
        throw(error(env.fault_string));
}



carriageParm_curl0::carriageParm_curl0(
    string const serverUrl
    ) {

    this->instantiate(serverUrl);
}


carriageParm_libwww0::carriageParm_libwww0(
    string const serverUrl
    ) {

    this->instantiate(serverUrl);
}



carriageParm_wininet0::carriageParm_wininet0(
    string const serverUrl
    ) {

    this->instantiate(serverUrl);
}



xmlTransaction::xmlTransaction() {}



void
xmlTransaction::finish(string const& responseXml) const {

    xml::trace("XML-RPC RESPONSE", responseXml);
}



void
xmlTransaction::finishErr(error const&) const {

}



xmlTransactionPtr::xmlTransactionPtr() {}



xmlTransaction *
xmlTransactionPtr::operator->() const {
    autoObject * const p(this->objectP);
    return dynamic_cast<xmlTransaction *>(p);
}



clientXmlTransport::~clientXmlTransport() {}



void
clientXmlTransport::start(carriageParm *    const  carriageParmP,
                          string            const& callXml,
                          xmlTransactionPtr const& xmlTranP) {
    
    string responseXml;

    this->call(carriageParmP, callXml, &responseXml);

    xmlTranP->finish(responseXml);
}



void
clientXmlTransport::finishAsync(xmlrpc_c::timeout const timeout) {
    if (timeout.finite == timeout.finite)
        throw(error("This class does not have finishAsync()"));
}



void
clientXmlTransport::asyncComplete(
    struct xmlrpc_call_info * const callInfoP,
    xmlrpc_mem_block *        const responseXmlMP,
    xmlrpc_env                const transportEnv) {

    xmlTransactionPtr * const xmlTranPP =
        reinterpret_cast<xmlTransactionPtr *>(callInfoP);

    try {
        if (transportEnv.fault_occurred) {
            (*xmlTranPP)->finishErr(error(transportEnv.fault_string));
        } else {
            string const responseXml(
                XMLRPC_MEMBLOCK_CONTENTS(char, responseXmlMP),
                XMLRPC_MEMBLOCK_SIZE(char, responseXmlMP));
            (*xmlTranPP)->finish(responseXml);
        }
    } catch(error) {
        /* We can't throw an error back to C code, and the async_complete
           interface does not provide for failure, so we define ->finish()
           as not being capable of throwing an error.
        */
        assert(false);
    }
    delete(xmlTranPP);

    /* Ordinarily, *xmlTranPP is the last reference to **xmlTranPP
       (The xmlTransaction), so it will get destroyed too.  But
       ->finish() could conceivably create a new reference to
       **xmlTranPP, and then it would keep living.
    */
}



clientXmlTransport_http::~clientXmlTransport_http() {}



void
clientXmlTransport_http::call(
    carriageParm * const  carriageParmP,
    string         const& callXml,
    string *       const  responseXmlP) {

    xmlrpc_env env;
    xmlrpc_env_init(&env);

    carriageParm_http0 * const carriageParmHttpP =
        dynamic_cast<carriageParm_http0 *>(carriageParmP);

    if (carriageParmHttpP == NULL)
        throw(error("HTTP client XML transport called with carriage "
                    "parameter object not of class carriageParm_http"));

    memblockStringWrapper callXmlM(callXml);

    xmlrpc_mem_block * responseXmlMP;

    this->c_transportOpsP->call(&env,
                                this->c_transportP,
                                carriageParmHttpP->c_serverInfoP,
                                callXmlM.memblockP,
                                &responseXmlMP);

    if (env.fault_occurred)
        throw(error(env.fault_string));

    memblockStringWrapper responseHolder(responseXmlMP);
        // Makes responseXmlMP get freed at end of scope
    
    *responseXmlP = string(XMLRPC_MEMBLOCK_CONTENTS(char, responseXmlMP),
                           XMLRPC_MEMBLOCK_SIZE(char, responseXmlMP));
}



void
clientXmlTransport_http::start(
    carriageParm *    const  carriageParmP,
    string            const& callXml,
    xmlTransactionPtr const& xmlTranP) {

    xmlrpc_env env;
    xmlrpc_env_init(&env);

    carriageParm_http0 * const carriageParmHttpP =
        dynamic_cast<carriageParm_http0 *>(carriageParmP);

    if (carriageParmHttpP == NULL)
        throw(error("HTTP client XML transport called with carriage "
                    "parameter object not of type carriageParm_http"));

    memblockStringWrapper callXmlM(callXml);

    /* xmlTranP2 is the reference to the XML transaction that is held by
       the running transaction, in C code.  It lives in dynamically allocated
       storage and xmlTranP2P points to it.
    */
    xmlTransactionPtr * const xmlTranP2P(new xmlTransactionPtr(xmlTranP));

    try {
        this->c_transportOpsP->send_request(
            &env,
            this->c_transportP,
            carriageParmHttpP->c_serverInfoP,
            callXmlM.memblockP,
            &this->asyncComplete,
            reinterpret_cast<xmlrpc_call_info *>(xmlTranP2P));

        if (env.fault_occurred)
            throw(error(env.fault_string));
    } catch (...) {
        delete xmlTranP2P;
        throw;
    }
}



void
clientXmlTransport_http::finishAsync(xmlrpc_c::timeout const timeout) {

    xmlrpc_timeoutType const c_timeoutType(
        timeout.finite ? timeout_yes : timeout_no);
    xmlrpc_timeout const c_timeout(timeout.duration);

    this->c_transportOpsP->finish_asynch(
        this->c_transportP, c_timeoutType, c_timeout);
}



#if MUST_BUILD_CURL_CLIENT

clientXmlTransport_curl::clientXmlTransport_curl(
    string const networkInterface,
    bool   const noSslVerifyPeer,
    bool   const noSslVerifyHost,
    string const userAgent) {

    struct xmlrpc_curl_xportparms transportParms; 

    transportParms.network_interface = 
        networkInterface.size() > 0 ? networkInterface.c_str() : NULL;
    transportParms.no_ssl_verifypeer = noSslVerifyPeer;
    transportParms.no_ssl_verifyhost = noSslVerifyHost;
    transportParms.user_agent =
        userAgent.size() > 0 ? userAgent.c_str() : NULL;

    this->c_transportOpsP = &xmlrpc_curl_transport_ops;

    xmlrpc_env env;
    xmlrpc_env_init(&env);

    xmlrpc_curl_transport_ops.create(
        &env, 0, "", "", (xmlrpc_xportparms *)&transportParms,
        XMLRPC_CXPSIZE(user_agent),
        &this->c_transportP);

    if (env.fault_occurred)
        throw(error(env.fault_string));
}
#else  // MUST_BUILD_CURL_CLIENT

clientXmlTransport_curl::clientXmlTransport_curl(string, bool, bool, string) {

    throw("There is no Curl client XML transport in this XML-RPC client "
          "library");
}

#endif
 


clientXmlTransport_curl::~clientXmlTransport_curl() {

    this->c_transportOpsP->destroy(this->c_transportP);
}



#if MUST_BUILD_LIBWWW_CLIENT

clientXmlTransport_libwww::clientXmlTransport_libwww(
    string const appname,
    string const appversion) {

    this->c_transportOpsP = &xmlrpc_libwww_transport_ops;

    xmlrpc_env env;
    xmlrpc_env_init(&env);

    xmlrpc_libwww_transport_ops.create(
        &env, 0, appname.c_str(), appversion.c_str(), NULL, 0,
        &this->c_transportP);

    if (env.fault_occurred)
        throw(error(env.fault_string));
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



#if MUST_BUILD_WININET_CLIENT

clientXmlTransport_wininet::clientXmlTransport_wininet(
    bool const allowInvalidSslCerts
    ) {

    struct xmlrpc_wininet_xportparms transportParms; 

    transportParms.allowInvalidSSLCerts = allowInvalidSslCerts;

    this->c_transportOpsP = &xmlrpc_wininet_transport_ops;

    xmlrpc_env env;
    xmlrpc_env_init(&env);

    xmlrpc_wininet_transport_ops.create(
        &env, 0, "", "", &transportParms, XMLRPC_WXPSIZE(allowInvalidSSLCerts),
        &this->c_transportP);

    if (env.fault_occurred)
        throw(error(env.fault_string));
}

#else  // MUST_BUILD_WININET_CLIENT

clientXmlTransport_wininet::clientXmlTransport_wininet(bool const) {

    throw(error("There is no Wininet client XML transport "
                "in this XML-RPC client library"));
}

#endif
 


clientXmlTransport_wininet::~clientXmlTransport_wininet() {

    this->c_transportOpsP->destroy(this->c_transportP);
}



clientTransaction::clientTransaction() {}



clientTransactionPtr::clientTransactionPtr() {}



clientTransactionPtr::~clientTransactionPtr() {}



clientTransaction *
clientTransactionPtr::operator->() const {
    autoObject * const p(this->objectP);
    return dynamic_cast<clientTransaction *>(p);
}



client::~client() {}


void
client::start(carriageParm *       const  carriageParmP,
              string               const  methodName,
              paramList            const& paramList,
              clientTransactionPtr const& tranP) {
/*----------------------------------------------------------------------------
   Start an RPC, wait for it to complete, and finish it.

   Usually, a derived class overrides this with something that does
   not wait for the RPC to complete, but rather arranges for something
   to finish the RPC later when the RPC does complete.
-----------------------------------------------------------------------------*/
    rpcOutcome outcome;

    this->call(carriageParmP, methodName, paramList, &outcome);

    tranP->finish(outcome);
}



client_xml::client_xml(clientXmlTransport * const transportP) :
    transportP(transportP) {}
     


void
client_xml::call(carriageParm * const  carriageParmP,
                 string         const  methodName,
                 paramList      const& paramList,
                 rpcOutcome *   const  outcomeP) {

    string callXml;
    string responseXml;

    xml::generateCall(methodName, paramList, &callXml);
    
    xml::trace("XML-RPC CALL", callXml);

    this->transportP->call(carriageParmP, callXml, &responseXml);

    xml::trace("XML-RPC RESPONSE", responseXml);

    xml::parseResponse(responseXml, outcomeP);
}
 


void
client_xml::start(carriageParm *       const  carriageParmP,
                  string               const  methodName,
                  paramList            const& paramList,
                  clientTransactionPtr const& tranP) {

    string callXml;

    xml::generateCall(methodName, paramList, &callXml);
    
    xml::trace("XML-RPC CALL", callXml);

    xmlTransaction_clientPtr const xmlTranP(tranP);

    this->transportP->start(carriageParmP, callXml, xmlTranP);
}
 


void
client_xml::finishAsync(xmlrpc_c::timeout const timeout) {

    transportP->finishAsync(timeout);
}




connection::connection(client *       const clientP,
                       carriageParm * const carriageParmP) :
    clientP(clientP), carriageParmP(carriageParmP) {}



connection::~connection() {}



rpc::rpc(string              const  methodName,
         xmlrpc_c::paramList const& paramList) {
    
    this->state      = STATE_UNFINISHED;
    this->methodName = methodName;
    this->paramList  = paramList;
}



rpc::~rpc() {

    if (this->state == STATE_ERROR)
        delete(this->errorP);
}



void
rpc::call(client       * const clientP,
          carriageParm * const carriageParmP) {

    if (this->state != STATE_UNFINISHED)
        throw(error("Attempt to execute an RPC that has already been "
                    "executed"));

    clientP->call(carriageParmP,
                  this->methodName,
                  this->paramList,
                  &this->outcome);

    this->state = outcome.succeeded() ? STATE_SUCCEEDED : STATE_FAILED;
}



void
rpc::call(connection const& connection) {

    this->call(connection.clientP, connection.carriageParmP);

}


 
void
rpc::start(client       * const clientP,
           carriageParm * const carriageParmP) {
    
    if (this->state != STATE_UNFINISHED)
        throw(error("Attempt to execute an RPC that has already been "
                    "executed"));

    clientP->start(carriageParmP,
                   this->methodName,
                   this->paramList,
                   rpcPtr(this));
}


 
void
rpc::start(xmlrpc_c::connection const& connection) {
    
    this->start(connection.clientP, connection.carriageParmP);
}



void
rpc::finish(rpcOutcome const& outcome) {

    this->state = outcome.succeeded() ? STATE_SUCCEEDED : STATE_FAILED;

    this->outcome = outcome;

    this->notifyComplete();
}



void
rpc::finishErr(error const& error) {

    this->state = STATE_ERROR;
    this->errorP = new girerr::error(error);
    this->notifyComplete();
}



void
rpc::notifyComplete() {
/*----------------------------------------------------------------------------
   Anyone who does RPCs asynchronously and doesn't use polling will
   want to make his own class derived from 'rpc' and override this
   with a notifyFinish() that does something.

   Typically, notifyFinish() will queue the RPC so some other thread
   will deal with the fact that the RPC is finished.


   In the absence of the aforementioned queueing, the RPC becomes
   unreferenced as soon as our Caller releases his reference, so the
   RPC gets destroyed when we return.
-----------------------------------------------------------------------------*/

}

    

value
rpc::getResult() const {

    switch (this->state) {
    case STATE_UNFINISHED:
        throw(error("Attempt to get result of RPC that is not finished."));
        break;
    case STATE_ERROR:
        throw(*this->errorP);
        break;
    case STATE_FAILED:
        throw(error("RPC failed.  " +
                    this->outcome.getFault().getDescription()));
        break;
    case STATE_SUCCEEDED: {
        // All normal
    }
    }

    return this->outcome.getResult();
}




fault
rpc::getFault() const {

    switch (this->state) {
    case STATE_UNFINISHED:
        throw(error("Attempt to get fault from RPC that is not finished"));
        break;
    case STATE_ERROR:
        throw(*this->errorP);
        break;
    case STATE_SUCCEEDED:
        throw(error("Attempt to get fault from an RPC that succeeded"));
        break;
    case STATE_FAILED: {
        // All normal
    }
    }

    return this->outcome.getFault();
}



bool
rpc::isFinished() const {
    return (this->state != STATE_UNFINISHED);
}



bool
rpc::isSuccessful() const {
    return (this->state == STATE_SUCCEEDED);
}



rpcPtr::rpcPtr() {}



rpcPtr::rpcPtr(rpc * const rpcP) {
    this->instantiate(rpcP);
}



rpcPtr::rpcPtr(string              const  methodName,
               xmlrpc_c::paramList const& paramList) {

    this->instantiate(new rpc(methodName, paramList));
}



rpc *
rpcPtr::operator->() const {

    autoObject * const p(this->objectP);
    return dynamic_cast<rpc *>(p);
}



xmlTransaction_client::xmlTransaction_client(
    clientTransactionPtr const& tranP) :
    tranP(tranP) {}



void
xmlTransaction_client::finish(string const& responseXml) const {

    xml::trace("XML-RPC RESPONSE", responseXml);

    try {
        rpcOutcome outcome;
    
        xml::parseResponse(responseXml, &outcome);

        this->tranP->finish(outcome);
    } catch (error const caughtError) {
        this->tranP->finishErr(caughtError);
    }
}



void
xmlTransaction_client::finishErr(error const& error) const {

    this->tranP->finishErr(error);
}



xmlTransaction_clientPtr::xmlTransaction_clientPtr() {}



xmlTransaction_clientPtr::xmlTransaction_clientPtr(
    clientTransactionPtr const& tranP) {

    this->instantiate(new xmlTransaction_client(tranP));
}



xmlTransaction_client *
xmlTransaction_clientPtr::operator->() const {
    autoObject * const p(this->objectP);
    return dynamic_cast<xmlTransaction_client *>(p);
}



} // namespace
