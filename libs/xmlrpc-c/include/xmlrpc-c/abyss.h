/*****************************************************************************
                                 abyss.h
******************************************************************************

  This file is the interface header for the Abyss HTTP server component of
  XML-RPC For C/C++ (Xmlrpc-c).

  The Abyss component of Xmlrpc-c is based on the independently developed
  and distributed Abyss web server package from 2001.

  Copyright information is at the end of the file.
****************************************************************************/

#ifndef XMLRPC_ABYSS_H_INCLUDED
#define XMLRPC_ABYSS_H_INCLUDED


#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>

#include <xmlrpc-c/c_util.h>
#include <xmlrpc-c/inttypes.h>

/****************************************************************************
  STUFF FOR THE OUTER CONTROL PROGRAM TO USE
****************************************************************************/

typedef int abyss_bool;

/****************************************************************************
  GLOBAL (STATIC) PROGRAM STUFF
****************************************************************************/

XMLRPC_DLLEXPORT
void
AbyssInit(const char ** const errorP);

XMLRPC_DLLEXPORT
void
AbyssTerm(void);

/*********************************************************************
** MIMEType
*********************************************************************/

typedef struct MIMEType MIMEType;

XMLRPC_DLLEXPORT
MIMEType *
MIMETypeCreate(void);

XMLRPC_DLLEXPORT
void
MIMETypeDestroy(MIMEType * const MIMETypeP);

XMLRPC_DLLEXPORT
void
MIMETypeInit(void);

XMLRPC_DLLEXPORT
void
MIMETypeTerm(void);

XMLRPC_DLLEXPORT
abyss_bool
MIMETypeAdd2(MIMEType *   const MIMETypeP,
             const char * const type,
             const char * const ext);

XMLRPC_DLLEXPORT
abyss_bool
MIMETypeAdd(const char * const type,
            const char * const ext);


enum abyss_foreback {ABYSS_FOREGROUND, ABYSS_BACKGROUND};

#define HAVE_CHANSWITCH

typedef struct _TChanSwitch TChanSwitch;
typedef struct _TChannel TChannel;
typedef struct _TSocket TSocket;

#ifdef WIN32
  #include <xmlrpc-c/abyss_winsock.h>
#else
  #include <xmlrpc-c/abyss_unixsock.h>
#endif

XMLRPC_DLLEXPORT
void
ChanSwitchInit(const char ** const errorP);

XMLRPC_DLLEXPORT
void
ChanSwitchTerm(void);

/* If you're wondering where the constructors for TChanSwitch,
   TChannel, and TSocket are: They're implementation-specific, so look
   in abyss_unixsock.h, etc.
*/

XMLRPC_DLLEXPORT
void
ChanSwitchDestroy(TChanSwitch * const chanSwitchP);

XMLRPC_DLLEXPORT
void
ChannelInit(const char ** const errorP);

XMLRPC_DLLEXPORT
void
ChannelTerm(void);

XMLRPC_DLLEXPORT
void
ChannelDestroy(TChannel * const channelP);

XMLRPC_DLLEXPORT
void
SocketDestroy(TSocket * const socketP);


typedef struct {
    /* Before Xmlrpc-c 1.04, the internal server representation,
       struct _TServer, was exposed to users and was the only way to
       set certain parameters of the server.  Now, use the (new)
       ServerSet...() functions.  Use the HAVE_ macros to determine
       which method you have to use.
    */
    struct _TServer * srvP;
} TServer;

typedef struct _TSession TSession;

XMLRPC_DLLEXPORT
abyss_bool
ServerCreate(TServer *       const serverP,
             const char *    const name,
             xmlrpc_uint16_t const port,
             const char *    const filespath,
             const char *    const logfilename);

XMLRPC_DLLEXPORT
void
ServerCreateSwitch(TServer *     const serverP,
                   TChanSwitch * const chanSwitchP,
                   const char ** const errorP);

XMLRPC_DLLEXPORT
abyss_bool
ServerCreateSocket(TServer *    const serverP,
                   const char * const name,
                   TOsSocket    const socketFd,
                   const char * const filespath,
                   const char * const logfilename);

#define HAVE_SERVER_CREATE_SOCKET_2
XMLRPC_DLLEXPORT
void
ServerCreateSocket2(TServer *     const serverP,
                    TSocket *     const socketP,
                    const char ** const errorP);

XMLRPC_DLLEXPORT
abyss_bool
ServerCreateNoAccept(TServer *    const serverP,
                     const char * const name,
                     const char * const filespath,
                     const char * const logfilename);

