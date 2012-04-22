/*=============================================================================
                        asynch_client.cpp
===============================================================================
  This is an example of an XML-RPC client that uses XML-RPC for C/C++
  (Xmlrpc-c).

  In particular, it does multiple RPCs asynchronously, running
  simultaneously.
=============================================================================*/

#include <cassert>
#include <cstdlib>
#include <string>
#include <iostream>
#include <xmlrpc-c/girerr.hpp>
#include <xmlrpc-c/base.hpp>
#include <xmlrpc-c/client.hpp>

using namespace std;

int
main(int argc, char **) {

    if (argc-1 > 0) {
        cerr << "This program has no arguments" << endl;
        exit(1);
    }

    try {
        xmlrpc_c::clientXmlTransport_curl myTransport;

        xmlrpc_c::client_xml myClient(&myTransport);

        string const methodName("sample.add");

        xmlrpc_c::paramList sampleAddParms1;
        sampleAddParms1.add(xmlrpc_c::value_int(3));
        sampleAddParms1.add(xmlrpc_c::value_int(1));

        xmlrpc_c::rpcPtr rpc1P(methodName, sampleAddParms1);

        xmlrpc_c::paramList sampleAddParms2;
        sampleAddParms2.add(xmlrpc_c::value_int(5));
        sampleAddParms2.add(xmlrpc_c::value_int(7));

        xmlrpc_c::rpcPtr rpc2P(methodName, sampleAddParms2);

        string const serverUrl("http://localhost:8080/RPC2");

        xmlrpc_c::carriageParm_curl0 myCarriageParm(serverUrl);

        rpc1P->start(&myClient, &myCarriageParm);
        rpc2P->start(&myClient, &myCarriageParm);

        cout << "Two RPCs started.  Waiting for them to finish." << endl;

        myClient.finishAsync(xmlrpc_c::timeout());  // infinite timeout

        assert(rpc1P->isFinished());
        assert(rpc2P->isFinished());

        int const sum1(xmlrpc_c::value_int(rpc1P->getResult()));
        int const sum2(xmlrpc_c::value_int(rpc2P->getResult()));

        cout << "Result of RPC 1 (sum of 3 and 1): " << sum1 << endl;
        cout << "Result of RPC 2 (sum of 5 and 7): " << sum2 << endl;

    } catch (exception const& e) {
        cerr << "Client threw error: " << e.what() << endl;
    } catch (...) {
        cerr << "Client threw unexpected error." << endl;
    }

    return 0;
}
