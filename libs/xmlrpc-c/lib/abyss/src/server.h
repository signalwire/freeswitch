#ifndef SERVER_H_INCLUDED
#define SERVER_H_INCLUDED

#include <sys/types.h>

#include "bool.h"
#include "xmlrpc-c/abyss.h"

#include "data.h"

struct TFile;
struct abyss_mutex;

struct _TServer {
    bool terminationRequested;
        /* User wants this server to terminate as soon as possible,
           in particular before accepting any more connections and without
           waiting for any.
        */
    bool chanSwitchBound;
        /* The channel switch exists and is bound to a local address
           (may already be listening as well)
        */
    TChanSwitch * chanSwitchP;
        /* Meaningful only when 'chanSwitchBound' is true: the channel
           switch which directs connections from clients to this server.
        */
    bool weCreatedChanSwitch;
        /* We created the channel switch 'chanSwitchP', as
           opposed to 1) User supplied it; or 2) there isn't one.
        */
    const char * logfilename;
    bool logfileisopen;
    struct TFile * logfileP;
    struct abyss_mutex * logmutexP;
    const char * name;
    bool serverAcceptsConnections;
        /* We listen for and accept TCP connections for HTTP transactions.
           (The alternative is the user supplies a TCP-connected socket
           for each transaction)
        */
    uint16_t port;
        /* Meaningful only when 'chanSwitchBound' is false: TCP port
           number to which we should bind the switch.
        */
    uint32_t keepalivetimeout;
    uint32_t keepalivemaxconn;
    uint32_t timeout;
        /* Maximum time in seconds the server will wait to read a header
           or a data chunk from the channel.
        */
    TList handlers;
        /* Ordered list of HTTP request handlers.  For each HTTP request,
           Server calls each one in order until one reports that it handled
           it.

           Each item in the list of of type 'URIHandler2'.
        */
    URIHandler defaultHandler;
        /* The handler for HTTP requests that aren't claimed by any handler
           in the list 'handlers'.  This can't be null; if user doesn't
           supply anything better, it is a built-in basic web server
           handler.  */
    void * defaultHandlerContext;
        /* This is opaque data to be given to the default handler when it
           requests it.
        */
    void * builtinHandlerP;
    bool advertise;
    bool useSigchld;
        /* Meaningless if not using forking for threads.
           TRUE means user will call ServerHandleSigchld to indicate that
           a SIGCHLD signal was received, and server shall use that as its
           indication that a child has died.  FALSE means server will not
           be aware of SIGCHLD and will instead poll for existence of PIDs
           to determine if a child has died.
        */
#ifndef WIN32
    uid_t uid;
    gid_t gid;
    struct TFile * pidfileP;
#endif
};

#endif