XMLRPC_DLLEXPORT
void
ServerFree(TServer * const serverP);

XMLRPC_DLLEXPORT
void
ServerSetName(TServer *    const serverP,
              const char * const name);

XMLRPC_DLLEXPORT
void
ServerSetFilesPath(TServer *    const serverP,
                   const char * const filesPath);

XMLRPC_DLLEXPORT
void
ServerSetLogFileName(TServer *    const serverP,
                     const char * const logFileName);

#define HAVE_SERVER_SET_KEEPALIVE_TIMEOUT 1
XMLRPC_DLLEXPORT
void
ServerSetKeepaliveTimeout(TServer *       const serverP,
                          xmlrpc_uint32_t const keepaliveTimeout);

#define HAVE_SERVER_SET_KEEPALIVE_MAX_CONN 1
XMLRPC_DLLEXPORT
void
ServerSetKeepaliveMaxConn(TServer *       const serverP,
                          xmlrpc_uint32_t const keepaliveMaxConn);

#define HAVE_SERVER_SET_TIMEOUT 1
XMLRPC_DLLEXPORT
void
ServerSetTimeout(TServer *       const serverP,
                 xmlrpc_uint32_t const timeout);

#define HAVE_SERVER_SET_ADVERTISE 1
XMLRPC_DLLEXPORT
void
ServerSetAdvertise(TServer *  const serverP,
                   abyss_bool const advertise);

#define HAVE_SERVER_SET_MIME_TYPE 1
XMLRPC_DLLEXPORT
void
ServerSetMimeType(TServer *  const serverP,
                  MIMEType * const MIMETypeP);

XMLRPC_DLLEXPORT
int
ServerInit(TServer * const serverP);

XMLRPC_DLLEXPORT
void
ServerRun(TServer * const serverP);

XMLRPC_DLLEXPORT
void
ServerRunOnce(TServer * const serverP);

/* ServerRunOnce2() is obsolete.  See user's guide. */
XMLRPC_DLLEXPORT
void
ServerRunOnce2(TServer *           const serverP,
               enum abyss_foreback const foregroundBackground);

XMLRPC_DLLEXPORT
void
ServerRunChannel(TServer *     const serverP,
                 TChannel *    const channelP,
                 void *        const channelInfoP,
                 const char ** const errorP);

#define HAVE_SERVER_RUN_CONN_2
XMLRPC_DLLEXPORT
void
ServerRunConn2(TServer *     const serverP,
               TSocket *     const connectedSocketP,
               const char ** const errorP);

XMLRPC_DLLEXPORT
void
ServerRunConn(TServer * const serverP,
              TOsSocket const connectedSocket);

XMLRPC_DLLEXPORT
void
ServerDaemonize(TServer * const serverP);

XMLRPC_DLLEXPORT
void
ServerTerminate(TServer * const serverP);

XMLRPC_DLLEXPORT
void
ServerResetTerminate(TServer * const serverP);

XMLRPC_DLLEXPORT
void
ServerUseSigchld(TServer * const serverP);

#ifndef WIN32
void
ServerHandleSigchld(pid_t const pid);
#endif

typedef abyss_bool (*URIHandler) (TSession *); /* deprecated */

struct URIHandler2;

typedef void (*initHandlerFn)(struct URIHandler2 *, abyss_bool *);

typedef void (*termHandlerFn)(void *);

typedef void (*handleReq3Fn)(void *,
                             TSession *,
                             abyss_bool *);

typedef void (*handleReq2Fn)(struct URIHandler2 *,
                             TSession *,
                             abyss_bool *);

struct ServerReqHandler3 {
    termHandlerFn term;
    handleReq3Fn  handleReq;
    void *        userdata;
    size_t        handleReqStackSize; /* zero = default */
};

XMLRPC_DLLEXPORT
void
ServerAddHandler3(TServer *                        const serverP,
                  const struct ServerReqHandler3 * const handlerP,
                  abyss_bool *                     const successP);

typedef struct URIHandler2 {
    initHandlerFn init;
    termHandlerFn term;
    handleReq2Fn  handleReq2;
    URIHandler    handleReq1;  /* deprecated */
    void *        userdata;
} URIHandler2;

XMLRPC_DLLEXPORT
void
ServerAddHandler2(TServer *     const srvP,
                  URIHandler2 * const handlerP,
                  abyss_bool *  const successP);

XMLRPC_DLLEXPORT
abyss_bool
ServerAddHandler(TServer * const srvP,
                 URIHandler const handler);

typedef abyss_bool (*THandlerDflt) (TSession *);

