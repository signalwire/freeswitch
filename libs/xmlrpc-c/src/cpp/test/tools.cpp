#include <string>
#include <sstream>
#include <iostream>
#include "xmlrpc-c/girerr.hpp"
using girerr::error;
using girerr::throwf;

#include "tools.hpp"

using namespace std;

testSuite::~testSuite() {
}



void
testSuite::run(unsigned int const indentation) {
    try {
        cout << string(indentation*2, ' ') 
             << "Running " << suiteName() << endl;
        this->runtests(indentation);
    } catch (error const& error) {
        throwf("%s failed.  %s", suiteName().c_str(), error.what());
    } catch (...) {
        throw(error(suiteName() + string(" failed.  ") +
                    string("It threw an unexpected type of object")));
    }
    cout << string(indentation*2, ' ') 
         << suiteName() << " tests passed." << endl;
}



// This is a good place to set a breakpoint.
void 
logFailedTest(const char * const fileName, 
              unsigned int const lineNum, 
              const char * const statement) {

    ostringstream msg;

    msg << endl
        << fileName << ":" << lineNum 
        << ": expected (" << statement << ")" << endl;

    throw(error(msg.str()));
}


error
fileLineError(string       const filename,
              unsigned int const lineNumber,
              string       const description) {
    
    ostringstream combined;
    
    combined << filename << ":" << lineNumber << " " << description;
    
    return error(combined.str());
}



