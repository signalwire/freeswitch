#include <assert.h>
#include <sys/types.h>
#include <string.h>
#include <stdio.h>

#include "bool.h"
#include "xmlrpc-c/util_int.h"
#include "xmlrpc-c/string_int.h"
#include "xmlrpc-c/abyss.h"
#include "server.h"
#include "http.h"
#include "conn.h"

#include "session.h"



abyss_bool
SessionRefillBuffer(TSession * const sessionP) {
/*----------------------------------------------------------------------------
   Get the next chunk of HTTP request body from the connection into
   the buffer.

   I.e. read data from the socket.
-----------------------------------------------------------------------------*/
    struct _TServer * const srvP = sessionP->connP->server->srvP;
    bool failed;

    failed = FALSE;  /* initial value */
            
    /* Reset our read buffer & flush data from previous reads. */
    ConnReadInit(sessionP->connP);

    if (sessionP->continueRequired)
        failed = !HTTPWriteContinue(sessionP);

    if (!failed) {
        const char * readError;

        sessionP->continueRequired = FALSE;

        /* Read more network data into our buffer.  Fail if we time out before
           client sends any data or client closes the connection or there's
           some network error.  We're very forgiving about the timeout here.
           We allow a full timeout per network read, which would allow
           somebody to keep a connection alive nearly indefinitely.  But it's
           hard to do anything intelligent here without very complicated code.
        */
        ConnRead(sessionP->connP, srvP->timeout, NULL, NULL, &readError);	
        if (readError) {
            failed = TRUE;
            xmlrpc_strfree(readError);
        }
    }
    return !failed;
}



size_t
SessionReadDataAvail(TSession * const sessionP) {

    return sessionP->connP->buffersize - sessionP->connP->bufferpos;

}



void
SessionGetReadData(TSession *    const sessionP, 
                   size_t        const max, 
                   const char ** const outStartP, 
                   size_t *      const outLenP) {
/*----------------------------------------------------------------------------
   Extract some HTTP request body which the server has read and
   buffered for the session.  Don't get or wait for any data that has
   not yet arrived.  Do not return more than 'max'.

   We return a pointer to the first byte as *outStartP, and the length in
   bytes as *outLenP.  The memory pointed to belongs to the session.
-----------------------------------------------------------------------------*/
    uint32_t const bufferPos = sessionP->connP->bufferpos;

    *outStartP = &sessionP->connP->buffer.t[bufferPos];

    assert(bufferPos <= sessionP->connP->buffersize);

    *outLenP = MIN(max, sessionP->connP->buffersize - bufferPos);

    /* move pointer past the bytes we are returning */
    sessionP->connP->bufferpos += *outLenP;

    assert(sessionP->connP->bufferpos <= sessionP->connP->buffersize);
}



void
SessionGetRequestInfo(TSession *            const sessionP,
                      const TRequestInfo ** const requestInfoPP) {
    
    *requestInfoPP = &sessionP->requestInfo;
}



void
SessionGetChannelInfo(TSession * const sessionP,
                      void **    const channelInfoPP) {
    
    *channelInfoPP = sessionP->connP->channelInfoP;
}



abyss_bool
SessionLog(TSession * const sessionP) {

    const char * logline;
    const char * user;
    const char * date;
    const char * peerInfo;

    if (sessionP->validRequest) {
        if (sessionP->requestInfo.user)
            user = sessionP->requestInfo.user;
        else
            user = "no_user";
    } else
        user = "???";
    
    DateToLogString(sessionP->date, &date);
    
    ConnFormatClientAddr(sessionP->connP, &peerInfo);
    
    xmlrpc_asprintf(&logline, "%s - %s - [%s] \"%s\" %d %u",
                    peerInfo,
                    user,
                    date, 
                    sessionP->validRequest ?
                        sessionP->requestInfo.requestline : "???",
                    sessionP->status,
                    sessionP->connP->outbytes
        );
    xmlrpc_strfree(peerInfo);
    xmlrpc_strfree(date);
    
    LogWrite(sessionP->connP->server, logline);

    xmlrpc_strfree(logline);
        
    return true;
}



void *
SessionGetDefaultHandlerCtx(TSession * const sessionP) {

    struct _TServer * const srvP = sessionP->connP->server->srvP;

    return srvP->defaultHandlerContext;
}
