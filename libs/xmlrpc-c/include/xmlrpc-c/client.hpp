#ifndef CLIENT_HPP_INCLUDED
#define CLIENT_HPP_INCLUDED

#include <string>

#include <xmlrpc-c/girerr.hpp>
#include <xmlrpc-c/girmem.hpp>
#include <xmlrpc-c/base.hpp>
#include <xmlrpc-c/timeout.hpp>
#include <xmlrpc-c/client.h>

namespace xmlrpc_c {


class carriageParm {
/*----------------------------------------------------------------------------
   The parameter to a client for an individual RPC.  It tells specifics
   of how to carry the call to the server and the response back.  For
   example, it may identify the server.  It may identify communication
   protocols to use.  It may indicate permission and accounting
   information.

   This is a base class; the carriage parameter is specific to the
   class of client.  For example, an HTTP-based client would have a
   URL and HTTP basic authentication info as parameter.
-----------------------------------------------------------------------------*/
protected:
    virtual ~carriageParm();
    carriageParm();
};

class clientTransactionPtr;

class clientTransaction : public girmem::autoObject {

    friend class clientTransactionPtr;

public:
    virtual void
    finish(xmlrpc_c::rpcOutcome const& outcome) = 0;
    
    virtual void
    finishErr(girerr::error const& error) = 0;

protected:
    clientTransaction();
};

class clientTransactionPtr : public girmem::autoObjectPtr {
    
public:
    clientTransactionPtr();
    virtual ~clientTransactionPtr();

    virtual xmlrpc_c::clientTransaction *
    operator->() const;
};

class client {
/*----------------------------------------------------------------------------
   A generic client -- a means of performing an RPC.  This is so generic
   that it can be used for clients that are not XML-RPC.

   This is a base class.  Derived classes define things such as that
   XML and HTTP get used to perform the RPC.
-----------------------------------------------------------------------------*/
public:
    virtual ~client();

    virtual void
    call(carriageParm *         const  carriageParmP,
         std::string            const  methodName,
         xmlrpc_c::paramList    const& paramList,
         xmlrpc_c::rpcOutcome * const  outcomeP) = 0;

    virtual void
    start(xmlrpc_c::carriageParm *       const  carriageParmP,
          std::string                    const  methodName,
          xmlrpc_c::paramList            const& paramList,
          xmlrpc_c::clientTransactionPtr const& tranP);
};

class connection {
/*----------------------------------------------------------------------------
   A nexus of a particular client and a particular server, along with
   carriage parameters for performing RPCs between the two.

   This is a minor convenience for client programs that always talk to
   the same server the same way.

   Use this as a parameter to rpc.call().
-----------------------------------------------------------------------------*/
public:
    connection(xmlrpc_c::client *       const clientP,
               xmlrpc_c::carriageParm * const carriageParmP);

    ~connection();

    xmlrpc_c::client *       clientP;
    xmlrpc_c::carriageParm * carriageParmP;
};

class carriageParm_http0 : public carriageParm {

public:
    carriageParm_http0(std::string const serverUrl);

    ~carriageParm_http0();

    void
    setBasicAuth(std::string const userid,
                 std::string const password);

    xmlrpc_server_info * c_serverInfoP;

protected:
    // Only a derived class is allowed to create an object with no
    // server URL, and the derived class expected to follow it up
    // with an instantiate() to establish the server URL.

    carriageParm_http0();

    void
    instantiate(std::string const serverUrl);
};

class carriageParm_curl0 : public xmlrpc_c::carriageParm_http0 {

public:
    carriageParm_curl0(std::string const serverUrl);

};

class carriageParm_libwww0 : public xmlrpc_c::carriageParm_http0 {

public:
    carriageParm_libwww0(std::string const serverUrl);

};

class carriageParm_wininet0 : public xmlrpc_c::carriageParm_http0 {

public:
    carriageParm_wininet0(std::string const serverUrl);

};

class xmlTransactionPtr;

class xmlTransaction : public girmem::autoObject {

    friend class xmlTransactionPtr;

public:
    virtual void
    finish(std::string const& responseXml) const;

    virtual void
    finishErr(girerr::error const& error) const;

protected:
    xmlTransaction();
};

class xmlTransactionPtr : public girmem::autoObjectPtr {
public:
    xmlTransactionPtr();

    xmlrpc_c::xmlTransaction *
    operator->() const;
};

class clientXmlTransport {
/*----------------------------------------------------------------------------
   An object which transports XML to and from an XML-RPC server for an
   XML-RPC client.

   This is a base class.  Derived classes define methods to perform the
   transportation in particular ways.
-----------------------------------------------------------------------------*/
public:
    virtual ~clientXmlTransport();

    virtual void
    call(xmlrpc_c::carriageParm * const  carriageParmP,
         std::string              const& callXml,
         std::string *            const  responseXmlP) = 0;

    virtual void
    start(xmlrpc_c::carriageParm *    const  carriageParmP,
          std::string                 const& callXml,
          xmlrpc_c::xmlTransactionPtr const& xmlTranP);

    virtual void
    finishAsync(xmlrpc_c::timeout const timeout);

    static void
    asyncComplete(
        struct xmlrpc_call_info * const callInfoP,
        xmlrpc_mem_block *        const responseXmlMP,
        xmlrpc_env                const transportEnv);
};

class clientXmlTransport_http : public xmlrpc_c::clientXmlTransport {
/*----------------------------------------------------------------------------
   A base class for client XML transports that use the simple, classic
   C HTTP transports.
-----------------------------------------------------------------------------*/
public:
    virtual ~clientXmlTransport_http();
    
