#include <cassert>
#include <string>
#include <memory>
#include <signal.h>
#include <sys/wait.h>

#include "xmlrpc-c/girerr.hpp"
using girerr::error;
#include "xmlrpc-c/base.h"
#include "xmlrpc-c/base.hpp"
#include "xmlrpc-c/server_abyss.h"
#include "xmlrpc-c/registry.hpp"
#include "xmlrpc-c/server_abyss.hpp"

using namespace std;
using namespace xmlrpc_c;

namespace xmlrpc_c {

serverAbyss::serverAbyss(
    xmlrpc_c::registry const& registry,
    unsigned int       const  portNumber,
    string             const& logFileName,
    unsigned int       const  keepaliveTimeout,
    unsigned int       const  keepaliveMaxConn,
    unsigned int       const  timeout,
    bool               const  dontAdvertise) {
   
    this->registryP = &registry;
    this->logFileName = logFileName;
    this->keepaliveTimeout = keepaliveTimeout;
    this->keepaliveMaxConn = keepaliveMaxConn;
    this->timeout = timeout;
    this->dontAdvertise = dontAdvertise;
    
    if (portNumber > 0xffff)
        throw(error("Port number exceeds the maximum possible port number "
                    "(65535)"));
    else {
        this->portNumber = portNumber;
    }
}


serverAbyss::~serverAbyss() {
}



namespace {


static void 
sigterm(int const sig) {
    TraceExit("Signal %d received. Exiting...\n",sig);
}


static void 
sigchld(int const sig) {
/*----------------------------------------------------------------------------
   This is a signal handler for a SIGCHLD signal (which informs us that
   one of our child processes has terminated).

   We respond by reaping the zombie process.

   Implementation note: In some systems, just setting the signal handler
   to SIG_IGN (ignore signal) does this.  In others, it doesn't.
-----------------------------------------------------------------------------*/
#ifndef _WIN32
    /* Reap zombie children until there aren't any more. */

    bool zombiesExist;
    bool error;

    assert(sig == SIGCHLD);
    
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

void
setupSignalHandlers(void) {
#ifndef _WIN32
    struct sigaction mysigaction;
   
    sigemptyset(&mysigaction.sa_mask);
    mysigaction.sa_flags = 0;

    /* These signals abort the program, with tracing */
    mysigaction.sa_handler = sigterm;
    sigaction(SIGTERM, &mysigaction, NULL);
    sigaction(SIGINT,  &mysigaction, NULL);
    sigaction(SIGHUP,  &mysigaction, NULL);
    sigaction(SIGUSR1, &mysigaction, NULL);

    /* This signal indicates connection closed in the middle */
    mysigaction.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &mysigaction, NULL);
   
    /* This signal indicates a child process (request handler) has died */
    mysigaction.sa_handler = sigchld;
    sigaction(SIGCHLD, &mysigaction, NULL);
#endif
}    


static void
setAdditionalServerParms(
    unsigned int       const keepaliveTimeout,
    unsigned int       const keepaliveMaxConn,
    unsigned int       const timeout,
    bool               const dontAdvertise,
    TServer *          const srvP) {

    /* The following ought to be parameters on ServerCreate(), but it
       looks like plugging them straight into the TServer structure is
       the only way to set them.  
    */

    if (keepaliveTimeout > 0)
        srvP->keepalivetimeout = keepaliveTimeout;
    if (keepaliveMaxConn > 0)
        srvP->keepalivemaxconn = keepaliveMaxConn;
    if (timeout > 0)
        srvP->timeout = timeout;
    srvP->advertise = !dontAdvertise;
}


} // namespace



void
serverAbyss::run() {

    xmlrpc_env env;
    xmlrpc_env_init(&env);
    
    DateInit();
    MIMETypeInit();
    
    TServer srv;

    const char * const logfileArg(this->logFileName.length() == 0 ?
                                  NULL : this->logFileName.c_str());

    ServerCreate(&srv, "XmlRpcServer", this->portNumber,
                 DEFAULT_DOCS, logfileArg);

    setAdditionalServerParms(
        this->keepaliveTimeout, this->keepaliveMaxConn, this->timeout,
        this->dontAdvertise, &srv);
                             
    xmlrpc_c::server_abyss_set_handlers(&srv, *this->registryP);
    
    ServerInit(&srv);
    
    setupSignalHandlers();
       
    ServerRun(&srv);

    /* We can't exist here because ServerRun doesn't return */
    assert(false);
}
 


void
server_abyss_set_handlers(TServer *          const  srvP,
                          xmlrpc_c::registry const& registry) {
    

    xmlrpc_server_abyss_set_handlers(srvP, registry.c_registry());
}



} // namespace