/* Note: 'handler' used to be URIHandler;  THandlerDflt is a newer name
   for the same type
*/

XMLRPC_DLLEXPORT
void
ServerDefaultHandler(TServer *    const srvP,
                     THandlerDflt const handler);

/* ConfReadServerFile() is inappropriately named; it was a mistake.
   But then, so is having this function at all.  The config file is
   inappropriate for an API.
*/

XMLRPC_DLLEXPORT
abyss_bool
ConfReadServerFile(const char * const filename,
                   TServer *    const srvP);

XMLRPC_DLLEXPORT
void
LogWrite(TServer *    const srvP,
         const char * const c);

/****************************************************************************
  STUFF FOR HTTP REQUEST HANDLERS TO USE
****************************************************************************/

typedef enum {
    m_unknown, m_get, m_put, m_head, m_post, m_delete, m_trace, m_options
} TMethod;

typedef struct {
    TMethod method;
    const char * uri;
        /* This is NOT the URI.  It is the pathname part of the URI.
           We really should fix that and put the pathname in another
           member.  If the URI does not contain a pathname, this is "*".
        */
    const char * query;
        /* The query part of the URI (stuff after '?').  NULL if none. */
    const char * host;
        /* NOT the value of the host: header field.  Rather, the name of the
           target host (could be part of the host: value; could be from the
           URI).  No port number.  NULL if request does not specify a host
           name.
        */
    const char * from;
    const char * useragent;
    const char * referer;
    const char * requestline;
    const char * user;
        /* Requesting user (from authorization: header field).  NULL if
           request doesn't specify or handler has not authenticated it.
        */
    xmlrpc_uint16_t port;
        /* The port number from the URI, or default 80 if the URI doesn't
           specify a port.
        */
    abyss_bool keepalive;
} TRequestInfo;

XMLRPC_DLLEXPORT
abyss_bool
SessionRefillBuffer(TSession * const sessionP);

XMLRPC_DLLEXPORT
size_t
SessionReadDataAvail(TSession * const sessionP);

XMLRPC_DLLEXPORT
void
SessionGetReadData(TSession *    const sessionP, 
                   size_t        const max, 
                   const char ** const outStartP, 
                   size_t *      const outLenP);

XMLRPC_DLLEXPORT
void
SessionGetRequestInfo(TSession *            const sessionP,
                      const TRequestInfo ** const requestInfoPP);

XMLRPC_DLLEXPORT
void
SessionGetChannelInfo(TSession * const sessionP,
                      void **    const channelInfoPP);

XMLRPC_DLLEXPORT
void *
SessionGetDefaultHandlerCtx(TSession * const sessionP);

XMLRPC_DLLEXPORT
char *
RequestHeaderValue(TSession *   const sessionP,
                   const char * const name);

XMLRPC_DLLEXPORT
abyss_bool
ResponseAddField(TSession *   const sessionP,
                 const char * const name,
                 const char * const value);

XMLRPC_DLLEXPORT
abyss_bool
ResponseWriteStart(TSession * const sessionP);

/* For backward compatibility: */
#define ResponseWrite ResponseWriteStart

XMLRPC_DLLEXPORT
abyss_bool
ResponseWriteBody(TSession *      const sessionP,
                  const char *    const data,
                  xmlrpc_uint32_t const len);

XMLRPC_DLLEXPORT
abyss_bool
ResponseWriteEnd(TSession * const sessionP);

XMLRPC_DLLEXPORT
abyss_bool
ResponseChunked(TSession * const sessionP);

XMLRPC_DLLEXPORT
xmlrpc_uint16_t
ResponseStatusFromErrno(int const errnoArg);

XMLRPC_DLLEXPORT
void
ResponseStatus(TSession *      const sessionP,
               xmlrpc_uint16_t const code);

XMLRPC_DLLEXPORT
void
ResponseStatusErrno(TSession * const sessionP);

XMLRPC_DLLEXPORT
abyss_bool
ResponseContentType(TSession *   const serverP,
                    const char * const type);

XMLRPC_DLLEXPORT
abyss_bool
ResponseContentLength(TSession *      const sessionP,
                      xmlrpc_uint64_t const len);

