/*=============================================================================
                                  server_pstream
===============================================================================
  Test the pstream server C++ facilities of XML-RPC for C/C++.
  
=============================================================================*/

#include "xmlrpc_config.h"

#if MSVCRT
  #include <winsock2.h>
  #include <io.h>
#else
  #include <unistd.h>
  #include <sys/socket.h>
  #include <arpa/inet.h>
#endif

#include <errno.h>
#include <string>
#include <cstring>
#include <fcntl.h>

#include "xmlrpc-c/config.h"

#if MSVCRT
  int
  xmlrpc_win32_socketpair(int    const domain,
                          int    const type,
                          int    const protocol,
                          SOCKET       socks[2]);
#endif

#include "xmlrpc-c/girerr.hpp"
using girerr::error;
using girerr::throwf;
#include "xmlrpc-c/sleep_int.h"
#include "xmlrpc-c/base.hpp"
#include "xmlrpc-c/registry.hpp"
#include "xmlrpc-c/server_pstream.hpp"

#include "tools.hpp"
#include "server_pstream.hpp"

using namespace xmlrpc_c;
using namespace std;


namespace {

static void
setNonBlocking(XMLRPC_SOCKET const socket) {
    
#if MSVCRT
    u_long iMode = 1;
    ioctlsocket(socket, FIONBIO, &iMode);
#else
    fcntl(socket, F_SETFL, O_NONBLOCK);
#endif
}



#define ESC_STR "\x1B"


static string const
xmlPrologue("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n");

static string const
packetStart(ESC_STR "PKT");

static string const
packetEnd(ESC_STR "END");


class callInfo_test : public callInfo {
public:
    callInfo_test() : info("this is a test") {}
    string const info;
};



class sampleAddMethod : public method {
public:
    sampleAddMethod() {
        this->_signature = "i:ii";
        this->_help = "This method adds two integers together";
    }
    void
    execute(xmlrpc_c::paramList const& paramList,
            value *             const  retvalP) {
        
        int const addend(paramList.getInt(0));
        int const adder(paramList.getInt(1));
        
        paramList.verifyEnd(2);
        
        *retvalP = value_int(addend + adder);
    }
};

string const sampleAddCallXml(
    xmlPrologue +
    "<methodCall>\r\n"
    "<methodName>sample.add</methodName>\r\n"
    "<params>\r\n"
    "<param><value><i4>5</i4></value></param>\r\n"
    "<param><value><i4>7</i4></value></param>\r\n"
    "</params>\r\n"
    "</methodCall>\r\n"
    );
    
string const sampleAddResponseXml(
    xmlPrologue +
    "<methodResponse>\r\n"
    "<params>\r\n"
    "<param><value><i4>12</i4></value></param>\r\n"
    "</params>\r\n"
    "</methodResponse>\r\n"
    );


class testCallInfoMethod : public method2 {

public:
    virtual void
    execute(paramList        const& paramList,
            const callInfo * const  callInfoPtr,
            value *          const  retvalP) {

        const callInfo_test * const callInfoP(
            dynamic_cast<const callInfo_test *>(callInfoPtr));

        TEST(callInfoP != NULL);
        
        paramList.verifyEnd(0);

        TEST(callInfoP->info == string("this is a test"));
        
        *retvalP = value_nil();
    }
};

string const testCallInfoCallXml(
    xmlPrologue +
    "<methodCall>\r\n"
    "<methodName>test.callinfo</methodName>\r\n"
    "<params>\r\n"
    "</params>\r\n"
    "</methodCall>\r\n"
    );

string const testCallInfoResponseXml(
    xmlPrologue +
    "<methodResponse>\r\n"
    "<params>\r\n"
    "<param><value><nil/></value>"
    "</param>\r\n"
    "</params>\r\n"
    "</methodResponse>\r\n"
    );



static void
waitForNetworkTransport() {
/*----------------------------------------------------------------------------
   Wait for a message to travel through the network.

   This is part of our hack to allow us to test client/server communication
   without the bother of a separate thread for each.  One party writes
   to a socket, causing the OS to buffer the message, then the other party
   reads from the socket, getting the buffered message.  We never wait
   to send or receive, because with only one thread to do both, we would
   deadlock.  Instead, we just count on the buffer being big enough.

   But on some systems, the message doesn't immediately travel like this.  It
   takes action by an independent thread (provided by the OS) to move the
   message.  In particular, we've seen this behavior on Windows (2010.10).

   So we just sleep for a small amount of time to let the message move.
-----------------------------------------------------------------------------*/

    // xmlrpc_millisecond_sleep() is allowed to return early, and on Windows
    // it does that in preference to returning late insofar as the clock
    // resolution doesn't allow returning at the exact time.  It is rumored
    // that Windows clock period may be as long as 40 milliseconds.

    xmlrpc_millisecond_sleep(50);
}



class client {
/*----------------------------------------------------------------------------
   This is an object you can use as a client to test a packet stream
   server.

   You attach the 'serverFd' member to your packet stream server, then
   call the 'sendCall' method to send a call to your server, then call
   the 'recvResp' method to get the response.

   Destroying the object closes the connection.

   We rely on typical, though unguaranteed socket function: we need to
   be able to write 'contents' to the socket in a single write()
   system call before the other side reads anything -- i.e. the socket
   has to have a buffer that big.  We do this because we're lazy; doing
   it right would require forking a writer process.
-----------------------------------------------------------------------------*/
public:

