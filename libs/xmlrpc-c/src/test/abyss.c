/* None of the tests in here rely on a client existing, or even a network
   connection.  We should figure out how to create a test client and do
   such tests.
*/

#include "unistdx.h"
#include <stdio.h>
#ifndef WIN32
#include <sys/socket.h>
#include <netinet/in.h>
#endif
#include <errno.h>
#include <string.h>

#include "xmlrpc_config.h"

#include "int.h"
#include "xmlrpc-c/base.h"
#include "xmlrpc-c/server.h"
#include "xmlrpc-c/abyss.h"

#include "test.h"

#include "abyss.h"



static void
bindSocketToPort(int      const fd,
                 uint16_t const portNumber) {

    struct sockaddr_in name;
    int rc;

    name.sin_family = AF_INET;
    name.sin_port   = htons(portNumber);
    name.sin_addr.s_addr = INADDR_ANY;

    rc = bind(fd, (struct sockaddr *)&name, sizeof(name));
    if (rc != 0)
        fprintf(stderr, "bind() of %d failed, errno=%d (%s)",
                fd, errno, strerror(errno));

    TEST(rc == 0);
}



static void
chanSwitchCreateFd(int            const fd,
                   TChanSwitch ** const chanSwitchPP,
                   const char **  const errorP) {

#ifdef WIN32
    ChanSwitchWinCreateWinsock(fd, chanSwitchPP, errorP);
#else
    ChanSwitchUnixCreateFd(fd, chanSwitchPP, errorP);
#endif
}    



static void
closesock(int const fd) {
#ifdef WIN32
    closesocket(fd);
#else
    close(fd);
#endif
}



static void
chanSwitchCreate(uint16_t       const portNumber,
                 TChanSwitch ** const chanSwitchPP,
                 const char **  const errorP) {

#ifdef WIN32
    ChanSwitchWinCreate(portNumber, chanSwitchPP, errorP);
#else
    ChanSwitchUnixCreate(portNumber, chanSwitchPP, errorP);
#endif
}    



static void
channelCreateFd(int const fd,
                TChannel ** const channelPP,
                const char ** const errorP) {

#ifdef WIN32
    struct abyss_win_chaninfo * channelInfoP;
    ChannelWinCreateWinsock(fd, channelPP, &channelInfoP, errorP);
#else
    struct abyss_unix_chaninfo * channelInfoP;
    ChannelUnixCreateFd(fd, channelPP, &channelInfoP, errorP);
#endif
}



static void
testChanSwitchOsSocket(void) {

    int rc;

    rc = socket(AF_INET, SOCK_STREAM, 0);
    if (rc < 0) {
        fprintf(stderr, "socket() failed with errno %d (%s)",
                errno, strerror(errno));
        abort();
    } else {
        int const fd = rc;

        TChanSwitch * chanSwitchP;
        TServer server;
        const char * error;

        bindSocketToPort(fd, 8080);

        chanSwitchCreateFd(fd, &chanSwitchP, &error);

        TEST_NULL_STRING(error);
        
        ServerCreateSwitch(&server, chanSwitchP, &error);
        
        TEST_NULL_STRING(error);
        
        ServerFree(&server);
        
        ChanSwitchDestroy(chanSwitchP);

        closesock(fd);
    }
}



static void
testChanSwitch(void) {

    TServer server;
    TChanSwitch * chanSwitchP;
    const char * error;

    chanSwitchCreate(8080, &chanSwitchP, &error);

    TEST_NULL_STRING(error);

    ServerCreateSwitch(&server, chanSwitchP, &error);

    TEST_NULL_STRING(error);

    ServerFree(&server);

    ChanSwitchDestroy(chanSwitchP);
    
    testChanSwitchOsSocket();
}



static void
testChannel(void) {

    int rc;

    rc = socket(AF_INET, SOCK_STREAM, 0);
    if (rc < 0) {
        fprintf(stderr, "socket() failed with errno %d (%s)",
                errno, strerror(errno));
        abort();
    } else {
        int const fd = rc;

        TChannel * channelP;
        const char * error;

        channelCreateFd(fd, &channelP, &error);

        TEST(error);

        TEST(strstr(error, "not in connected"));
    }
}



static void
testOsSocket(void) {

    int rc;

    rc = socket(AF_INET, SOCK_STREAM, 0);
    if (rc < 0) {
        fprintf(stderr, "socket() failed with errno %d (%s)",
                errno, strerror(errno));
        abort();
    } else {
        int const fd = rc;

        TServer server;
        abyss_bool success;

        bindSocketToPort(fd, 8080);

        success = ServerCreateSocket(&server, NULL, fd, NULL, NULL);

        TEST(success);

        ServerFree(&server);

        closesock(fd);
    }
}



static void
testSocket(void) {

#ifndef WIN32
    int rc;

    rc = socket(AF_INET, SOCK_STREAM, 0);
    if (rc < 0) {
        fprintf(stderr, "socket() failed with errno %d (%s)",
                errno, strerror(errno));
        abort();
    } else {
        int const fd = rc;

        TSocket * socketP;
        TServer server;
        const char * error;

        SocketUnixCreateFd(fd, &socketP);

        TEST(socketP != NULL);

        ServerCreateSocket2(&server, socketP, &error);

        TEST(!error);

        ServerFree(&server);

        SocketDestroy(socketP);

        close(fd);
    }
#endif
}



static void
testServerCreate(void) {

    TServer server;
    abyss_bool success;

    success = ServerCreate(&server, NULL, 8080, NULL, NULL);
    TEST(success);
    ServerInit(&server);
    ServerFree(&server);

    success = ServerCreate(&server, "myserver", 8080,
                           "/tmp/docroot", "/tmp/logfile");
    TEST(success);
    ServerInit(&server);
    ServerFree(&server);

    success = ServerCreateNoAccept(&server, NULL, NULL, NULL);
    TEST(success);
    ServerFree(&server);

    {
        TChanSwitch * chanSwitchP;
        const char * error;

        chanSwitchCreate(8080, &chanSwitchP, &error);
        
        TEST_NULL_STRING(error);

        ServerCreateSwitch(&server, chanSwitchP, &error);
        
        TEST_NULL_STRING(error);

        ServerSetName(&server, "/tmp/docroot");
        ServerSetLogFileName(&server, "/tmp/logfile");
        ServerSetKeepaliveTimeout(&server, 50);
        ServerSetKeepaliveMaxConn(&server, 5);
        ServerSetTimeout(&server, 75);
        ServerSetAdvertise(&server, 1);
        ServerSetAdvertise(&server, 0);

        ServerInit(&server);

        ServerFree(&server);

        ChanSwitchDestroy(chanSwitchP);
    }        
}



void
test_abyss(void) {

    const char * error;

    printf("Running Abyss server tests...\n");
    
    AbyssInit(&error);
    TEST_NULL_STRING(error);

    ChanSwitchInit(&error);
    TEST_NULL_STRING(error);

    ChannelInit(&error);
    TEST_NULL_STRING(error);

    testChanSwitch();

    testChannel();

    testOsSocket();

    testSocket();

    testServerCreate();

    ChannelTerm();
    ChanSwitchTerm();
    AbyssTerm();

    printf("\n");
    printf("Abyss server tests done.\n");
}

