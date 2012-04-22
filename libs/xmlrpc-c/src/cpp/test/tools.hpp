#ifndef TEST_HPP_INCLUDED
#define TEST_HPP_INCLUDED

#include <string>

#include "xmlrpc-c/girerr.hpp"
using girerr::error;

class testSuite {
/*----------------------------------------------------------------------------
   This is a base class for a test suite.  Give the suite a name
   (to be used in messages about it) and some test code via
   virtual methods suiteName() and runtests(), respectively.

   runtests() should throw either an 'error' object or an 'XmlRpcFault'
   object if the test fails.  It should throw something else if the
   test can't run.  It should throw nothing if the tests pass.

   You don't normally keep an object of this class around.  You don't
   even give it a name.  You simply refer to a literal object, like so:

     myTestSuite().run(0)
-----------------------------------------------------------------------------*/
public:
    virtual ~testSuite();

    void run(unsigned int const indentation);

    virtual void runtests(unsigned int const) {
        throw(error("test suite does not have a runtests() method"));
    };
    virtual std::string suiteName() {
        return "unnamed test suite";
    }
};



void 
logFailedTest(const char * const fileName, 
              unsigned int const lineNum, 
              const char * const statement);

error
fileLineError(std::string  const filename,
              unsigned int const lineNumber,
              std::string  const description);

#define TEST(statement) \
    do { \
        if (!(statement)) \
            logFailedTest(__FILE__, __LINE__, #statement); \
    } while (0)


#define TEST_PASSED() \
    do { } while (0)

#define TEST_FAILED(reason) \
    do { \
        logFailedTest(__FILE__, __LINE__, (reason)); \
    } while (0)

#define EXPECT_ERROR(statement) \
    do { try { statement } catch (error) {break;} \
      throw(fileLineError(__FILE__, __LINE__, "Expected error; didn't get one")); \
    } while (0)

#define trickToStraightenOutEmacsIndentation \
;

#endif
