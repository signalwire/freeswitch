#include <cstdlib>
#include <string>
#include <memory>
#include <signal.h>
#include <errno.h>
#include <iostream>
#ifndef _WIN32
#include <sys/wait.h>
#endif

#include "assertx.hpp"
#include "xmlrpc-c/string_int.h"
#include "xmlrpc-c/girerr.hpp"
using girerr::error;
using girerr::throwf;
#include "xmlrpc-c/base.h"
#include "xmlrpc-c/base.hpp"
#include "xmlrpc-c/server_abyss.h"
#include "xmlrpc-c/registry.hpp"
#include "xmlrpc-c/server_abyss.hpp"

using namespace std;
using namespace xmlrpc_c;

namespace xmlrpc_c {

namespace {


static void 
sigterm(int const signalClass) {

    cerr << "Signal of Class " << signalClass << " received.  Exiting" << endl;

    exit(1);
}



static void 
sigchld(int const ASSERT_ONLY_ARG(signalClass)) {
/*----------------------------------------------------------------------------
   This is a signal handler for a SIGCHLD signal (which informs us that
   one of our child processes has terminated).

   We respond by reaping the zombie process.

   Implementation note: In some systems, just setting the signal handler
   to SIG_IGN (ignore signal) does this.  In some, the system does this
   automatically if the signal is blocked.
-----------------------------------------------------------------------------*/
#ifndef _WIN32
    /* Reap zombie children until there aren't any more. */

    bool zombiesExist;
    bool error;

    assert(signalClass == SIGCHLD);
    
    zombiesExist = true;  // initial assumption
    error = false;  // no error yet
    while (zombiesExist && !error) {
        int status;
        pid_t const pid = waitpid((pid_t) -1, &status, WNOHANG);
    
        if (pid == 0)
            zombiesExist = false;
        else if (pid < 0) {
            /* because of ptrace */
            if (errno == EINTR) {
                // This is OK - it's a ptrace notification
            } else
                error = true;
        }
    }
#endif /* _WIN32 */
}



struct signalHandlers {
#ifndef WIN32
    struct sigaction term;
    struct sigaction int_;
    struct sigaction hup;
    struct sigaction usr1;
    struct sigaction pipe;
    struct sigaction chld;
#else
    int dummy;
#endif
};



void
setupSignalHandlers(struct signalHandlers * const oldHandlersP) {
#ifndef _WIN32
    struct sigaction mysigaction;
   
    sigemptyset(&mysigaction.sa_mask);
    mysigaction.sa_flags = 0;

    /* These signals abort the program, with tracing */
    mysigaction.sa_handler = sigterm;
    sigaction(SIGTERM, &mysigaction, &oldHandlersP->term);
    sigaction(SIGINT,  &mysigaction, &oldHandlersP->int_);
    sigaction(SIGHUP,  &mysigaction, &oldHandlersP->hup);
    sigaction(SIGUSR1, &mysigaction, &oldHandlersP->usr1);

    /* This signal indicates connection closed in the middle */
    mysigaction.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &mysigaction, &oldHandlersP->pipe);
   
    /* This signal indicates a child process (request handler) has died */
    mysigaction.sa_handler = sigchld;
    sigaction(SIGCHLD, &mysigaction, &oldHandlersP->chld);
#endif
}    



void
restoreSignalHandlers(struct signalHandlers const& oldHandlers) {

#ifndef _WIN32
    sigaction(SIGCHLD, &oldHandlers.chld, NULL);
    sigaction(SIGPIPE, &oldHandlers.pipe, NULL);
    sigaction(SIGUSR1, &oldHandlers.usr1, NULL);
    sigaction(SIGHUP,  &oldHandlers.hup,  NULL);
    sigaction(SIGINT,  &oldHandlers.int_, NULL);
    sigaction(SIGTERM, &oldHandlers.term, NULL);
#endif
}



// We need 'global' because methods of class serverAbyss call
// functions in the Abyss C library.  By virtue of global's static
// storage class, the program loader will call its constructor and
// destructor and thus initialize and terminate the Abyss C library.

class abyssGlobalState {
public:
    abyssGlobalState() {
        const char * error;
        AbyssInit(&error);
        if (error) {
            string const e(error);
            xmlrpc_strfree(error);
            throwf("AbyssInit() failed.  %s", e.c_str());
        }
    }
    ~abyssGlobalState() {
        AbyssTerm();
    }
} const global;

} // namespace



