/*=============================================================================
                                  server_pstream
===============================================================================
  Test the pstream server C++ facilities of XML-RPC for C/C++.
  
=============================================================================*/
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string>
#include <fcntl.h>

#include "xmlrpc-c/girerr.hpp"
using girerr::error;
using girerr::throwf;
#include "xmlrpc-c/base.hpp"
#include "xmlrpc-c/registry.hpp"
#include "xmlrpc-c/server_pstream.hpp"

#include "tools.hpp"
#include "server_pstream.hpp"

using namespace xmlrpc_c;
using namespace std;



class sampleAddMethod : public method {
public:
    sampleAddMethod() {
        this->_signature = "i:ii";
        this->_help = "This method adds two integers together";
    }
    void
    execute(xmlrpc_c::paramList const& paramList,
            value *             const  retvalP) {
        
        int const addend(paramList.getInt(0));
        int const adder(paramList.getInt(1));
        
        paramList.verifyEnd(2);
        
        *retvalP = value_int(addend + adder);
    }
};



static void
createTestFile(string const& contents,
               int *  const  fdP) {

    string const filename("/tmp/xmlrpc_test_pstream");
    unlink(filename.c_str());
    int rc;
    rc = open(filename.c_str(), O_RDWR | O_CREAT);
    unlink(filename.c_str());
    
    if (rc < 0)
        throwf("Failed to create file '%s' as a test tool.  errno=%d (%s)",
               filename.c_str(), errno, strerror(errno));
    else {
        int const fd(rc);

        int rc;
    
        rc = write(fd, contents.c_str(), contents.length());

        if (rc < 0)
            throwf("write() of test file failed, errno=%d (%s)",
                   errno, strerror(errno));
        else {
            unsigned int bytesWritten(rc);

            if (bytesWritten != contents.length())
                throwf("Short write");
            else {
                int rc;
                rc = lseek(fd, 0, SEEK_SET);
                
                if (rc < 0)
                    throwf("lseek(0) of test file failed, errno=%d (%s)",
                           errno, strerror(errno));
            }
        }
        *fdP = fd;
    }
}



class serverPstreamConnTestSuite : public testSuite {

public:
    virtual string suiteName() {
        return "serverPstreamConnTestSuite";
    }
    virtual void runtests(unsigned int const) {
        int const devNullFd(open("/dev/null", 0));

        if (devNullFd < 0)
            throwf("Failed to open /dev/null, needed for test.");

        registry myRegistry;
        
        myRegistry.addMethod("sample.add", methodPtr(new sampleAddMethod));

        registryPtr myRegistryP(new registry);

        myRegistryP->addMethod("sample.add", methodPtr(new sampleAddMethod));

        EXPECT_ERROR(  // Empty options
            serverPstreamConn::constrOpt opt;
            serverPstreamConn server(opt);
            );

        EXPECT_ERROR(  // No registry
            serverPstreamConn server(serverPstreamConn::constrOpt()
                                     .socketFd(3));
            );

        EXPECT_ERROR(  // No socket fd
            serverPstreamConn server(serverPstreamConn::constrOpt()
                                     .registryP(&myRegistry));
            );
        
        EXPECT_ERROR(  // No such file descriptor
            serverPstreamConn server(serverPstreamConn::constrOpt()
                                     .registryP(&myRegistry)
                                     .socketFd(37));
            );
        
        {
            serverPstreamConn server(serverPstreamConn::constrOpt()
                                     .registryP(&myRegistry)
                                     .socketFd(devNullFd));

            bool eof;
            server.runOnce(&eof);
            TEST(eof);
        }
        {
            int fd;
            createTestFile("junk", &fd);

            serverPstreamConn server(serverPstreamConn::constrOpt()
                                     .registryP(&myRegistry)
                                     .socketFd(fd));

            bool eof;

            EXPECT_ERROR(   // EOF in the middle of a packet
                server.runOnce(&eof);
                );
            close(fd);
        }

        close(devNullFd);
    }
};



string
serverPstreamTestSuite::suiteName() {
    return "serverPstreamTestSuite";
}


void
serverPstreamTestSuite::runtests(unsigned int const indentation) {

    serverPstreamConnTestSuite().run(indentation + 1);

}