    client();
    
    ~client();

    void
    sendCall(string const& callBytes) const;

    void
    hangup();

    void
    recvResp(string * const respBytesP) const;

    int serverFd;

private:

    int clientFd;
};



client::client() {

    enum {
        SERVER = 0,
        CLIENT = 1,
    };
    XMLRPC_SOCKET sockets[2];
    int rc;

    rc = XMLRPC_SOCKETPAIR(AF_UNIX, SOCK_STREAM, 0, sockets);

    if (rc < 0)
        throwf("Failed to create UNIX domain stream socket pair "
               "as test tool.  errno=%d (%s)",
               errno, strerror(errno));
    else {
        setNonBlocking(sockets[CLIENT]);

        this->serverFd = sockets[SERVER];
        this->clientFd = sockets[CLIENT];
    }
}



client::~client() {

    XMLRPC_CLOSESOCKET(this->clientFd);
    XMLRPC_CLOSESOCKET(this->serverFd);
}



void
client::sendCall(string const& packetBytes) const {

    int rc;

    rc = send(this->clientFd, packetBytes.c_str(), packetBytes.length(), 0);

    waitForNetworkTransport();

    if (rc < 0)
        throwf("send() of test data to socket failed, errno=%d (%s)",
               errno, strerror(errno));
    else {
        unsigned int bytesWritten(rc);

        if (bytesWritten != packetBytes.length())
            throwf("Short write to socket");
    }
}



void
client::hangup() {

    // Closing the socket (close()) would be a better simulation of the
    // real world, and easier, but we shut down just the client->server
    // half of the socket and remain open to receive an RPC response.
    // That's because this test program is lazy and does the client and
    // server in the same thread, depending on socket buffering on the
    // receive side to provide parallelism.  We need to be able to do the
    // following sequence:
    //
    //   - Client sends call
    //   - Client hangs up
    //   - Server gets call
    //   - Server sends response
    //   - Client gets response
    //   - Server notices hangup

    shutdown(this->clientFd, 1);  // Shutdown for transmission only
}



void
client::recvResp(string * const packetBytesP) const {

    char buffer[4096];
    int rc;

    waitForNetworkTransport();

    rc = recv(this->clientFd, buffer, sizeof(buffer), 0);

    if (rc < 0)
        throwf("recv() from socket failed, errno=%d (%s)",
               errno, strerror(errno));
    else {
        unsigned int bytesReceived(rc);

        *packetBytesP = string(buffer, bytesReceived);
    }
}



static void
testEmptyStream(registry const& myRegistry) {
/*----------------------------------------------------------------------------
   Here we send the pstream server an empty stream; i.e. we close the
   socket from the client end without sending anything.

   This should cause the server to recognize EOF.
-----------------------------------------------------------------------------*/

    client client;

    serverPstreamConn server(serverPstreamConn::constrOpt()
                             .registryP(&myRegistry)
                             .socketFd(client.serverFd));

    client.hangup();

    bool eof;
    server.runOnce(&eof);

    TEST(eof);
}



static void
testBrokenPacket(registry const& myRegistry) {
/*----------------------------------------------------------------------------
   Here we send a stream that is not a legal packetsocket stream: it
   doesn't have any control word.
-----------------------------------------------------------------------------*/
    client client;

    serverPstreamConn server(serverPstreamConn::constrOpt()
                             .registryP(&myRegistry)
                             .socketFd(client.serverFd));

    client.sendCall("junk");
    client.hangup();

    bool eof;

    EXPECT_ERROR(
        server.runOnce(&eof);
        );
}



static void
testEmptyPacket(registry const& myRegistry) {
/*----------------------------------------------------------------------------
   Here we send the pstream server one empty packet.  It should respond
   with one packet, being an XML-RPC fault response complaining that the
   call is not valid XML.
-----------------------------------------------------------------------------*/
    client client;

    serverPstreamConn server(serverPstreamConn::constrOpt()
                             .registryP(&myRegistry)
                             .socketFd(client.serverFd));

    client.sendCall(packetStart + packetEnd);

    bool eof;
    server.runOnce(&eof);

    TEST(!eof);

    string response;
    client.recvResp(&response);

    // We ought to validate that the response is a complaint about
    // the empty call

    client.hangup();

    server.runOnce(&eof);

    TEST(eof);
}



static void
testCallInfo(client *            const  clientP,
             serverPstreamConn * const  serverP) {
    
    string const testCallInfoCallStream(
        packetStart + testCallInfoCallXml + packetEnd
        );

    string const testCallInfoResponseStream(
        packetStart + testCallInfoResponseXml + packetEnd
        );

    clientP->sendCall(testCallInfoCallStream);
    
    callInfo_test callInfo;
    int nointerrupt(0);
    bool eof;
    serverP->runOnce(&callInfo, &nointerrupt, &eof);

    TEST(!eof);

    string response;
    clientP->recvResp(&response);

    TEST(response == testCallInfoResponseStream);
}



static void
testNormalCall(registry const& myRegistry) {

    string const sampleAddGoodCallStream(
        packetStart + sampleAddCallXml + packetEnd
        );

    string const sampleAddGoodResponseStream(
        packetStart + sampleAddResponseXml + packetEnd
        );

    client client;

    serverPstreamConn server(serverPstreamConn::constrOpt()
                             .registryP(&myRegistry)
                             .socketFd(client.serverFd));

    client.sendCall(sampleAddGoodCallStream);

    bool eof;

    int interrupt(1);
    server.runOnce(&interrupt, &eof); // returns without reading socket
    TEST(!eof);

    server.runOnce(&eof);

    TEST(!eof);

    string response;
    client.recvResp(&response);

    TEST(response == sampleAddGoodResponseStream);
    
    testCallInfo(&client, &server);

    client.hangup();

    server.runOnce(&eof);

    TEST(eof);
}



static void
testNoWaitCall(registry const& myRegistry) {

    string const sampleAddGoodCallStream(
        packetStart +
        xmlPrologue +
        "<methodCall>\r\n"
        "<methodName>sample.add</methodName>\r\n"
        "<params>\r\n"
        "<param><value><i4>5</i4></value></param>\r\n"
        "<param><value><i4>7</i4></value></param>\r\n"
        "</params>\r\n"
        "</methodCall>\r\n" +
        packetEnd
        );
    

    string const sampleAddGoodResponseStream(
        packetStart +
        xmlPrologue +
        "<methodResponse>\r\n"
        "<params>\r\n"
        "<param><value><i4>12</i4></value></param>\r\n"
        "</params>\r\n"
        "</methodResponse>\r\n" +
        packetEnd
        );

    client client;

    serverPstreamConn server(serverPstreamConn::constrOpt()
                             .registryP(&myRegistry)
                             .socketFd(client.serverFd));

    bool eof;
    bool gotOne;
    string response;

    server.runOnceNoWait(&eof, &gotOne);

    TEST(!eof);
    TEST(!gotOne);

    server.runOnceNoWait(&eof);

    TEST(!eof);

    client.sendCall(sampleAddGoodCallStream);

    server.runOnceNoWait(&eof, &gotOne);

    TEST(!eof);
    TEST(gotOne);

    client.recvResp(&response);

    TEST(response == sampleAddGoodResponseStream);
    
    client.sendCall(sampleAddGoodCallStream);

    server.runOnce(&eof);

    TEST(!eof);
    client.recvResp(&response);
    TEST(response == sampleAddGoodResponseStream);

    client.hangup();

    server.runOnce(&eof);

    TEST(eof);
}



static void
testMultiRpcRunNoRpc(registry const& myRegistry) {

    client client;

    serverPstreamConn server(serverPstreamConn::constrOpt()
                             .registryP(&myRegistry)
                             .socketFd(client.serverFd));

    client.hangup();

    server.run();
}



static void
testMultiRpcRunOneRpc(registry const& myRegistry) {

    string const sampleAddGoodCallStream(
        packetStart +
        xmlPrologue +
        "<methodCall>\r\n"
        "<methodName>sample.add</methodName>\r\n"
        "<params>\r\n"
        "<param><value><i4>5</i4></value></param>\r\n"
        "<param><value><i4>7</i4></value></param>\r\n"
        "</params>\r\n"
        "</methodCall>\r\n" +
        packetEnd
        );
    

    string const sampleAddGoodResponseStream(
        packetStart +
        xmlPrologue +
        "<methodResponse>\r\n"
        "<params>\r\n"
        "<param><value><i4>12</i4></value></param>\r\n"
        "</params>\r\n"
        "</methodResponse>\r\n" +
        packetEnd
        );

    client client;

    serverPstreamConn server(serverPstreamConn::constrOpt()
                             .registryP(&myRegistry)
                             .socketFd(client.serverFd));


    client.sendCall(sampleAddGoodCallStream);
    client.hangup();

    int interrupt;

    interrupt = 1;
    server.run(&interrupt);  // Returns without reading socket

    interrupt = 0;
    server.run(&interrupt);  // Does the buffered RPC

    string response;
    client.recvResp(&response);

    TEST(response == sampleAddGoodResponseStream);
}



class serverPstreamConnTestSuite : public testSuite {

public:
    virtual string suiteName() {
        return "serverPstreamConnTestSuite";
    }
    virtual void runtests(unsigned int const) {
        registry myRegistry;
        
        myRegistry.addMethod("sample.add",
                             methodPtr(new sampleAddMethod));
        myRegistry.addMethod("test.callinfo",
                             methodPtr(new testCallInfoMethod));

        registryPtr myRegistryP(new registry);

        myRegistryP->addMethod("sample.add", methodPtr(new sampleAddMethod));

        EXPECT_ERROR(  // Empty options
            serverPstreamConn::constrOpt opt;
            serverPstreamConn server(opt);
            );

        EXPECT_ERROR(  // No registry
            serverPstreamConn server(serverPstreamConn::constrOpt()
                                     .socketFd(3));
            );

        EXPECT_ERROR(  // No socket fd
            serverPstreamConn server(serverPstreamConn::constrOpt()
                                     .registryP(&myRegistry));
            );
        
        testEmptyStream(myRegistry);

        testBrokenPacket(myRegistry);

        testEmptyPacket(myRegistry);

        testNormalCall(myRegistry);

        testNoWaitCall(myRegistry);

        testMultiRpcRunNoRpc(myRegistry);

        testMultiRpcRunOneRpc(myRegistry);
    }
};



static void
testMultiConnInterrupt(registry const& myRegistry) {

    // We use a nonexistent file descriptor, but the server won't
    // ever access it, so it won't know.

    serverPstream server(serverPstream::constrOpt()
                         .registryP(&myRegistry)
                         .socketFd(37));

    int interrupt(1);  // interrupt immediately

    server.runSerial(&interrupt);
}



class derivedServer : public xmlrpc_c::serverPstream {
public:
    derivedServer(serverPstream::constrOpt const& constrOpt) :
        serverPstream(constrOpt),
        info("this is my derived server") {}

