// A simple standalone XML-RPC server written in C++.
//
// This server returns to the caller his IP address and port number,
// as a demonstration of how to access such information.
//
// This works only on Unix (to wit, something that uses Abyss's
// ChanSwitchUnix channel switch to accept TCP connections from clients).
//
// See xmlrpc_sample_add_server.cpp for a more basic example.
//
//    To run this:
//
//       $ ./callinfo_abyss_server &
//       $ xmlrpc localhost:8080 getCallInfo

#include <cassert>
#include <stdexcept>
#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>

#include <xmlrpc-c/base.hpp>
#include <xmlrpc-c/registry.hpp>
#include <xmlrpc-c/server_abyss.hpp>
#include <xmlrpc-c/abyss.h>

using namespace std;


struct tcpPortAddr {
    unsigned char  ipAddr[4];
    unsigned short portNumber;
};


static struct tcpPortAddr
tcpAddrFromSockAddr(struct sockaddr const sockAddr) {

    const struct sockaddr_in * const sockAddrInP(
        static_cast<struct sockaddr_in *>((void *)&sockAddr));

    const unsigned char * const ipAddr(
        static_cast<const unsigned char *>(
            (const void *)&sockAddrInP->sin_addr.s_addr)
        );   // 4 byte array

    assert(sockAddrInP->sin_family == AF_INET);

    struct tcpPortAddr retval;

    retval.ipAddr[0] = ipAddr[0];
    retval.ipAddr[1] = ipAddr[1];
    retval.ipAddr[2] = ipAddr[2];
    retval.ipAddr[3] = ipAddr[3];
    retval.portNumber = ntohs(sockAddrInP->sin_port);

    return retval;
}



static std::string
rpcIpAddrMsg(xmlrpc_c::callInfo_serverAbyss const& callInfo) {

    void * chanInfoPtr;
    SessionGetChannelInfo(callInfo.abyssSessionP, &chanInfoPtr);

    struct abyss_unix_chaninfo * const chanInfoP(
        static_cast<struct abyss_unix_chaninfo *>(chanInfoPtr));

    struct tcpPortAddr const tcpAddr(tcpAddrFromSockAddr(chanInfoP->peerAddr));

    char msg[128];

    sprintf(msg, "RPC is from IP address %u.%u.%u.%u, Port %hu",
            tcpAddr.ipAddr[0],
            tcpAddr.ipAddr[1],
            tcpAddr.ipAddr[2],
            tcpAddr.ipAddr[3],
            tcpAddr.portNumber);

    return std::string(msg);
}



class getCallInfoMethod : public xmlrpc_c::method2 {
public:
    void
    execute(xmlrpc_c::paramList        const& paramList,
            const xmlrpc_c::callInfo * const  callInfoPtr,
            xmlrpc_c::value *          const  retvalP) {

        const xmlrpc_c::callInfo_serverAbyss * const callInfoP(
            dynamic_cast<const xmlrpc_c::callInfo_serverAbyss *>(callInfoPtr));
        
        paramList.verifyEnd(0);

        // Because this gets called via a xmlrpc_c::serverAbyss:
        assert(callInfoP != NULL);

        *retvalP = xmlrpc_c::value_string(rpcIpAddrMsg(*callInfoP));
    }
};



int 
main(int           const, 
     const char ** const) {

    try {
        xmlrpc_c::registry myRegistry;

        xmlrpc_c::methodPtr const getCallInfoMethodP(new getCallInfoMethod);

        myRegistry.addMethod("getCallInfo", getCallInfoMethodP);
        
        xmlrpc_c::serverAbyss myAbyssServer(xmlrpc_c::serverAbyss::constrOpt()
                                            .registryP(&myRegistry)
                                            .portNumber(8080)
                                            );
        
        myAbyssServer.run();
        // xmlrpc_c::serverAbyss.run() never returns
        assert(false);
    } catch (exception const& e) {
        cerr << "Something failed.  " << e.what() << endl;
    }
    return 0;
}
