/*=============================================================================
                                client.cpp
===============================================================================
  This is the C++ XML-RPC client library for Xmlrpc-c.

  Note that unlike most of Xmlprc-c's C++ API, this is _not_ based on the
  C client library.  This code is independent of the C client library, and
  is based directly on the client XML transport libraries (with a little
  help from internal C utility libraries).
=============================================================================*/

#include <stdlib.h>
#include <cassert>
#include <string>
#include <vector>

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
#include "xmlrpc-c/base.hpp"
#include "xmlrpc-c/xml.hpp"
#include "xmlrpc-c/client.hpp"
#include "transport_config.h"

using namespace std;
using namespace xmlrpc_c;


namespace {

void
throwIfError(env_wrap const& env) {

    if (env.env_c.fault_occurred)
        throw(error(env.env_c.fault_string));
}



class memblockStringWrapper {

public:    
    memblockStringWrapper(string const value) {

        env_wrap env;

        this->memblockP = XMLRPC_MEMBLOCK_NEW(char, &env.env_c, 0);
        throwIfError(env);

        XMLRPC_MEMBLOCK_APPEND(char, &env.env_c, this->memblockP,
                               value.c_str(), value.size());
        throwIfError(env);
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

struct client_xml_impl {
    /* We have both kinds of pointers to give the user flexibility -- we
       have constructors that take both.  But the simple pointer
       'transportP' is valid in both cases.
    */
    clientXmlTransport * transportP;
    clientXmlTransportPtr transportPtr;
    xmlrpc_dialect dialect;

    client_xml_impl(clientXmlTransport * const transportP,
                    xmlrpc_dialect       const dialect = xmlrpc_dialect_i8) :
        transportP(transportP),
        dialect(dialect) {}

    client_xml_impl(clientXmlTransportPtr const transportPtr,
                    clientXmlTransport *  const transportP,
                    xmlrpc_dialect        const dialect = xmlrpc_dialect_i8) :
        transportP(transportP),
        transportPtr(transportPtr),
        dialect(dialect) {}
};
     


carriageParm::carriageParm() {}



carriageParm::~carriageParm() {}



carriageParmPtr::carriageParmPtr() {
    // Base class constructor will construct pointer that points to nothing
}



carriageParmPtr::carriageParmPtr(
carriageParm * const carriageParmP) : autoObjectPtr(carriageParmP) {}



carriageParm *
carriageParmPtr::operator->() const {

    autoObject * const p(this->objectP);
    return dynamic_cast<carriageParm *>(p);
}



carriageParm *
carriageParmPtr::get() const {
    return dynamic_cast<carriageParm *>(objectP);
}



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
    
    env_wrap env;

    this->c_serverInfoP =
        xmlrpc_server_info_new(&env.env_c, serverUrl.c_str());
    throwIfError(env);
}



void
carriageParm_http0::setUser(string const userid,
                            string const password) {

    if (!this->c_serverInfoP)
        throw(error("object not instantiated"));

    env_wrap env;

    xmlrpc_server_info_set_user(
        &env.env_c, this->c_serverInfoP, userid.c_str(), password.c_str());

    if (env.env_c.fault_occurred)
        throw(error(env.env_c.fault_string));
}



void
carriageParm_http0::allowAuthBasic() {

    if (!this->c_serverInfoP)
        throw(error("object not instantiated"));

    env_wrap env;

    xmlrpc_server_info_allow_auth_basic(&env.env_c, this->c_serverInfoP);

    if (env.env_c.fault_occurred)
        throw(error(env.env_c.fault_string));
}



void
carriageParm_http0::disallowAuthBasic() {

    if (!this->c_serverInfoP)
        throw(error("object not instantiated"));

    env_wrap env;

    xmlrpc_server_info_disallow_auth_basic(&env.env_c, this->c_serverInfoP);

    if (env.env_c.fault_occurred)
        throw(error(env.env_c.fault_string));
}



void
carriageParm_http0::allowAuthDigest() {

    if (!this->c_serverInfoP)
        throw(error("object not instantiated"));

    env_wrap env;

    xmlrpc_server_info_allow_auth_digest(&env.env_c, this->c_serverInfoP);

    if (env.env_c.fault_occurred)
        throw(error(env.env_c.fault_string));
}



void
carriageParm_http0::disallowAuthDigest() {

    if (!this->c_serverInfoP)
        throw(error("object not instantiated"));

    env_wrap env;

    xmlrpc_server_info_disallow_auth_digest(&env.env_c, this->c_serverInfoP);

    if (env.env_c.fault_occurred)
        throw(error(env.env_c.fault_string));
}



void
carriageParm_http0::allowAuthNegotiate() {

    if (!this->c_serverInfoP)
        throw(error("object not instantiated"));

    env_wrap env;

    xmlrpc_server_info_allow_auth_negotiate(&env.env_c, this->c_serverInfoP);

    if (env.env_c.fault_occurred)
        throw(error(env.env_c.fault_string));
}



void
carriageParm_http0::disallowAuthNegotiate() {

    if (!this->c_serverInfoP)
        throw(error("object not instantiated"));

    env_wrap env;

    xmlrpc_server_info_disallow_auth_negotiate(
        &env.env_c, this->c_serverInfoP);

    if (env.env_c.fault_occurred)
        throw(error(env.env_c.fault_string));
}



void
carriageParm_http0::allowAuthNtlm() {

    if (!this->c_serverInfoP)
        throw(error("object not instantiated"));

    env_wrap env;

    xmlrpc_server_info_allow_auth_ntlm(&env.env_c, this->c_serverInfoP);

    if (env.env_c.fault_occurred)
        throw(error(env.env_c.fault_string));
}



void
carriageParm_http0::disallowAuthNtlm() {

    if (!this->c_serverInfoP)
        throw(error("object not instantiated"));

    env_wrap env;

    xmlrpc_server_info_disallow_auth_ntlm(&env.env_c, this->c_serverInfoP);

    if (env.env_c.fault_occurred)
        throw(error(env.env_c.fault_string));
}



void
carriageParm_http0::setBasicAuth(string const username,
                                 string const password) {

    if (!this->c_serverInfoP)
        throw(error("object not instantiated"));
    
    env_wrap env;

    xmlrpc_server_info_set_basic_auth(
        &env.env_c, this->c_serverInfoP, username.c_str(), password.c_str());
    throwIfError(env);
}



carriageParm_http0Ptr::carriageParm_http0Ptr() {
    // Base class constructor will construct pointer that points to nothing
}



carriageParm_http0Ptr::carriageParm_http0Ptr(
    carriageParm_http0 * const carriageParmP) :
    carriageParmPtr(carriageParmP) {}



carriageParm_http0 *
carriageParm_http0Ptr::operator->() const {

    autoObject * const p(this->objectP);
    return dynamic_cast<carriageParm_http0 *>(p);
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



xmlTransactionPtr::xmlTransactionPtr(xmlTransaction * xmlTransP) :
    autoObjectPtr(xmlTransP) {}
 


xmlTransaction *
xmlTransactionPtr::operator->() const {
    autoObject * const p(this->objectP);
    return dynamic_cast<xmlTransaction *>(p);
}



struct xmlTranCtl {
/*----------------------------------------------------------------------------
   This contains information needed to conduct a transaction.  You
   construct it as you start the transaction and destroy it after the
   work is done.  You need this only for an asynchronous one, because
   where the user starts and finishes the RPC in the same
   libxmlrpc_client call, you can just keep this information in
   various stack variables, and it's faster and easier to understand
   that way.

   The C transport is designed to take a xmlrpc_call_info argument for
   similar stuff needed by the the C client object.  But it's really
   opaque to the transport, so we just let xmlTranCtl masquerade as
   xmlprc_call_info in our call to the C transport.
-----------------------------------------------------------------------------*/
    xmlTranCtl(xmlTransactionPtr const& xmlTranP,
               string            const& callXml) :

        xmlTranP(xmlTranP) {

        env_wrap env;

        this->callXmlP = XMLRPC_MEMBLOCK_NEW(char, &env.env_c, 0);
        throwIfError(env);

        XMLRPC_MEMBLOCK_APPEND(char, &env.env_c, this->callXmlP,
                               callXml.c_str(), callXml.size());
        throwIfError(env);
    }
    
    ~xmlTranCtl() {
        XMLRPC_MEMBLOCK_FREE(char, this->callXmlP);
    }

    xmlTransactionPtr const xmlTranP;
        // The transaction we're controlling.  Most notable use of this is
        // that this object we inform when the transaction is done.  This
        // is where the response XML and other transaction results go.

    xmlrpc_mem_block * callXmlP;
        // The XML of the call.  This is what the transport transports.
};



clientXmlTransport::~clientXmlTransport() {}



void
clientXmlTransport::start(carriageParm *    const  carriageParmP,
                          string            const& callXml,
                          xmlTransactionPtr const& xmlTranP) {

    // Note: derived class clientXmlTransport_http overrides this,
    // so it doesn't normally get used.
    
    string responseXml;

    this->call(carriageParmP, callXml, &responseXml);

    xmlTranP->finish(responseXml);
}



void
clientXmlTransport::finishAsync(xmlrpc_c::timeout) {

    // Since our start() does the whole thing, there's nothing for
    // us to do.

    // A derived class that overrides start() with something properly
    // asynchronous had better also override finishAsync() with something
    // substantial.
}



void
clientXmlTransport::asyncComplete(
    struct xmlrpc_call_info * const callInfoP,
    xmlrpc_mem_block *        const responseXmlMP,
    xmlrpc_env                const transportEnv) {

    xmlTranCtl * const xmlTranCtlP = reinterpret_cast<xmlTranCtl *>(callInfoP);

    try {
        if (transportEnv.fault_occurred) {
            xmlTranCtlP->xmlTranP->finishErr(error(transportEnv.fault_string));
        } else {
            string const responseXml(
                XMLRPC_MEMBLOCK_CONTENTS(char, responseXmlMP),
                XMLRPC_MEMBLOCK_SIZE(char, responseXmlMP));
            xmlTranCtlP->xmlTranP->finish(responseXml);
        }
    } catch(error) {
        /* We can't throw an error back to C code, and the async_complete
           interface does not provide for failure, so we define ->finish()
           as not being capable of throwing an error.
        */
        assert(false);
    }
    delete(xmlTranCtlP);

    /* Ordinarily, *xmlTranCtlP is the last reference to
       xmlTranCtlP->xmlTranP, so that will get destroyed too.  But
       ->finish() could conceivably create a new reference to
       xmlTranCtlP->xmlTranP, and then it would keep living.
    */
}



void
clientXmlTransport::setInterrupt(int *) {

    throwf("The client XML transport is not interruptible");
}



clientXmlTransportPtr::clientXmlTransportPtr() {
    // Base class constructor will construct pointer that points to nothing
}



clientXmlTransportPtr::clientXmlTransportPtr(
    clientXmlTransport * const transportP) : autoObjectPtr(transportP) {}



clientXmlTransport *
clientXmlTransportPtr::get() const {
    return dynamic_cast<clientXmlTransport *>(objectP);
}



clientXmlTransport *
clientXmlTransportPtr::operator->() const {

    autoObject * const p(this->objectP);
    return dynamic_cast<clientXmlTransport *>(p);
}



clientXmlTransport_http::~clientXmlTransport_http() {}



void
clientXmlTransport_http::call(
    carriageParm * const  carriageParmP,
    string         const& callXml,
    string *       const  responseXmlP) {

    carriageParm_http0 * const carriageParmHttpP =
        dynamic_cast<carriageParm_http0 *>(carriageParmP);

    if (carriageParmHttpP == NULL)
        throw(error("HTTP client XML transport called with carriage "
                    "parameter object not of class carriageParm_http"));

    memblockStringWrapper callXmlM(callXml);

    xmlrpc_mem_block * responseXmlMP;

    env_wrap env;

    this->c_transportOpsP->call(&env.env_c,
                                this->c_transportP,
                                carriageParmHttpP->c_serverInfoP,
                                callXmlM.memblockP,
                                &responseXmlMP);

    throwIfError(env);

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

    env_wrap env;

    carriageParm_http0 * const carriageParmHttpP =
        dynamic_cast<carriageParm_http0 *>(carriageParmP);

    if (carriageParmHttpP == NULL)
        throw(error("HTTP client XML transport called with carriage "
                    "parameter object not of type carriageParm_http"));

    xmlTranCtl * const tranCtlP(new xmlTranCtl(xmlTranP, callXml));

    try {
        this->c_transportOpsP->send_request(
            &env.env_c,
            this->c_transportP,
            carriageParmHttpP->c_serverInfoP,
            tranCtlP->callXmlP,
            &this->asyncComplete,
            reinterpret_cast<xmlrpc_call_info *>(tranCtlP));

        throwIfError(env);
    } catch (...) {
        delete tranCtlP;
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



void
clientXmlTransport_http::setInterrupt(int * const interruptP) {

    if (this->c_transportOpsP->set_interrupt)
        this->c_transportOpsP->set_interrupt(this->c_transportP, interruptP);
}



bool const haveCurl(
#if MUST_BUILD_CURL_CLIENT
true
#else
false
#endif
);

bool const haveLibwww(
#if MUST_BUILD_LIBWWW_CLIENT
true
#else
false
#endif
);

bool const haveWininet(
#if MUST_BUILD_WININET_CLIENT
true
#else
false
#endif
);



vector<string>
clientXmlTransport_http::availableTypes() {

    vector<string> retval;

    if (haveCurl)
        retval.push_back("curl");

    if (haveLibwww)
        retval.push_back("libwww");

    if (haveWininet)
        retval.push_back("wininet");

    return retval;
}



clientXmlTransportPtr
clientXmlTransport_http::create() {
/*----------------------------------------------------------------------------
  Make an HTTP Client XML transport of any kind (Caller doesn't care).

  Caller can find out what kind he got by trying dynamic casts.

  Caller can use a carriageParm_http0 with the transport.
-----------------------------------------------------------------------------*/
    if (haveCurl)
        return clientXmlTransportPtr(new clientXmlTransport_curl());
    else if (haveLibwww)
        return clientXmlTransportPtr(new clientXmlTransport_libwww());
    else if (haveWininet)
        return clientXmlTransportPtr(new clientXmlTransport_wininet());
    else
        throwf("This XML-RPC client library contains no HTTP XML transports");
}



clientTransaction::clientTransaction() {}



clientTransactionPtr::clientTransactionPtr() {}



clientTransactionPtr::clientTransactionPtr(
    clientTransaction * const transP) : autoObjectPtr(transP) {}



clientTransactionPtr::~clientTransactionPtr() {}



clientTransaction *
clientTransactionPtr::operator->() const {
    autoObject * const p(this->objectP);
    return dynamic_cast<clientTransaction *>(p);
}



client::~client() {}



void
client::start(carriageParm *       const  carriageParmP,
              string               const& methodName,
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



void
client::finishAsync(xmlrpc_c::timeout) {

    // Since our start() does the whole thing, there's nothing for
    // us to do.

    // A derived class that overrides start() with something properly
    // asynchronous had better also override finishAsync() with something
    // substantial.
}



void
client::setInterrupt(int *) {

    throwf("Clients of this type are not interruptible");
}



clientPtr::clientPtr() {
    // Base class constructor will construct pointer that points to nothing
}



clientPtr::clientPtr(
    client * const clientP) : autoObjectPtr(clientP) {}



client *
clientPtr::operator->() const {

    autoObject * const p(this->objectP);
    return dynamic_cast<client *>(p);
}



client *
clientPtr::get() const {
    return dynamic_cast<client *>(objectP);
}



client_xml::client_xml(clientXmlTransport * const transportP) {

    this->implP = new client_xml_impl(transportP);
}



client_xml::client_xml(clientXmlTransportPtr const transportPtr) {

    this->implP = new client_xml_impl(transportPtr, transportPtr.get());
}



client_xml::client_xml(clientXmlTransport * const transportP,
                       xmlrpc_dialect       const dialect) {

    this->implP = new client_xml_impl(transportP, dialect);
}



client_xml::client_xml(clientXmlTransportPtr const transportPtr,
                       xmlrpc_dialect        const dialect) {

    this->implP = new client_xml_impl(transportPtr, transportPtr.get(),
                                      dialect);
}



client_xml::~client_xml() {

    delete(this->implP);
}



void
client_xml::call(carriageParm * const  carriageParmP,
                 string         const& methodName,
                 paramList      const& paramList,
                 rpcOutcome *   const  outcomeP) {

    string callXml;
    string responseXml;

    xml::generateCall(methodName, paramList, this->implP->dialect, &callXml);
    
    xml::trace("XML-RPC CALL", callXml);

    try {
        this->implP->transportP->call(carriageParmP, callXml, &responseXml);
    } catch (error const& error) {
        throwf("Unable to transport XML to server and "
               "get XML response back.  %s", error.what());
    }
    xml::trace("XML-RPC RESPONSE", responseXml);
        
    try {
        xml::parseResponse(responseXml, outcomeP);
    } catch (error const& error) {
        throwf("Response XML from server is not valid XML-RPC response.  %s",
               error.what());
    }
}
 


void
client_xml::start(carriageParm *       const  carriageParmP,
                  string               const& methodName,
                  paramList            const& paramList,
                  clientTransactionPtr const& tranP) {

    string callXml;

    xml::generateCall(methodName, paramList, this->implP->dialect, &callXml);
    
    xml::trace("XML-RPC CALL", callXml);

    xmlTransaction_clientPtr const xmlTranP(tranP);

    this->implP->transportP->start(carriageParmP, callXml, xmlTranP);
}
 


void
client_xml::finishAsync(xmlrpc_c::timeout const timeout) {

    this->implP->transportP->finishAsync(timeout);
}



void
client_xml::setInterrupt(int * const interruptP) {

    this->implP->transportP->setInterrupt(interruptP);
}



serverAccessor::serverAccessor(clientPtr       const clientP,
                               carriageParmPtr const carriageParmP) :

    clientP(clientP), carriageParmP(carriageParmP) {};



void
serverAccessor::call(std::string            const& methodName,
                     xmlrpc_c::paramList    const& paramList,
                     xmlrpc_c::rpcOutcome * const  outcomeP) const {

    this->clientP->call(this->carriageParmP.get(),
                        methodName,
                        paramList,
                        outcomeP);
}



serverAccessorPtr::serverAccessorPtr() {
    // Base class constructor will construct pointer that points to nothing
}



serverAccessorPtr::serverAccessorPtr(
    serverAccessor * const serverAccessorParmP) :
    autoObjectPtr(serverAccessorParmP) {}



serverAccessor *
serverAccessorPtr::operator->() const {

    autoObject * const p(this->objectP);
    return dynamic_cast<serverAccessor *>(p);
}



serverAccessor *
serverAccessorPtr::get() const {
    return dynamic_cast<serverAccessor *>(objectP);
}



connection::connection(client *       const clientP,
                       carriageParm * const carriageParmP) :
    clientP(clientP), carriageParmP(carriageParmP) {}



connection::~connection() {}



struct rpc_impl {
    enum state {
        STATE_UNFINISHED,  // RPC is running or not started yet
        STATE_ERROR,       // We couldn't execute the RPC
        STATE_FAILED,      // RPC executed successfully, but failed per XML-RPC
        STATE_SUCCEEDED    // RPC is done, no exception
    };
    enum state state;
    girerr::error * errorP;     // Defined only in STATE_ERROR
    rpcOutcome outcome;
        // Defined only in STATE_FAILED and STATE_SUCCEEDED
    string methodName;
    xmlrpc_c::paramList paramList;

    rpc_impl(string              const& methodName,
             xmlrpc_c::paramList const& paramList) :
        state(STATE_UNFINISHED),
        methodName(methodName),
        paramList(paramList) {}
};



rpc::rpc(string    const  methodName,
         paramList const& paramList) {
    
    this->implP = new rpc_impl(methodName, paramList);
}



rpc::~rpc() {
    
    if (this->implP->state == rpc_impl::STATE_ERROR)
        delete(this->implP->errorP);

    delete(this->implP);
}



void
rpc::call(client       * const clientP,
          carriageParm * const carriageParmP) {

    if (this->implP->state != rpc_impl::STATE_UNFINISHED)
        throw(error("Attempt to execute an RPC that has already been "
                    "executed"));

    clientP->call(carriageParmP,
                  this->implP->methodName,
                  this->implP->paramList,
                  &this->implP->outcome);

    this->implP->state = this->implP->outcome.succeeded() ?
        rpc_impl::STATE_SUCCEEDED : rpc_impl::STATE_FAILED;
}



void
rpc::call(connection const& connection) {

    this->call(connection.clientP, connection.carriageParmP);

}


 
void
rpc::start(client       * const clientP,
           carriageParm * const carriageParmP) {
    
    if (this->implP->state != rpc_impl::STATE_UNFINISHED)
        throw(error("Attempt to execute an RPC that has already been "
                    "executed"));

    clientP->start(carriageParmP,
                   this->implP->methodName,
                   this->implP->paramList,
                   rpcPtr(this));
}


 
void
rpc::start(xmlrpc_c::connection const& connection) {
    
    this->start(connection.clientP, connection.carriageParmP);
}



void
rpc::finish(rpcOutcome const& outcome) {

    this->implP->state =
        outcome.succeeded() ?
        rpc_impl::STATE_SUCCEEDED : rpc_impl::STATE_FAILED;

    this->implP->outcome = outcome;

    this->notifyComplete();
}



void
rpc::finishErr(error const& error) {

    this->implP->state = rpc_impl::STATE_ERROR;
    this->implP->errorP = new girerr::error(error);
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

    switch (this->implP->state) {
    case rpc_impl::STATE_UNFINISHED:
        throw(error("Attempt to get result of RPC that is not finished."));
        break;
    case rpc_impl::STATE_ERROR:
        throw(*this->implP->errorP);
        break;
    case rpc_impl::STATE_FAILED:
        throw(error("RPC response indicates failure.  " +
                    this->implP->outcome.getFault().getDescription()));
        break;
    case rpc_impl::STATE_SUCCEEDED: {
        // All normal
    }
    }

    return this->implP->outcome.getResult();
}




fault
rpc::getFault() const {

    switch (this->implP->state) {
    case rpc_impl::STATE_UNFINISHED:
        throw(error("Attempt to get fault from RPC that is not finished"));
        break;
    case rpc_impl::STATE_ERROR:
        throw(*this->implP->errorP);
        break;
    case rpc_impl::STATE_SUCCEEDED:
        throw(error("Attempt to get fault from an RPC that succeeded"));
        break;
    case rpc_impl::STATE_FAILED: {
        // All normal
    }
    }

    return this->implP->outcome.getFault();
}



bool
rpc::isFinished() const {
    return (this->implP->state != rpc_impl::STATE_UNFINISHED);
}



bool
rpc::isSuccessful() const {
    return (this->implP->state == rpc_impl::STATE_SUCCEEDED);
}



rpcPtr::rpcPtr() {}



rpcPtr::rpcPtr(rpc * const rpcP) : clientTransactionPtr(rpcP) {}



rpcPtr::rpcPtr(string              const  methodName,
               xmlrpc_c::paramList const& paramList) :
                   clientTransactionPtr(new rpc(methodName, paramList)) {}



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
    } catch (error const& error) {
        this->tranP->finishErr(error);
    }
}



void
xmlTransaction_client::finishErr(error const& error) const {

    this->tranP->finishErr(error);
}



xmlTransaction_clientPtr::xmlTransaction_clientPtr() {}



xmlTransaction_clientPtr::xmlTransaction_clientPtr(
    clientTransactionPtr const& tranP) :
        xmlTransactionPtr(new xmlTransaction_client(tranP)) {}



xmlTransaction_client *
xmlTransaction_clientPtr::operator->() const {
    autoObject * const p(this->objectP);
    return dynamic_cast<xmlTransaction_client *>(p);
}



} // namespace