    void
    call(xmlrpc_c::carriageParm * const  carriageParmP,
         std::string              const& callXml,
         std::string *            const  responseXmlP);
    
    void
    start(xmlrpc_c::carriageParm *    const  carriageParmP,
          std::string                 const& callXml,
          xmlrpc_c::xmlTransactionPtr const& xmlTranP);
        
    virtual void
    finishAsync(xmlrpc_c::timeout const timeout);

protected:
    clientXmlTransport_http() {} // ensure no one can create
    struct xmlrpc_client_transport *           c_transportP;
    const struct xmlrpc_client_transport_ops * c_transportOpsP;
};

class clientXmlTransport_curl : public xmlrpc_c::clientXmlTransport_http {

public:
    clientXmlTransport_curl(std::string const networkInterface = "",
                            bool        const noSslVerifyPeer = false,
                            bool        const noSslVerifyHost = false,
                            std::string const userAgent = "");

    ~clientXmlTransport_curl();
};

class clientXmlTransport_libwww : public xmlrpc_c::clientXmlTransport_http {
    
public:
    clientXmlTransport_libwww(std::string const appname = "",
                              std::string const appversion = "");

    ~clientXmlTransport_libwww();
};

class clientXmlTransport_wininet : public xmlrpc_c::clientXmlTransport_http {

public:
    clientXmlTransport_wininet(bool const allowInvalidSslCerts = false);

    ~clientXmlTransport_wininet();
};

class client_xml : public xmlrpc_c::client {
/*----------------------------------------------------------------------------
   A client that uses XML-RPC XML in the RPC.  This class does not define
   how the XML gets transported, though (i.e. does not require HTTP).
-----------------------------------------------------------------------------*/
public:
    client_xml(xmlrpc_c::clientXmlTransport * const transportP);

    void
    call(carriageParm *         const  carriageParmP,
         std::string            const  methodName,
         xmlrpc_c::paramList    const& paramList,
         xmlrpc_c::rpcOutcome * const  outcomeP);

    void
    start(xmlrpc_c::carriageParm *       const  carriageParmP,
          std::string                    const  methodName,
          xmlrpc_c::paramList            const& paramList,
          xmlrpc_c::clientTransactionPtr const& tranP);

    void
    finishAsync(xmlrpc_c::timeout const timeout);

private:
    xmlrpc_c::clientXmlTransport * transportP;
};

class xmlTransaction_client : public xmlrpc_c::xmlTransaction {

public:
    xmlTransaction_client(xmlrpc_c::clientTransactionPtr const& tranP);

    void
    finish(std::string const& responseXml) const;

    void
    finishErr(girerr::error const& error) const;
private:
    xmlrpc_c::clientTransactionPtr const tranP;
};

class xmlTransaction_clientPtr : public xmlTransactionPtr {
public:
    xmlTransaction_clientPtr();
    
    xmlTransaction_clientPtr(xmlrpc_c::clientTransactionPtr const& tranP);

    xmlrpc_c::xmlTransaction_client *
    operator->() const;
};

class rpcPtr;

class rpc : public clientTransaction {
/*----------------------------------------------------------------------------
   An RPC.  An RPC consists of method name, parameters, and result.  It
   does not specify in any way how the method name and parameters get
   turned into a result.  It does not presume XML or HTTP.

   You don't create an object of this class directly.  All references to
   an rpc object should be by an rpcPtr object.  Create a new RPC by
   creating a new rpcPtr.  Accordingly, our constructors and destructors
   are protected, but available to our friend class rpcPtr.

   In order to do asynchronous RPCs, you normally have to create a derived
   class that defines a useful notifyComplete().  If you do that, you'll
   want to make sure the derived class objects get accessed only via rpcPtrs
   as well.
-----------------------------------------------------------------------------*/
    friend class xmlrpc_c::rpcPtr;

public:
    void
    call(xmlrpc_c::client       * const clientP,
         xmlrpc_c::carriageParm * const carriageParmP);

    void
    call(xmlrpc_c::connection const& connection);

    void
    start(xmlrpc_c::client       * const clientP,
          xmlrpc_c::carriageParm * const carriageParmP);
    
    void
    start(xmlrpc_c::connection const& connection);
    
    void
    finish(xmlrpc_c::rpcOutcome const& outcome);

    void
    finishErr(girerr::error const& error);

    virtual void
    notifyComplete();

    bool
    isFinished() const;

    bool
    isSuccessful() const;

    xmlrpc_c::value
    getResult() const;

    xmlrpc_c::fault
    getFault() const;

protected:
    rpc(std::string         const  methodName,
        xmlrpc_c::paramList const& paramList);

    virtual ~rpc();

private:
    enum state {
        STATE_UNFINISHED,  // RPC is running or not started yet
        STATE_ERROR,       // We couldn't execute the RPC
        STATE_FAILED,      // RPC executed successfully, but failed per XML-RPC
        STATE_SUCCEEDED    // RPC is done, no exception
    };
    enum state state;
    girerr::error * errorP;     // Defined only in STATE_ERROR
    xmlrpc_c::rpcOutcome outcome;
        // Defined only in STATE_FAILED and STATE_SUCCEEDED
    std::string methodName;
    xmlrpc_c::paramList paramList;
};

class rpcPtr : public clientTransactionPtr {
public:
    rpcPtr();

    rpcPtr(xmlrpc_c::rpc * const rpcP);

    rpcPtr(std::string         const  methodName,
           xmlrpc_c::paramList const& paramList);

    xmlrpc_c::rpc *
    operator->() const;
};

} // namespace
#endif
