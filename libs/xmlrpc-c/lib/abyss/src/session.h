#ifndef SESSION_H_INCLUDED
#define SESSION_H_INCLUDED

#include "xmlrpc-c/abyss.h"
#include "bool.h"
#include "date.h"
#include "data.h"

typedef struct {
    uint8_t major;
    uint8_t minor;
} httpVersion;

struct _TSession {
    bool validRequest;
        /* Client has sent, and server has recognized, a valid HTTP request.
           This is false when the session is new.  If and when the server
           reads the request from the client and finds it to be valid HTTP,
           it becomes true.
        */
    TRequestInfo requestInfo;
        /* Some of the strings this references are in individually malloc'ed
           memory, but some are pointers into arbitrary other data structures
           that happen to live as long as the session.  Some day, we will
           fix that.

           'requestInfo' is valid only if 'validRequest' is true.
        */
    uint32_t nbfileds;
    TList cookies;
    TList ranges;

    uint16_t status;
        /* Response status from handler.  Zero means session is not ready
           for a response yet.  This can mean that we ran a handler and it
           did not call ResponseStatus() to declare this fact.
        */
    TString header;

    bool serverDeniesKeepalive;
        /* Server doesn't want keepalive for this session, regardless of
           what happens in the session.  E.g. because the connection has
           already been kept alive long enough.
        */
    bool responseStarted;
        /* Handler has at least started the response (i.e. called
           ResponseWriteStart())
        */

    struct _TConn * conn;

    httpVersion version;

    TTable request_headers;
        /* All the headers in the HTTP request.  The key is the header
           name in lower case.  The value is the verbatim value from
           the header.
        */

    TTable response_headers;

    time_t date;

    bool chunkedwrite;
    bool chunkedwritemode;

    bool continueRequired;
        /* This client must receive 100 (continue) status before it will
           send more of the body of the request.
        */
};


#endif
