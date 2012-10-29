/* A simple standalone RPC server based on an Xmlrpc-c packet socket.

   This program expects the invoker to provide an established connection
   to a client as Standard Input (E.g. Inetd can do this).  It processes
   RPCs from that connection until the client closes the connection.

   This is not an XML-RPC server, because it uses a simple packet socket
   instead of HTTP.  See xmlrpc_sample_add_server.cpp for an example of
   an XML-RPC server.

   The advantage of this example over XML-RPC is that it has a connection
   concept.  The client can be connected indefinitely and the server gets
   notified when the client terminates, even if it gets aborted by its OS.

   Here's an example of running this:

     $ socketexec -accept -local_port=8080 ./pstream_inetd_server
*/

#ifndef WIN32
#include <unistd.h>
#endif
#include <cassert>
#include <iostream>
#include <signal.h>

#include <xmlrpc-c/base.hpp>
#include <xmlrpc-c/registry.hpp>
#include <xmlrpc-c/server_pstream.hpp>

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

    // It's a good idea to disable SIGPIPE signals; if client closes his end
    // of the pipe/socket, we'd rather see a failure to send a response than
    // get killed by the OS.
    signal(SIGPIPE, SIG_IGN);

    try {
        xmlrpc_c::registry myRegistry;

        xmlrpc_c::methodPtr const sampleAddMethodP(new sampleAddMethod);

        myRegistry.addMethod("sample.add", sampleAddMethodP);

        xmlrpc_c::serverPstreamConn server(
            xmlrpc_c::serverPstreamConn::constrOpt()
            .socketFd(STDIN_FILENO)
            .registryP(&myRegistry));

        server.run();

    } catch (exception const& e) {
        cerr << "Something threw an error: " << e.what() << endl;
    }
    return 0;
}
