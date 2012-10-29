/* A simple XML-RPC server that runs under Inetd.  I.e. it lets the invoking
   program handle all the connection switching and simply processes one
   RPC on the provided connection (Standard Input) and exits.

   A typical example of where this would be useful is with an Inetd
   "super server."

   xmlrpc_sample_add_server.cpp is a server that does the same thing,
   but you give it a TCP port number and it listens for TCP connections
   and processes RPCs ad infinitum.  xmlrpc_socket_server.c is halfway
   in between those -- you give it an already bound and listening
   socket, and it listens for TCP connections and processes RPCs ad
   infinitum.

   Here is an easy way to test this program:

     socketexec --accept --local_port=8080 --stdin -- ./xmlrpc_inetd_server

   Now run the client program 'xmlrpc_sample_add_client'.  Socketexec
   will accept the connection that the client program requests and pass it
   to this program on Standard Input.  This program will perform the RPC,
   respond to the client, then exit.
*/

#ifndef WIN32
#include <unistd.h>
#endif
#include <cassert>

#include <xmlrpc-c/base.hpp>
#include <xmlrpc-c/registry.hpp>
#include <xmlrpc-c/server_abyss.hpp>

using namespace std;

class sampleAddMethod : public xmlrpc_c::method {
public:
    sampleAddMethod() {
        // signature and help strings are documentation -- the client
        // can query this information with a system.methodSignature and
        // system.methodHelp RPC.
        this->_signature = "i:ii";  // method's arguments are two integers
        this->_help = "This method adds two integers together";
    }
    void
    execute(xmlrpc_c::paramList const& paramList,
            xmlrpc_c::value *   const  retvalP) {
        
        int const addend(paramList.getInt(0));
        int const adder(paramList.getInt(1));
        
        paramList.verifyEnd(2);
        
        *retvalP = xmlrpc_c::value_int(addend + adder);
    }
};



int 
main(int           const, 
     const char ** const) {

    xmlrpc_c::registry myRegistry;

    xmlrpc_c::methodPtr const sampleAddMethodP(new sampleAddMethod);

    myRegistry.addMethod("sample.add", sampleAddMethodP);

    xmlrpc_c::serverAbyss myAbyssServer(
         xmlrpc_c::serverAbyss::constrOpt()
         .registryP(&myRegistry));

    myAbyssServer.runConn(STDIN_FILENO);
        /* This reads the HTTP POST request from Standard Input and
           executes the indicated RPC.
        */
    return 0;
}