serverAbyss::shutdown::shutdown(serverAbyss * const serverAbyssP) :
    serverAbyssP(serverAbyssP) {}



serverAbyss::shutdown::~shutdown() {}



void
serverAbyss::shutdown::doit(string const&,
                            void * const) const {

    this->serverAbyssP->terminate();
}



serverAbyss::constrOpt::constrOpt() {
    present.registryPtr      = false;
    present.registryP        = false;
    present.socketFd         = false;
    present.portNumber       = false;
    present.logFileName      = false;
    present.keepaliveTimeout = false;
    present.keepaliveMaxConn = false;
    present.timeout          = false;
    present.dontAdvertise    = false;
    present.uriPath          = false;
    present.chunkResponse    = false;
    
    // Set default values
    value.dontAdvertise  = false;
    value.uriPath        = string("/RPC2");
    value.chunkResponse  = false;
}



#define DEFINE_OPTION_SETTER(OPTION_NAME, TYPE) \
serverAbyss::constrOpt & \
serverAbyss::constrOpt::OPTION_NAME(TYPE const& arg) { \
    this->value.OPTION_NAME = arg; \
    this->present.OPTION_NAME = true; \
    return *this; \
}

DEFINE_OPTION_SETTER(registryPtr,      xmlrpc_c::registryPtr);
DEFINE_OPTION_SETTER(registryP,        const registry *);
DEFINE_OPTION_SETTER(socketFd,         XMLRPC_SOCKET);
DEFINE_OPTION_SETTER(portNumber,       unsigned int);
DEFINE_OPTION_SETTER(logFileName,      string);
DEFINE_OPTION_SETTER(keepaliveTimeout, unsigned int);
DEFINE_OPTION_SETTER(keepaliveMaxConn, unsigned int);
DEFINE_OPTION_SETTER(timeout,          unsigned int);
DEFINE_OPTION_SETTER(dontAdvertise,    bool);
DEFINE_OPTION_SETTER(uriPath,          string);
DEFINE_OPTION_SETTER(chunkResponse,    bool);



void
serverAbyss::setAdditionalServerParms(constrOpt const& opt) {

    /* The following ought to be parameters on ServerCreate(), but it
       looks like plugging them straight into the TServer structure is
       the only way to set them.  
    */

    if (opt.present.keepaliveTimeout)
        ServerSetKeepaliveTimeout(&this->cServer, opt.value.keepaliveTimeout);
    if (opt.present.keepaliveMaxConn)
        ServerSetKeepaliveMaxConn(&this->cServer, opt.value.keepaliveMaxConn);
    if (opt.present.timeout)
        ServerSetTimeout(&this->cServer, opt.value.timeout);
    ServerSetAdvertise(&this->cServer, !opt.value.dontAdvertise);
}



static void
createServer(bool         const  logFileNameGiven,
             string       const& logFileName,
             bool         const  socketFdGiven,
             int          const  socketFd,
             bool         const  portNumberGiven,
             unsigned int const  portNumber,
             TServer *    const  srvPP) {
             
    const char * const logfileArg(logFileNameGiven ? 
                                  logFileName.c_str() : NULL);

    const char * const serverName("XmlRpcServer");

    abyss_bool created;
        
    if (socketFdGiven)
        created =
            ServerCreateSocket(srvPP, serverName, socketFd,
                               DEFAULT_DOCS, logfileArg);
    else if (portNumberGiven) {
        if (portNumber > 0xffff)
            throwf("Port number %u exceeds the maximum possible port number "
                   "(65535)", portNumber);

        created =
            ServerCreate(srvPP, serverName, portNumber,
                         DEFAULT_DOCS, logfileArg);
    } else
        created = 
            ServerCreateNoAccept(srvPP, serverName,
                                 DEFAULT_DOCS, logfileArg);

    if (!created)
        throw(error("Failed to create Abyss server.  See Abyss error log for "
                    "reason."));
}



