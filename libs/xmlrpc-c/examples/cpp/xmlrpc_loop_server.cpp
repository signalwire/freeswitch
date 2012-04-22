/* A simple standalone XML-RPC server based on Abyss that contains a
   simple one-thread request processing loop.  

   xmlrpc_sample_add_server.cpp is a server that does the same thing, but
   does it by running a full Abyss daemon in the background, so it has
   less control over how the requests are served.
*/

#include <cassert>
#include <iostream>

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
        this->_signature = "i:ii";  // method's arguments, result are integers
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

    try {
        xmlrpc_c::registry myRegistry;

        xmlrpc_c::methodPtr const sampleAddMethodP(new sampleAddMethod);

        myRegistry.addMethod("sample.add", sampleAddMethodP);

        xmlrpc_c::serverAbyss myAbyssServer(
            xmlrpc_c::serverAbyss::constrOpt()
            .registryP(&myRegistry)
            .portNumber(8080)
            .logFileName("/tmp/xmlrpc_log"));

        while (true) {
            cout << "Waiting for next RPC..." << endl;

            myAbyssServer.runOnce();
            /* This waits for the next connection, accepts it, reads the
               HTTP POST request, executes the indicated RPC, and closes
               the connection.
            */
        }
    } catch (exception const& e) {
        cerr << "Something failed.  " << e.what() << endl;
    }
    return 0;
}
