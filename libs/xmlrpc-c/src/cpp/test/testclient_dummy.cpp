/*=============================================================================
                                  testclient_dummy
===============================================================================
  This is a substitute for testclient.cpp, for use in a test program that is
  not linked with the client libraries.

  It simply passes the test.
=============================================================================*/

#include <string>
#include <iostream>

#include "tools.hpp"
#include "testclient.hpp"

using namespace std;

string
clientTestSuite::suiteName() {
    return "clientTestSuite";
}


void
clientTestSuite::runtests(unsigned int const indentation) {

    cout << string((indentation+1)*2, ' ') 
         << "Running dummy test." << endl;
}