void
serverAbyss::initialize(constrOpt const& opt) {

    const registry * registryP;

    if (!opt.present.registryP && !opt.present.registryPtr)
        throwf("You must specify the 'registryP' or 'registryPtr' option");
    else if (opt.present.registryP && opt.present.registryPtr)
        throwf("You may not specify both the 'registryP' and "
               "the 'registryPtr' options");
    else {
        if (opt.present.registryP)
            registryP = opt.value.registryP;
        else {
            this->registryPtr = opt.value.registryPtr;
            registryP = this->registryPtr.get();
        }
    }
    if (opt.present.portNumber && opt.present.socketFd)
        throwf("You can't specify both portNumber and socketFd options");

    DateInit();
    
    createServer(opt.present.logFileName, opt.value.logFileName,
                 opt.present.socketFd,    opt.value.socketFd,
                 opt.present.portNumber,  opt.value.portNumber,
                 &this->cServer);

    try {
        setAdditionalServerParms(opt);

        // chunked response implementation is incomplete.  We must
        // eventually get away from libxmlrpc_server_abyss and
        // register our own handler with the Abyss server.  At that
        // time, we'll have some place to pass
        // opt.value.chunkResponse.
        
        xmlrpc_c::server_abyss_set_handlers(&this->cServer,
                                            registryP,
                                            opt.value.uriPath);
        
        if (opt.present.portNumber || opt.present.socketFd)
            ServerInit(&this->cServer);
    } catch (...) {
        ServerFree(&this->cServer);
        throw;
    }
}



serverAbyss::serverAbyss(constrOpt const& opt) {

    initialize(opt);
}



serverAbyss::serverAbyss(
    xmlrpc_c::registry const& registry,
    unsigned int       const  portNumber,
    string             const& logFileName,
    unsigned int       const  keepaliveTimeout,
    unsigned int       const  keepaliveMaxConn,
    unsigned int       const  timeout,
    bool               const  dontAdvertise,
    bool               const  socketBound,
    XMLRPC_SOCKET      const  socketFd) {
/*----------------------------------------------------------------------------
  This is a backward compatibility interface.  This used to be the only
  constructor.
-----------------------------------------------------------------------------*/
    serverAbyss::constrOpt opt;

    opt.registryP(&registry);
    if (logFileName.length() > 0)
        opt.logFileName(logFileName);
    if (keepaliveTimeout > 0)
        opt.keepaliveTimeout(keepaliveTimeout);
    if (keepaliveMaxConn > 0)
        opt.keepaliveMaxConn(keepaliveMaxConn);
    if (timeout > 0)
        opt.timeout(timeout);
    opt.dontAdvertise(dontAdvertise);
    if (socketBound)
        opt.socketFd(socketFd);
    else
        opt.portNumber(portNumber);

    initialize(opt);
}



serverAbyss::~serverAbyss() {

    ServerFree(&this->cServer);
}



void
serverAbyss::run() {

    /* We do some pretty ugly stuff for an object method: we set signal
       handlers, which are process-global.

       One example of where this can be hairy is: Caller has a child
       process unrelated to the Abyss server.  That child dies.  We
       get his death of a child signal and Caller never knows.

       We really expect to be the only thing in the process, at least
       for the time we're running.  If you want the Abyss Server
       to behave more like an object and own the signals yourself,
       use runOnce() in a loop instead of run().
    */
    signalHandlers oldHandlers;

    setupSignalHandlers(&oldHandlers);

    ServerRun(&this->cServer);

    restoreSignalHandlers(oldHandlers);
}
 


void
serverAbyss::runOnce() {

    ServerRunOnce(&this->cServer);
}



void
serverAbyss::runConn(int const socketFd) {

    ServerRunConn(&this->cServer, socketFd);
}



void
serverAbyss::terminate() {

    ServerTerminate(&this->cServer);
}

void
server_abyss_set_handlers(TServer * const  srvP,
                          registry  const& registry,
                          string    const& uriPath) {

    xmlrpc_server_abyss_set_handlers2(srvP,
                                      uriPath.c_str(),
                                      registry.c_registry());
}



void
server_abyss_set_handlers(TServer *        const  srvP,
                          const registry * const  registryP,
                          string           const& uriPath) {

    xmlrpc_server_abyss_set_handlers2(srvP,
                                      uriPath.c_str(),
                                      registryP->c_registry());
}



void
server_abyss_set_handlers(TServer *   const  srvP,
                          registryPtr const  registryPtr,
                          string      const& uriPath) {

    xmlrpc_server_abyss_set_handlers2(srvP,
                                      uriPath.c_str(),
                                      registryPtr->c_registry());
}



} // namespace