typedef struct {
/*----------------------------------------------------------------------------
   These are parameters to control the HTTP "Access Control functions.  That's
   where the client asks whether it is OK to send a request that some other
   server asked the client to send (e.g. a person web browses a page at
   a.example.com, and it sends a script that executes on the user's computer
   and tries to perform an XML-RPC RPC on b.example.com.  The user's browser
   first asks b.example.com if it is OK to do an RPC that is really initiated
   by a.example.com.
-----------------------------------------------------------------------------*/
    const char * allowOrigin;
        /* This tells what original servers (a.example.com in the example
           above) are allowed to submit RPCs indirectly to us.  The value is a
           verbatim value for an HTTP Access-Control-Allow-Origin header field
           (just the value part of the field, not the whole field).  "*"
           therefore means everyone is allowed.  "" means no one.
    
           NULL means not to say anything about access control to the client.
        */
    abyss_bool expires;
        /* The permissions herein expire after a certain period from now.
           'maxAge' is that period.
        */
    unsigned int maxAge;
        /* Meaningful only when 'expires' is true.  The expiration period
           in seconds.  Zero is valid.
        */
} ResponseAccessCtl;

XMLRPC_DLLEXPORT
void
ResponseAccessControl(TSession *        const abyssSessionP, 
                      ResponseAccessCtl const accessControl);

XMLRPC_DLLEXPORT
void
ResponseError2(TSession *   const sessionP,
               const char * const explanation);

XMLRPC_DLLEXPORT
void
ResponseError(TSession * const sessionP);

XMLRPC_DLLEXPORT
const char *
MIMETypeFromExt(const char * const ext);

XMLRPC_DLLEXPORT
const char *
MIMETypeFromExt2(MIMEType *   const MIMETypeP,
                 const char * const ext);

XMLRPC_DLLEXPORT
const char *
MIMETypeFromFileName2(MIMEType *   const MIMETypeP,
                      const char * const fileName);

XMLRPC_DLLEXPORT
const char *
MIMETypeFromFileName(const char * const fileName);

XMLRPC_DLLEXPORT
const char *
MIMETypeGuessFromFile2(MIMEType *   const MIMETypeP,
                       const char * const fileName);

XMLRPC_DLLEXPORT
const char *
MIMETypeGuessFromFile(const char * const filename);


/****************************************************************************
  STUFF THAT PROBABLY DOESN'T BELONG IN THIS FILE BECAUSE IT IS INTERNAL

  Some day, we sort this out.
****************************************************************************/


#define CR      '\r'
#define LF      '\n'
#define CRLF    "\r\n"

/*********************************************************************
** Paths and so on...
*********************************************************************/

#ifdef WIN32
#define DEFAULT_ROOT        "c:\\abyss"
#define DEFAULT_DOCS        DEFAULT_ROOT"\\htdocs"
#define DEFAULT_CONF_FILE   DEFAULT_ROOT"\\conf\\abyss.conf"
#define DEFAULT_LOG_FILE    DEFAULT_ROOT"\\log\\abyss.log"
#else
#ifdef __rtems__
#define DEFAULT_ROOT        "/abyss"
#else
#define DEFAULT_ROOT        "/usr/local/abyss"
#endif
#define DEFAULT_DOCS        DEFAULT_ROOT"/htdocs"
#define DEFAULT_CONF_FILE   DEFAULT_ROOT"/conf/abyss.conf"
#define DEFAULT_LOG_FILE    DEFAULT_ROOT"/log/abyss.log"
#endif

/*********************************************************************
** Maximum number of simultaneous connections
*********************************************************************/

#define MAX_CONN    16

/*********************************************************************
** General purpose definitions
*********************************************************************/

#ifndef NULL
#define NULL ((void *)0)
#endif  /* NULL */

#ifndef TRUE
#define TRUE    1
#endif  /* TRUE */

#ifndef FALSE
#define FALSE    0
#endif  /* FALSE */

/*********************************************************************
** Range
*********************************************************************/

XMLRPC_DLLEXPORT
abyss_bool
RangeDecode(char *            const str,
            xmlrpc_uint64_t   const filesize,
            xmlrpc_uint64_t * const start,
            xmlrpc_uint64_t * const end);

XMLRPC_DLLEXPORT
abyss_bool DateInit(void);

/*********************************************************************
** Session
*********************************************************************/

XMLRPC_DLLEXPORT
abyss_bool SessionLog(TSession * const s);


#ifdef __cplusplus
}


#endif

/*****************************************************************************
** Here is the copyright notice from the Abyss web server project file from
** which this file is derived.
**
** Copyright (C) 2000 by Moez Mahfoudh <mmoez@bigfoot.com>.
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
** 
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
** ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
** IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
** ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
** FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
** DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
** OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
** HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
** LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
** OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
** SUCH DAMAGE.
**
******************************************************************************/
#endif  /* _ABYSS_H_ */
