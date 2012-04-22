#ifndef CLIENT_HPP_INCLUDED
#define CLIENT_HPP_INCLUDED

#include <string>
#include <vector>
#include <memory>

#include <xmlrpc-c/girerr.hpp>
#include <xmlrpc-c/girmem.hpp>
#include <xmlrpc-c/base.hpp>
#include <xmlrpc-c/timeout.hpp>
#include <xmlrpc-c/client_transport.hpp>

namespace xmlrpc_c {

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

    clientTransactionPtr(clientTransaction * const transP);

    virtual ~clientTransactionPtr();

    virtual xmlrpc_c::clientTransaction *
    operator->() const;
};

class clientPtr;

class client : public girmem::autoObject {
/*----------------------------------------------------------------------------
   A generic client -- a means of performing an RPC.  This is so generic
   that it can be used for clients that are not XML-RPC.

   This is a base class.  Derived classes define things such as that
   XML and HTTP get used to perform the RPC.
-----------------------------------------------------------------------------*/
    friend class clientTransactionPtr;

public:
    virtual ~client();

    virtual void
    call(xmlrpc_c::carriageParm * const  carriageParmP,
         std::string              const& methodName,
         xmlrpc_c::paramList      const& paramList,
         xmlrpc_c::rpcOutcome *   const  outcomeP) = 0;

    virtual void
    start(xmlrpc_c::carriageParm *       const  carriageParmP,
          std::string                    const& methodName,
          xmlrpc_c::paramList            const& paramList,
          xmlrpc_c::clientTransactionPtr const& tranP);

    void
    finishAsync(xmlrpc_c::timeout const timeout);
    
    virtual void
    setInterrupt(int *);
};

class clientPtr : public girmem::autoObjectPtr {
public:
    clientPtr();

    explicit clientPtr(xmlrpc_c::client * const clientP);

    xmlrpc_c::client *
    operator->() const;

    xmlrpc_c::client *
    get() const;
};

class serverAccessor : public girmem::autoObject {
    
public:
    serverAccessor(xmlrpc_c::clientPtr       const clientP,
                   xmlrpc_c::carriageParmPtr const carriageParmP);

    void
    call(std::string            const& methodName,
         xmlrpc_c::paramList    const& paramList,
         xmlrpc_c::rpcOutcome * const  outcomeP) const;

private:
    xmlrpc_c::clientPtr       const clientP;
    xmlrpc_c::carriageParmPtr const carriageParmP;
};

class serverAccessorPtr : public girmem::autoObjectPtr {
public:
    serverAccessorPtr();

    explicit
    serverAccessorPtr(xmlrpc_c::serverAccessor * const serverAccessorP);

    xmlrpc_c::serverAccessor *
    operator->() const;

    xmlrpc_c::serverAccessor *
    get() const;
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

class client_xml : public xmlrpc_c::client {
/*----------------------------------------------------------------------------
   A client that uses XML-RPC XML in the RPC.  This class does not define
   how the XML gets transported, though (i.e. does not require HTTP).
-----------------------------------------------------------------------------*/
public:
    client_xml(xmlrpc_c::clientXmlTransport * const transportP);

    client_xml(xmlrpc_c::clientXmlTransport * const transportP,
               xmlrpc_dialect                 const dialect);

    client_xml(xmlrpc_c::clientXmlTransportPtr const transportP);

    client_xml(xmlrpc_c::clientXmlTransportPtr const transportP,
               xmlrpc_dialect                  const dialect);

    ~client_xml();

    void
    call(carriageParm *         const  carriageParmP,
         std::string            const& methodName,
         xmlrpc_c::paramList    const& paramList,
         xmlrpc_c::rpcOutcome * const  outcomeP);

    void
    start(xmlrpc_c::carriageParm *       const  carriageParmP,
          std::string                    const& methodName,
          xmlrpc_c::paramList            const& paramList,
          xmlrpc_c::clientTransactionPtr const& tranP);

    void
    finishAsync(xmlrpc_c::timeout const timeout);

    virtual void
    setInterrupt(int * interruptP);

private:
    struct client_xml_impl * implP;
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

    rpc(std::string         const  methodName,
        xmlrpc_c::paramList const& paramList);

    virtual ~rpc();

private:
    struct rpc_impl * implP;
};

class rpcPtr : public clientTransactionPtr {
public:
    rpcPtr();

    explicit rpcPtr(xmlrpc_c::rpc * const rpcP);

    rpcPtr(std::string         const  methodName,
           xmlrpc_c::paramList const& paramList);

    xmlrpc_c::rpc *
    operator->() const;
};

} // namespace
#endif