    string const info;
};



class multiTestCallInfoMethod : public method2 {

// The test isn't sophisticated enough actually to do an RPC, so this
// code never runs.  We just want to see if it compiles.

public:
    virtual void
    execute(paramList        const& paramList,
            const callInfo * const  callInfoPtr,
            value *          const  retvalP) {

        const callInfo_serverPstream * const callInfoP(
            dynamic_cast<const callInfo_serverPstream *>(callInfoPtr));

        TEST(callInfoP != NULL);
        
        paramList.verifyEnd(0);

        derivedServer * const derivedServerP(
            dynamic_cast<derivedServer *>(callInfoP->serverP));

        TEST(derivedServerP->info == string("this is my derived server"));

        TEST(callInfoP->clientAddr.sa_family == AF_INET);
        TEST(callInfoP->clientAddrSize >= sizeof(struct sockaddr_in));
        
        *retvalP = value_nil();
    }
};

static void
testMultiConnCallInfo() {

    registry myRegistry;
        
    myRegistry.addMethod("testCallInfo",
                         methodPtr(new multiTestCallInfoMethod));

    derivedServer server(serverPstream::constrOpt()
                         .registryP(&myRegistry)
                         .socketFd(37));
}



class multiConnServerTestSuite : public testSuite {

public:
    virtual string suiteName() {
        return "multiConnServerTestSuite";
    }
    virtual void runtests(unsigned int const) {
        registry myRegistry;
        
        myRegistry.addMethod("sample.add",
                             methodPtr(new sampleAddMethod));

        registryPtr myRegistryP(new registry);

        myRegistryP->addMethod("sample.add", methodPtr(new sampleAddMethod));

        EXPECT_ERROR(  // Empty options
            serverPstream::constrOpt opt;
            serverPstream server(opt);
            );

        EXPECT_ERROR(  // No registry
            serverPstream server(serverPstream::constrOpt()
                                 .socketFd(3));
            );

        EXPECT_ERROR(  // No socket fd
            serverPstream server(serverPstream::constrOpt()
                                 .registryP(&myRegistry));
            );
        
        testMultiConnInterrupt(myRegistry);

        testMultiConnCallInfo();
    }
};



} // unnamed namespace



string
serverPstreamTestSuite::suiteName() {
    return "serverPstreamTestSuite";
}


void
serverPstreamTestSuite::runtests(unsigned int const indentation) {

    serverPstreamConnTestSuite().run(indentation + 1);

    multiConnServerTestSuite().run(indentation + 1);
}

