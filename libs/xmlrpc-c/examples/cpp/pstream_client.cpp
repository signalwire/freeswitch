/*=============================================================================
                        pstream_client.cpp
===============================================================================
  This is an example of a client that uses XML-RPC for C/C++
  (Xmlrpc-c).

  In particular, it uses the simple "packet stream" XML transport mechanism
  instead of HTTP as specified by XML-RPC (so this is not an XML-RPC
  client).

  You have to supply as Standard Input a stream (TCP) socket whose other
  end is hooked up to the RPC server.  The 'socket_exec' program is a
  good way to arrange that.

  The sample program pstream_server.cpp is compatible with this client.

  Example:

    $ socketexec -connect -remote_host=localhost -remote_port=8080 \
        ./pstream_client
=============================================================================*/

#include <cassert>
#include <cstdlib>
#include <string>
#include <iostream>
#include <unistd.h>
#include <sys/signal.h>
#include <xmlrpc-c/girerr.hpp>
#include <xmlrpc-c/base.hpp>
#include <xmlrpc-c/client.hpp>
#include <xmlrpc-c/client_transport.hpp>


using namespace std;

int
main(int argc, char **) {

    if (argc-1 > 0) {
        cerr << "This program has no arguments" << endl;
        exit(1);
    }

    // It's a good idea to disable SIGPIPE signals; if server closes his end
    // of the pipe/socket, we'd rather see a failure to send a call than
    // get killed by the OS.
    signal(SIGPIPE, SIG_IGN);

    try {
        xmlrpc_c::clientXmlTransport_pstream myTransport(
            xmlrpc_c::clientXmlTransport_pstream::constrOpt()
            .fd(STDIN_FILENO));

        xmlrpc_c::client_xml myClient(&myTransport);

        string const methodName("sample.add");

        xmlrpc_c::paramList sampleAddParms;
        sampleAddParms.add(xmlrpc_c::value_int(5));
        sampleAddParms.add(xmlrpc_c::value_int(7));

        xmlrpc_c::rpcPtr myRpcP(methodName, sampleAddParms);

        xmlrpc_c::carriageParm_pstream myCarriageParm;
            // Empty; transport doesn't need any information

        myRpcP->call(&myClient, &myCarriageParm);

        assert(myRpcP->isFinished());

        int const sum(xmlrpc_c::value_int(myRpcP->getResult()));
            // Assume the method returned an integer; throws error if not

        cout << "Result of RPC (sum of 5 and 7): " << sum << endl;

    } catch (exception const& e) {
        cerr << "Client threw error: " << e.what() << endl;
    } catch (...) {
        cerr << "Client threw unexpected error." << endl;
    }

    return 0;
}
