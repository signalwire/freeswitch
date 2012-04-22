/*=============================================================================
                        sample_add_client_complex.cpp
===============================================================================
  This is an example of an XML-RPC client that uses XML-RPC for C/C++
  (Xmlrpc-c).

  In particular, it uses the complex lower-level interface that gives you
  lots of flexibility but requires lots of code.  Also see
  xmlrpc_sample_add_server, which does the same thing as this program,
  but with much simpler code because it uses a simpler facility of
  Xmlrpc-c.

  This program actually gains nothing from using the more difficult
  facility.  It is for demonstration purposes.
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
        xmlrpc_c::clientXmlTransport_curl myTransport(
            xmlrpc_c::clientXmlTransport_curl::constrOpt()
            .no_ssl_verifyhost(true)
            .user_agent("sample_add/1.0"));

        xmlrpc_c::client_xml myClient(&myTransport);

        string const methodName("sample.add");

        xmlrpc_c::paramList sampleAddParms;
        sampleAddParms.add(xmlrpc_c::value_int(5));
        sampleAddParms.add(xmlrpc_c::value_int(7));

        xmlrpc_c::rpcPtr myRpcP(methodName, sampleAddParms);

        string const serverUrl("http://localhost:8080/RPC2");

        xmlrpc_c::carriageParm_curl0 myCarriageParm(serverUrl);

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
