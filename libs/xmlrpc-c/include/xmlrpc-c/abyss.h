/*****************************************************************************
                                 abyss.h
******************************************************************************

  This file is the interface header for the Abyss HTTP server component of
  XML-RPC For C/C++ (Xmlrpc-c).

  The Abyss component of Xmlrpc-c is based on the independently developed
  and distributed Abyss web server package from 2001.

  Copyright information is at the end of the file.
****************************************************************************/

#ifndef _ABYSS_H_
#define _ABYSS_H_


#ifdef __cplusplus
extern "C" {
#endif

#ifdef ABYSS_WIN32
#include "xmlrpc_config.h"
#else
#include <inttypes.h>
#endif
/*********************************************************************
** Paths and so on...
*********************************************************************/

#ifdef ABYSS_WIN32
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
** Maximum numer of simultaneous connections
*********************************************************************/

#define MAX_CONN    4000

/*********************************************************************
** Server Info Definitions
*********************************************************************/

#define SERVER_VERSION      "0.3"
#define SERVER_HVERSION     "ABYSS/0.3"
#define SERVER_HTML_INFO \
  "<p><HR><b><i><a href=\"http:\057\057abyss.linuxave.net\">" \
  "ABYSS Web Server</a></i></b> version "SERVER_VERSION"<br>" \
  "&copy; <a href=\"mailto:mmoez@bigfoot.com\">Moez Mahfoudh</a> - 2000</p>"
#define SERVER_PLAIN_INFO \
  CRLF "----------------------------------------" \
       "----------------------------------------" \
  CRLF "ABYSS Web Server version "SERVER_VERSION CRLF"(C) Moez Mahfoudh - 2000"

/*********************************************************************
** General purpose definitions
*********************************************************************/

#ifdef ABYSS_WIN32
#define strcasecmp(a,b) stricmp((a),(b))
#else
#define ioctlsocket(a,b,c)  ioctl((a),(b),(c))
#endif  /* ABYSS_WIN32 */

#ifndef NULL
#define NULL ((void *)0)
#endif  /* NULL */

#ifndef TRUE
#define TRUE    1
#endif  /* TRUE */

#ifndef FALSE
#define FALSE    0
#endif  /* FALSE */

#ifdef ABYSS_WIN32
#define LBR "\n"
#else
#define LBR "\n"
#endif  /* ABYSS_WIN32 */

typedef int abyss_bool;

/*********************************************************************
** Buffer
*********************************************************************/

typedef struct
{
    void *data;
    uint32_t size;
    uint32_t staticid;
} TBuffer;

abyss_bool BufferAlloc(TBuffer *buf,uint32_t memsize);
abyss_bool BufferRealloc(TBuffer *buf,uint32_t memsize);
void BufferFree(TBuffer *buf);


/*********************************************************************
** String
*********************************************************************/

typedef struct
{
    TBuffer buffer;
    uint32_t size;
} TString;

abyss_bool StringAlloc(TString *s);
abyss_bool StringConcat(TString *s,char *s2);
abyss_bool StringBlockConcat(TString *s,char *s2,char **ref);
void StringFree(TString *s);
char *StringData(TString *s);


/*********************************************************************
** List
*********************************************************************/

typedef struct {
    void **item;
    uint16_t size;
    uint16_t maxsize;
    abyss_bool autofree;
} TList;

void
ListInit(TList * const listP);

void
ListInitAutoFree(TList * const listP);

void
ListFree(TList * const listP);

void
ListFreeItems(TList * const listP);

abyss_bool
ListAdd(TList * const listP,
        void *  const str);

void
ListRemove(TList * const listP);

abyss_bool
ListAddFromString(TList * const listP,
                  char *  const c);

abyss_bool
ListFindString(TList *    const listP,
               char *     const str,
               uint16_t * const indexP);


/*********************************************************************
** Table
*********************************************************************/

typedef struct 
{
    char *name,*value;
    uint16_t hash;
} TTableItem;

typedef struct
{
    TTableItem *item;
    uint16_t size,maxsize;
} TTable;

void TableInit(TTable *t);
void TableFree(TTable *t);
abyss_bool TableAdd(TTable *t,char *name,char *value);
abyss_bool TableAddReplace(TTable *t,char *name,char *value);
abyss_bool TableFindIndex(TTable *t,char *name,uint16_t *index);
char *TableFind(TTable *t,char *name);


/*********************************************************************
** Thread
*********************************************************************/

#ifdef ABYSS_WIN32
#include <windows.h>
#define  THREAD_ENTRYTYPE  WINAPI
#else
#define  THREAD_ENTRYTYPE
#include <pthread.h>
#endif  /* ABYSS_WIN32 */

typedef uint32_t (THREAD_ENTRYTYPE *TThreadProc)(void *);
#ifdef ABYSS_WIN32
typedef HANDLE TThread;
#else
typedef pthread_t TThread;
typedef void* (*PTHREAD_START_ROUTINE)(void *);
#endif  /* ABYSS_WIN32 */

abyss_bool ThreadCreate(TThread *t,TThreadProc func,void *arg);
abyss_bool ThreadRun(TThread *t);
abyss_bool ThreadStop(TThread *t);
abyss_bool ThreadKill(TThread *t);
void ThreadWait(uint32_t ms);
void ThreadExit( TThread *t, int ret_value );
void ThreadClose( TThread *t );

/*********************************************************************
** Mutex
*********************************************************************/

#ifdef ABYSS_WIN32
typedef HANDLE TMutex;
#else
typedef pthread_mutex_t TMutex;
#endif  /* ABYSS_WIN32 */

abyss_bool MutexCreate(TMutex *m);
abyss_bool MutexLock(TMutex *m);
abyss_bool MutexUnlock(TMutex *m);
abyss_bool MutexTryLock(TMutex *m);
void MutexFree(TMutex *m);


/*********************************************************************
** Pool
*********************************************************************/

typedef struct _TPoolZone
{
    char *pos,*maxpos;
    struct _TPoolZone *next,*prev;
/*  char data[0]; */
    char data[1];
} TPoolZone;

typedef struct
{
    TPoolZone *firstzone,*currentzone;
    uint32_t zonesize;
    TMutex mutex;
} TPool;

abyss_bool PoolCreate(TPool *p,uint32_t zonesize);
void PoolFree(TPool *p);

void *PoolAlloc(TPool *p,uint32_t size);
char *PoolStrdup(TPool *p,char *s);


/*********************************************************************
** Socket
*********************************************************************/

#ifdef ABYSS_WIN32
#include <winsock.h>
#else
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>

#ifdef HAVE_SYS_FILIO_H
#include <sys/filio.h>
#endif
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif

#endif  /* ABYSS_WIN32 */

#define TIME_INFINITE   0xffffffff

#ifdef ABYSS_WIN32
typedef SOCKET TSocket;
#else
typedef uint32_t TSocket;
#endif  /* ABYSS_WIN32 */

typedef struct in_addr TIPAddr;

#define IPB1(x) (((unsigned char *)(&x))[0])
#define IPB2(x) (((unsigned char *)(&x))[1])
#define IPB3(x) (((unsigned char *)(&x))[2])
#define IPB4(x) (((unsigned char *)(&x))[3])

abyss_bool SocketInit(void);

abyss_bool SocketCreate(TSocket *s);
abyss_bool SocketClose(TSocket *s);

int SocketWrite(TSocket *s, char *buffer, uint32_t len);
uint32_t SocketRead(TSocket *s, char *buffer, uint32_t len);
uint32_t SocketPeek(TSocket *s, char *buffer, uint32_t len);

abyss_bool SocketConnect(TSocket *s, TIPAddr *addr, uint16_t port);
abyss_bool SocketBind(TSocket *s, TIPAddr *addr, uint16_t port);

abyss_bool SocketListen(TSocket *s, uint32_t backlog);
abyss_bool SocketAccept(TSocket *s, TSocket *ns,TIPAddr *ip);

uint32_t SocketError(void);

uint32_t SocketWait(TSocket *s,abyss_bool rd,abyss_bool wr,uint32_t timems);

abyss_bool SocketBlocking(TSocket *s, abyss_bool b);
uint32_t SocketAvailableReadBytes(TSocket *s);


/*********************************************************************
** File
*********************************************************************/

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>

#ifndef NAME_MAX
#define NAME_MAX    1024
#endif

#ifdef ABYSS_WIN32
#ifndef __BORLANDC__
#define O_APPEND    _O_APPEND
#define O_CREAT     _O_CREAT 
#define O_EXCL      _O_EXCL
#define O_RDONLY    _O_RDONLY
#define O_RDWR      _O_RDWR 
#define O_TRUNC _O_TRUNC
#define O_WRONLY    _O_WRONLY
#define O_TEXT      _O_TEXT
#define O_BINARY    _O_BINARY
#endif

#define A_HIDDEN    _A_HIDDEN
#define A_NORMAL    _A_NORMAL
#define A_RDONLY    _A_RDONLY
#define A_SUBDIR    _A_SUBDIR
#else
#define A_SUBDIR    1
#define O_BINARY    0
#define O_TEXT      0
#endif  /* ABYSS_WIN32 */

#ifdef ABYSS_WIN32

#ifndef __BORLANDC__
typedef struct _stati64 TFileStat;
typedef struct _finddata_t TFileInfo;
typedef long TFileFind;

#else

typedef struct stat TFileStat;
typedef struct finddata_t
{
    char name[NAME_MAX+1];
    int attrib;
    uint64_t size;
    time_t time_write;
   WIN32_FIND_DATA data;
} TFileInfo;
typedef HANDLE TFileFind;
#endif

#else

#include <unistd.h>
#include <dirent.h>

typedef struct stat TFileStat;

typedef struct finddata_t
{
    char name[NAME_MAX+1];
    int attrib;
    uint64_t size;
    time_t time_write;
} TFileInfo;

typedef struct 
{
    char path[NAME_MAX+1];
    DIR *handle;
} TFileFind;

#endif

typedef int TFile;

abyss_bool FileOpen(TFile *f, const char *name,uint32_t attrib);
abyss_bool FileOpenCreate(TFile *f, const char *name, uint32_t attrib);
abyss_bool FileClose(TFile *f);

abyss_bool FileWrite(TFile *f, void *buffer, uint32_t len);
int32_t FileRead(TFile *f, void *buffer, uint32_t len);

abyss_bool FileSeek(TFile *f, uint64_t pos, uint32_t attrib);
uint64_t FileSize(TFile *f);

abyss_bool FileStat(char *filename,TFileStat *filestat);

abyss_bool FileFindFirst(TFileFind *filefind,char *path,TFileInfo *fileinfo);
abyss_bool FileFindNext(TFileFind *filefind,TFileInfo *fileinfo);
void FileFindClose(TFileFind *filefind);

/*********************************************************************
** Server (1/2)
*********************************************************************/

typedef struct _TServer
{
    TSocket listensock;
    TFile logfile;
    TMutex logmutex;
    char *name;
    char *filespath;
    uint16_t port;
    uint32_t keepalivetimeout,keepalivemaxconn,timeout;
    TList handlers;
    TList defaultfilenames;
    void *defaulthandler;
    abyss_bool advertise;
	int running;
#ifndef _WIN32
    uid_t uid;
    gid_t gid;
    TFile pidfile;
#endif  
} TServer;


/*********************************************************************
** Conn
*********************************************************************/

#define BUFFER_SIZE 4096 

typedef struct _TConn
{
    TServer *server;
    uint32_t buffersize,bufferpos;
    uint32_t inbytes,outbytes;  
    TSocket socket;
    TIPAddr peerip;
    abyss_bool hasOwnThread;
    TThread thread;
    abyss_bool connected;
    abyss_bool inUse;
    const char * trace;
    void (*job)(struct _TConn *);
    char buffer[BUFFER_SIZE];
} TConn;

TConn *ConnAlloc(void);
void ConnFree(TConn *c);

enum abyss_foreback {ABYSS_FOREGROUND, ABYSS_BACKGROUND};

abyss_bool ConnCreate(TConn *c, TSocket *s, void (*func)(TConn *));
abyss_bool ConnCreate2(TConn *             const connectionP, 
                       TServer *           const serverP,
                       TSocket             const connectedSocket,
                       TIPAddr             const peerIpAddr,
                       void            ( *       func)(TConn *),
                       enum abyss_foreback const foregroundBackground);
abyss_bool ConnProcess(TConn *c);
abyss_bool ConnKill(TConn *c);
void ConnClose(TConn *c);

abyss_bool ConnWrite(TConn *c,void *buffer,uint32_t size);
abyss_bool ConnRead(TConn *c, uint32_t timems);
void ConnReadInit(TConn *c);
abyss_bool ConnReadLine(TConn *c,char **z,uint32_t timems);

abyss_bool ConnWriteFromFile(TConn *c,TFile *file,uint64_t start,uint64_t end,
            void *buffer,uint32_t buffersize,uint32_t rate);


/*********************************************************************
** Range
*********************************************************************/

abyss_bool RangeDecode(char *str,uint64_t filesize,uint64_t *start,uint64_t *end);

/*********************************************************************
** Date
*********************************************************************/

#include <time.h>

typedef struct tm TDate;

abyss_bool DateToString(TDate *tm,char *s);
abyss_bool DateToLogString(TDate *tm,char *s);

abyss_bool DateDecode(char *s,TDate *tm);

int32_t DateCompare(TDate *d1,TDate *d2);

abyss_bool DateFromGMT(TDate *d,time_t t);
abyss_bool DateFromLocal(TDate *d,time_t t);

abyss_bool DateInit(void);

/*********************************************************************
** Base64
*********************************************************************/

void Base64Encode(char *s,char *d);

/*********************************************************************
** Session
*********************************************************************/

typedef enum
{
    m_unknown,m_get,m_put,m_head,m_post,m_delete,m_trace,m_options
} TMethod;

typedef struct
{
    TMethod method;
    uint32_t nbfileds;
    char *uri;
    char *query;
    char *host;
    char *from;
    char *useragent;
    char *referer;
    char *requestline;
    char *user;
    uint16_t port;
    TList cookies;
    TList ranges;

    uint16_t status;
    TString header;

    abyss_bool keepalive,cankeepalive;
    abyss_bool done;

    TServer *server;
    TConn *conn;

    uint8_t versionminor,versionmajor;

    TTable request_headers,response_headers;

    TDate date;

    abyss_bool chunkedwrite,chunkedwritemode;
} TSession;

/*********************************************************************
** Request
*********************************************************************/

#define CR      '\r'
#define LF      '\n'
#define CRLF    "\r\n"

abyss_bool RequestValidURI(TSession *r);
abyss_bool RequestValidURIPath(TSession *r);
abyss_bool RequestUnescapeURI(TSession *r);

char *RequestHeaderValue(TSession *r,char *name);

abyss_bool RequestRead(TSession *r);
void RequestInit(TSession *r,TConn *c);
void RequestFree(TSession *r);

abyss_bool RequestAuth(TSession *r,char *credential,char *user,char *pass);

/*********************************************************************
** Response
*********************************************************************/

abyss_bool ResponseAddField(TSession *r,char *name,char *value);
void ResponseWrite(TSession *r);

abyss_bool ResponseChunked(TSession *s);

void ResponseStatus(TSession *r,uint16_t code);
void ResponseStatusErrno(TSession *r);

abyss_bool ResponseContentType(TSession *r,char *type);
abyss_bool ResponseContentLength(TSession *r,uint64_t len);

void ResponseError(TSession *r);


/*********************************************************************
** HTTP
*********************************************************************/

char *HTTPReasonByStatus(uint16_t status);

int32_t HTTPRead(TSession *s,char *buffer,uint32_t len);

abyss_bool HTTPWrite(TSession *s,char *buffer,uint32_t len);
abyss_bool HTTPWriteEnd(TSession *s);

/*********************************************************************
** Server (2/2)
*********************************************************************/

abyss_bool ServerCreate(TServer *srv,
                        const char *name,
                        uint16_t port,
                        const char *filespath,
                        const char *logfilename);

void ServerFree(TServer *srv);

int ServerInit(TServer *srv);
void ServerRun(TServer *srv);
void ServerRunOnce(TServer *srv);
void ServerRunOnce2(TServer *           const srv,
                    enum abyss_foreback const foregroundBackground);

typedef abyss_bool (*URIHandler) (TSession *); /* deprecated */

struct URIHandler2;

typedef void (*initHandlerFn)(struct URIHandler2 *,
                              abyss_bool *);

typedef void (*termHandlerFn)(struct URIHandler2 *);

typedef void (*handleReq2Fn)(struct URIHandler2 *,
                             TSession *,
                             abyss_bool *);

typedef struct URIHandler2 {
    initHandlerFn init;
    termHandlerFn term;
    handleReq2Fn  handleReq2;
    URIHandler    handleReq1;  /* deprecated */
    void *        userdata;
} URIHandler2;

void
ServerAddHandler2(TServer *     const srvP,
                  URIHandler2 * const handlerP,
                  abyss_bool *  const successP);

abyss_bool
ServerAddHandler(TServer * const srvP,
                 URIHandler const handler);

void
ServerDefaultHandler(TServer *  const srvP,
                     URIHandler const handler);

abyss_bool LogOpen(TServer *srv, const char *filename);
void LogWrite(TServer *srv,char *c);
void LogClose(TServer *srv);


/*********************************************************************
** MIMEType
*********************************************************************/

void MIMETypeInit(void);
abyss_bool MIMETypeAdd(char *type,char *ext);
char *MIMETypeFromExt(char *ext);
char *MIMETypeFromFileName(char *filename);
char *MIMETypeGuessFromFile(char *filename);


/*********************************************************************
** Conf
*********************************************************************/

abyss_bool ConfReadMIMETypes(char *filename);
abyss_bool ConfReadServerFile(const char *filename,TServer *srv);


/*********************************************************************
** Trace
*********************************************************************/

void TraceMsg(char *fmt,...);
void TraceExit(char *fmt,...);


/*********************************************************************
** Session
*********************************************************************/

abyss_bool SessionLog(TSession *s);


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
