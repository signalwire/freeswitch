/* Copyright information is at the end of the file */

#include <ctype.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#include "xmlrpc_config.h"
#include "bool.h"
#include "mallocvar.h"
#include "xmlrpc-c/util.h"
#include "xmlrpc-c/string_int.h"
#include "xmlrpc-c/abyss.h"

#include "server.h"
#include "session.h"
#include "conn.h"
#include "token.h"
#include "date.h"
#include "data.h"

#include "http.h"

/*********************************************************************
** Request Parser
*********************************************************************/

/*********************************************************************
** Request
*********************************************************************/

static void
initRequestInfo(TRequestInfo * const requestInfoP,
                httpVersion    const httpVersion,
                const char *   const requestLine,
                TMethod        const httpMethod,
                const char *   const host,
                unsigned int   const port,
                const char *   const path,
                const char *   const query) {
/*----------------------------------------------------------------------------
  Set up the request info structure.  For information that is
  controlled by headers, use the defaults -- I.e. the value that
  applies if the request contains no applicable header.
-----------------------------------------------------------------------------*/
    XMLRPC_ASSERT_PTR_OK(requestLine);
    XMLRPC_ASSERT_PTR_OK(path);

    requestInfoP->requestline = strdup(requestLine);
    requestInfoP->method      = httpMethod;
    requestInfoP->host        = xmlrpc_strdupnull(host);
    requestInfoP->port        = port;
    requestInfoP->uri         = strdup(path);
    requestInfoP->query       = xmlrpc_strdupnull(query);
    requestInfoP->from        = NULL;
    requestInfoP->useragent   = NULL;
    requestInfoP->referer     = NULL;
    requestInfoP->user        = NULL;

    if (httpVersion.major > 1 ||
        (httpVersion.major == 1 && httpVersion.minor >= 1))
        requestInfoP->keepalive = TRUE;
    else
        requestInfoP->keepalive = FALSE;
}



static void
freeRequestInfo(TRequestInfo * const requestInfoP) {

    xmlrpc_strfreenull(requestInfoP->host);

    xmlrpc_strfreenull(requestInfoP->user);

    xmlrpc_strfree(requestInfoP->uri);

    xmlrpc_strfree(requestInfoP->requestline);
}



void
RequestInit(TSession * const sessionP,
            TConn *    const connectionP) {

    sessionP->validRequest = false;  /* Don't have valid request yet */

    time(&sessionP->date);

    sessionP->conn = connectionP;

    sessionP->responseStarted = FALSE;

    sessionP->chunkedwrite = FALSE;
    sessionP->chunkedwritemode = FALSE;

    sessionP->continueRequired = FALSE;

    ListInit(&sessionP->cookies);
    ListInit(&sessionP->ranges);
    TableInit(&sessionP->request_headers);
    TableInit(&sessionP->response_headers);

    sessionP->status = 0;  /* No status from handler yet */

    StringAlloc(&(sessionP->header));
}



void
RequestFree(TSession * const sessionP) {

    if (sessionP->validRequest)
        freeRequestInfo(&sessionP->requestInfo);

    ListFree(&sessionP->cookies);
    ListFree(&sessionP->ranges);
    TableFree(&sessionP->request_headers);
    TableFree(&sessionP->response_headers);
    StringFree(&(sessionP->header));
}



static char *
firstLfPos(TConn * const connectionP,
           char *  const lineStart) {
/*----------------------------------------------------------------------------
   Return a pointer in the connection's receive buffer to the first
   LF (linefeed aka newline) character in the buffer at or after 'lineStart'.

   If there is no LF in the buffer at or after 'lineStart', return NULL.
-----------------------------------------------------------------------------*/
    const char * const bufferEnd =
        connectionP->buffer + connectionP->buffersize;

    char * p;

    for (p = lineStart; p < bufferEnd && *p != LF; ++p);

    if (p < bufferEnd)
        return p;
    else
        return NULL;
}



static void
getLineInBuffer(TConn * const connectionP,
                char *  const lineStart,
                time_t  const deadline,
                char ** const lineEndP,
                bool *  const errorP) {
/*----------------------------------------------------------------------------
   Get a line into the connection's read buffer, starting at position
   'lineStart', if there isn't one already there.   'lineStart' is either
   within the buffer or just after it.

   Read the channel until we get a full line, except fail if we don't get
   one by 'deadline'.
-----------------------------------------------------------------------------*/
    bool error;
    char * lfPos;

    assert(lineStart <= connectionP->buffer + connectionP->buffersize);

    error = FALSE;  /* initial value */
    lfPos = NULL;  /* initial value */

    while (!error && !lfPos) {
        int const timeLeft = (int)(deadline - time(NULL));

        if (timeLeft <= 0)
            error = TRUE;
        else {
            lfPos = firstLfPos(connectionP, lineStart);
            if (!lfPos)
                error = !ConnRead(connectionP, timeLeft);
        }
    }    
    *errorP = error;
    *lineEndP = lfPos + 1;
}



static bool
isContinuationLine(const char * const line) {

    return (line[0] == ' ' || line[0] == '\t');
}



static bool
isEmptyLine(const char * const line) {

    return (line[0] == '\n' || (line[0] == '\r' && line[1] == '\n'));
}



static void
convertLineEnd(char * const lineStart,
               char * const prevLineStart,
               char   const newVal) {
/*----------------------------------------------------------------------------
   Assuming a line begins at 'lineStart' and the line before it (the
   "previous line") begins at 'prevLineStart', replace the line
   delimiter at the end of the previous line with the character 'newVal'.

   The line delimiter is either CRLF or LF.  In the CRLF case, we replace
   both CR and LF with 'newVal'.
-----------------------------------------------------------------------------*/
    assert(lineStart >= prevLineStart + 1);
    *(lineStart-1) = newVal;
    if (prevLineStart + 1 < lineStart &&
        *(lineStart-2) == CR)
        *(lineStart-2) = newVal;
}



static void
getRestOfHeader(TConn *       const connectionP,
                char *        const lineEnd,
                time_t        const deadline,
                const char ** const headerEndP,
                bool *        const errorP) {
/*----------------------------------------------------------------------------
   Given that the read buffer for connection *connectionP contains (at
   its current read position) the first line of an HTTP header, which
   ends at position 'lineEnd', find the rest of it.

   Some or all of the rest of the header may be in the buffer already;
   we read more from the connection as necessary, but not if it takes past
   'deadline'.  In the latter case, we fail.

   We return the location of the end of the whole header as *headerEndP.
   We do not remove the header from the buffer, but we do modify the
   buffer so as to join the multiple lines of the header into a single
   line, and to NUL-terminate the header.
-----------------------------------------------------------------------------*/
    char * const headerStart = connectionP->buffer + connectionP->bufferpos;

    char * headerEnd;
        /* End of the header lines we've seen at so far */
    bool gotWholeHeader;
    bool error;

    headerEnd = lineEnd;  /* initial value - end of 1st line */
        
    for (gotWholeHeader = FALSE, error = FALSE;
         !gotWholeHeader && !error;) {

        char * nextLineEnd;

        /* Note that we are guaranteed, assuming the HTTP stream is
           valid, that there is at least one more line in it.  Worst
           case, it's the empty line that marks the end of the headers.
        */
        getLineInBuffer(connectionP, headerEnd, deadline,
                        &nextLineEnd, &error);
        if (!error) {
            if (isContinuationLine(headerEnd)) {
                /* Join previous line to this one */
                convertLineEnd(headerEnd, headerStart, ' ');
                /* Add this line to the header */
                headerEnd = nextLineEnd;
            } else {
                gotWholeHeader = TRUE;
                *headerEndP = headerEnd;

                /* NUL-terminate the whole header */
                convertLineEnd(headerEnd, headerStart, '\0');
            }
        }
    }
    *errorP = error;
}



static void
readHeader(TConn * const connectionP,
           time_t  const deadline,
           bool *  const endOfHeadersP,
           char ** const headerP,
           bool *  const errorP) {
/*----------------------------------------------------------------------------
   Read an HTTP header, or the end of headers empty line, on connection
   *connectionP.

   An HTTP header is basically a line, except that if a line starts
   with white space, it's a continuation of the previous line.  A line
   is delimited by either LF or CRLF.

   The first line of an HTTP header is never empty; an empty line signals
   the end of the HTTP headers and beginning of the HTTP body.  We call
   that empty line the EOH mark.

   We assume the connection is positioned to a header or EOH mark.
   
   In the course of reading, we read at least one character past the
   line delimiter at the end of the header or EOH mark; we may read
   much more.  But we leave everything after the header or EOH (and
   its line delimiter) in the internal buffer, with the buffer pointer
   pointing to it.

   We use stuff already in the internal buffer (perhaps left by a
   previous call to this subroutine) before reading any more from from
   the channel.

   We return as *headerP the next header as an ASCIIZ string, with no
   line delimiter.  That string is stored in the "unused" portion of
   the connection's internal buffer.  Iff there is no next header, we
   return *endOfHeadersP == true and nothing meaningful as *headerP.
-----------------------------------------------------------------------------*/
    char * const bufferStart = connectionP->buffer + connectionP->bufferpos;

    bool error;
    char * lineEnd;

    getLineInBuffer(connectionP, bufferStart, deadline, &lineEnd, &error);

    if (!error) {
        if (isContinuationLine(bufferStart))
            error = TRUE;
        else if (isEmptyLine(bufferStart)) {
            /* Consume the EOH mark from the buffer */
            connectionP->bufferpos = lineEnd - connectionP->buffer;
            *endOfHeadersP = TRUE;
        } else {
            /* We have the first line of a header; there may be more. */

            const char * headerEnd;

            *endOfHeadersP = FALSE;

            getRestOfHeader(connectionP, lineEnd, deadline,
                            &headerEnd, &error);

            if (!error) {
                *headerP = bufferStart;

                /* Consume the header from the buffer (but be careful --
                   you can't reuse that part of the buffer because the
                   string we will return is in it!
                */
                connectionP->bufferpos = headerEnd - connectionP->buffer;
            }
        }
    }
    *errorP = error;
}



static void
skipToNonemptyLine(TConn * const connectionP,
                   time_t  const deadline,
                   bool *  const errorP) {

    char * const bufferStart = connectionP->buffer + connectionP->bufferpos;

    bool gotNonEmptyLine;
    bool error;
    char * lineStart;
    
    lineStart       = bufferStart;  /* initial value */
    gotNonEmptyLine = FALSE;        /* initial value */
    error           = FALSE;        /* initial value */          

    while (!gotNonEmptyLine && !error) {
        char * lineEnd;

        getLineInBuffer(connectionP, lineStart, deadline, &lineEnd, &error);

        if (!error) {
            if (!isEmptyLine(lineStart))
                gotNonEmptyLine = TRUE;
            else
                lineStart = lineEnd;
        }
    }
    if (!error) {
        /* Consume all the empty lines; advance buffer pointer to first
           non-empty line.
        */
        connectionP->bufferpos = lineStart - connectionP->buffer;
    }
    *errorP = error;
}



static void
readRequestHeader(TSession * const sessionP,
                  time_t     const deadline,
                  char **    const requestLineP,
                  uint16_t * const httpErrorCodeP) {
/*----------------------------------------------------------------------------
   Read the HTTP request header (aka request header field) from
   session 'sessionP'.  We read through the session's internal buffer;
   i.e.  we may get data that was previously read from the network, or
   we may read more from the network.

   We assume the connection is presently positioned to the beginning of
   the HTTP document.  We leave it positioned after the request header.
   
   We ignore any empty lines at the beginning of the stream, per
   RFC2616 Section 4.1.

   Fail if we can't get the header before 'deadline'.

   Return as *requestLineP the request header read.  This ASCIIZ string is
   in the session's internal buffer.

   Return as *httpErrorCodeP the HTTP error code that describes how we
   are not able to read the request header, or 0 if we can.
   If we can't, *requestLineP is meaningless.
-----------------------------------------------------------------------------*/
    char * line = NULL;
    bool error = FALSE;
    bool endOfHeaders = FALSE;

    skipToNonemptyLine(sessionP->conn, deadline, &error);

    if (!error)
        readHeader(sessionP->conn, deadline, &endOfHeaders, &line, &error);

    if (error)
        *httpErrorCodeP = 408;  /* Request Timeout */
    else {
        *httpErrorCodeP = 0;
        *requestLineP = line;
    }
}



static void
unescapeUri(char * const uri,
            bool * const errorP) {

    char * x;
    char * y;

    x = y = uri;
    
    *errorP = FALSE;

    while (*x && !*errorP) {
        switch (*x) {
        case '%': {
            char c;
            ++x;
            c = tolower(*x++);
            if ((c >= '0') && (c <= '9'))
                c -= '0';
            else if ((c >= 'a') && (c <= 'f'))
                c -= 'a' - 10;
            else
                *errorP = TRUE;

            if (!*errorP) {
                char d;
                d = tolower(*x++);
                if ((d >= '0') && (d <= '9'))
                    d -= '0';
                else if ((d >= 'a') && (d <= 'f'))
                    d -= 'a' - 10;
                else
                    *errorP = TRUE;

                if (!*errorP)
                    *y++ = ((c << 4) | d);
            }
        } break;

        default:
            *y++ = *x++;
            break;
        }
    }
    *y = '\0';
}



static void
parseHostPort(const char *     const hostport,
              const char **    const hostP,
              unsigned short * const portP,
              uint16_t *       const httpErrorCodeP) {
/*----------------------------------------------------------------------------
   Parse a 'hostport', a string in the form www.acme.com:8080 .

   Return the host name part (www.acme.com) as *hostP (in newly
   malloced storage), and the port part (8080) as *portP.

   Default the port to 80 if 'hostport' doesn't have the port part.
-----------------------------------------------------------------------------*/
    char * buffer;
    char * colonPos;

    buffer = strdup(hostport);

    colonPos = strchr(buffer, ':');
    if (colonPos) {
        const char * p;
        uint32_t port;

        *colonPos = '\0';  /* Split hostport at the colon */

        for (p = colonPos + 1, port = 0;
             isdigit(*p) && port < 65535;
             (port = port * 10 + (*p - '0')), ++p);
            
        if (*p || port == 0)
            *httpErrorCodeP = 400;  /* Bad Request */
        else {
            *hostP = strdup(buffer);
            *portP = port;
            *httpErrorCodeP = 0;
        }
    } else {
        *hostP          = strdup(buffer);
        *portP          = 80;
        *httpErrorCodeP = 0;
    }
    free(buffer);
}



static void
parseRequestUri(char *           const requestUri,
                const char **    const hostP,
                unsigned short * const portP,
                const char **    const pathP,
                const char **    const queryP,
                uint16_t *       const httpErrorCodeP) {
/*----------------------------------------------------------------------------
  Parse the request URI (in the request line
  "GET http://www.myserver.com:8080/myfile.cgi?parm HTTP/1.1",
  "http://www.myserver.com:8080/myfile.cgi?parm" is the request URI).

  Return as *hostP the "www.myserver.com" in the above example.  If
  that part of the URI doesn't exist, return *hostP == NULL.

  Return as *portP the 8080 in the above example.  If it doesn't exist,
  return 80.

  Return as *pathP the "/myfile.cgi" in the above example.  If it
  doesn't exist, return "*".

  Return as *queryP the "parm" in the above example.  If it doesn't
  exist, return *queryP == NULL.

  Return strings in newly malloc'ed storage.

  Return as *httpErrorCodeP the HTTP error code that describes how the
  URI is invalid, or 0 if it is valid.  If it's invalid, other return
  values are meaningless.

  This destroys 'requestUri'.  We should fix that.
-----------------------------------------------------------------------------*/
    bool error;

    unescapeUri(requestUri, &error);
    
    if (error)
        *httpErrorCodeP = 400;  /* Bad Request */
    else {
        char * requestUriNoQuery;
           /* The request URI with any query (the stuff marked by a question
              mark at the end of a request URI) chopped off.
           */
        {
            /* Split requestUri at the question mark */
            char * const qmark = strchr(requestUri, '?');
            
            if (qmark) {
                *qmark = '\0';
                *queryP = strdup(qmark + 1);
            } else
                *queryP = NULL;

            requestUriNoQuery = requestUri;
        }

        if (requestUriNoQuery[0] == '/') {
            *hostP = NULL;
            *pathP = strdup(requestUriNoQuery);
            *portP = 80;
            *httpErrorCodeP = 0;
        } else {
            if (!xmlrpc_strneq(requestUriNoQuery, "http://", 7))
                *httpErrorCodeP = 400;  /* Bad Request */
            else {
                char * const hostportpath = &requestUriNoQuery[7];
                char * const slashPos = strchr(hostportpath, '/');

                const char * host;
                const char * path;
                unsigned short port;

                char * hostport;
                
                if (slashPos) {
                    char * p;
                    path = strdup(slashPos);
                    
                    /* Nul-terminate the host name.  To make space for
                       it, slide the whole name back one character.
                       This moves it into the space now occupied by
                       the end of "http://", which we don't need.
                    */
                    for (p = hostportpath; *p != '/'; ++p)
                        *(p-1) = *p;
                    *(p-1) = '\0';
                    
                    hostport = hostportpath - 1;
                    *httpErrorCodeP = 0;
                } else {
                    path = strdup("*");
                    hostport = hostportpath;
                    *httpErrorCodeP = 0;
                }
                if (!*httpErrorCodeP)
                    parseHostPort(hostport, &host, &port, httpErrorCodeP);
                if (*httpErrorCodeP)
                    xmlrpc_strfree(path);

                *hostP  = host;
                *portP  = port;
                *pathP  = path;
            }
        }
    }
}



static void
parseRequestLine(char *           const requestLine,
                 TMethod *        const httpMethodP,
                 httpVersion *    const httpVersionP,
                 const char **    const hostP,
                 unsigned short * const portP,
                 const char **    const pathP,
                 const char **    const queryP,
                 bool *           const moreLinesP,
                 uint16_t *       const httpErrorCodeP) {
/*----------------------------------------------------------------------------
   Modifies *requestLine!
-----------------------------------------------------------------------------*/
    const char * httpMethodName;
    char * p;

    p = requestLine;

    /* Jump over spaces */
    NextToken((const char **)&p);

    httpMethodName = GetToken(&p);
    if (!httpMethodName)
        *httpErrorCodeP = 400;  /* Bad Request */
    else {
        char * requestUri;

        if (xmlrpc_streq(httpMethodName, "GET"))
            *httpMethodP = m_get;
        else if (xmlrpc_streq(httpMethodName, "PUT"))
            *httpMethodP = m_put;
        else if (xmlrpc_streq(httpMethodName, "OPTIONS"))
            *httpMethodP = m_options;
        else if (xmlrpc_streq(httpMethodName, "DELETE"))
            *httpMethodP = m_delete;
        else if (xmlrpc_streq(httpMethodName, "POST"))
            *httpMethodP = m_post;
        else if (xmlrpc_streq(httpMethodName, "TRACE"))
            *httpMethodP = m_trace;
        else if (xmlrpc_streq(httpMethodName, "HEAD"))
            *httpMethodP = m_head;
        else
            *httpMethodP = m_unknown;
        
        /* URI and Query Decoding */
        NextToken((const char **)&p);

        requestUri = GetToken(&p);
        if (!requestUri)
            *httpErrorCodeP = 400;  /* Bad Request */
        else {
            const char * host;
            unsigned short port;
            const char * path;
            const char * query;

            parseRequestUri(requestUri, &host, &port, &path, &query,
                            httpErrorCodeP);

            if (!*httpErrorCodeP) {
                const char * httpVersion;

                NextToken((const char **)&p);
        
                /* HTTP Version Decoding */
                
                httpVersion = GetToken(&p);
                if (httpVersion) {
                    uint32_t vmin, vmaj;
                    if (sscanf(httpVersion, "HTTP/%d.%d", &vmaj, &vmin) != 2)
                        *httpErrorCodeP = 400;  /* Bad Request */
                    else {
                        httpVersionP->major = vmaj;
                        httpVersionP->minor = vmin;
                        *httpErrorCodeP = 0;  /* no error */
                    }
                    *moreLinesP = TRUE;
                } else {
                    /* There is no HTTP version, so this is a single
                       line request.
                    */
                    *httpErrorCodeP = 0;  /* no error */
                    *moreLinesP = FALSE;
                }
                if (*httpErrorCodeP) {
                    xmlrpc_strfree(host);
                    xmlrpc_strfree(path);
                    xmlrpc_strfree(query);
                }
                *hostP = host;
                *portP = port;
                *pathP = path;
                *queryP = query;
            }
        }
    }
}



static void
strtolower(char * const s) {

    char * t;

    t = &s[0];
    while (*t) {
        *t = tolower(*t);
        ++t;
    }
}



static void
getFieldNameToken(char **    const pP,
                  char **    const fieldNameP,
                  uint16_t * const httpErrorCodeP) {
/*----------------------------------------------------------------------------
   Assuming that *pP points to the place in an HTTP header where the field
   name belongs, return the field name and advance *pP past that token.

   The field name is the lower case representation of the value of the
   field name token.
-----------------------------------------------------------------------------*/
    char * fieldName;

    NextToken((const char **)pP);
    
    fieldName = GetToken(pP);
    if (!fieldName)
        *httpErrorCodeP = 400;  /* Bad Request */
    else {
        if (fieldName[strlen(fieldName)-1] != ':')
            /* Not a valid field name */
            *httpErrorCodeP = 400;  /* Bad Request */
        else {
            fieldName[strlen(fieldName)-1] = '\0';  /* remove trailing colon */

            strtolower(fieldName);
            
            *httpErrorCodeP = 0;  /* no error */
            *fieldNameP = fieldName;
        }
    }
}



static void
processHeader(const char * const fieldName,
              char *       const fieldValue,
              TSession *   const sessionP,
              uint16_t *   const httpErrorCodeP) {
/*----------------------------------------------------------------------------
   We may modify *fieldValue, and we put pointers to *fieldValue and
   *fieldName into *sessionP.

   We must fix this some day.  *sessionP should point to individual
   malloc'ed strings.
-----------------------------------------------------------------------------*/
    *httpErrorCodeP = 0;  /* initial assumption */

    if (xmlrpc_streq(fieldName, "connection")) {
        if (xmlrpc_strcaseeq(fieldValue, "keep-alive"))
            sessionP->requestInfo.keepalive = TRUE;
        else
            sessionP->requestInfo.keepalive = FALSE;
    } else if (xmlrpc_streq(fieldName, "host")) {
        if (sessionP->requestInfo.host) {
            xmlrpc_strfree(sessionP->requestInfo.host);
            sessionP->requestInfo.host = NULL;
        }
        parseHostPort(fieldValue, &sessionP->requestInfo.host,
                      &sessionP->requestInfo.port, httpErrorCodeP);
    } else if (xmlrpc_streq(fieldName, "from"))
        sessionP->requestInfo.from = fieldValue;
    else if (xmlrpc_streq(fieldName, "user-agent"))
        sessionP->requestInfo.useragent = fieldValue;
    else if (xmlrpc_streq(fieldName, "referer"))
        sessionP->requestInfo.referer = fieldValue;
    else if (xmlrpc_streq(fieldName, "range")) {
        if (xmlrpc_strneq(fieldValue, "bytes=", 6)) {
            bool succeeded;
            succeeded = ListAddFromString(&sessionP->ranges, &fieldValue[6]);
            *httpErrorCodeP = succeeded ? 0 : 400;
        }
    } else if (xmlrpc_streq(fieldName, "cookies")) {
        bool succeeded;
        succeeded = ListAddFromString(&sessionP->cookies, fieldValue);
        *httpErrorCodeP = succeeded ? 0 : 400;
    } else if (xmlrpc_streq(fieldName, "expect")) {
        if (xmlrpc_strcaseeq(fieldValue, "100-continue"))
            sessionP->continueRequired = TRUE;
    }
}



static void
readAndProcessHeaders(TSession * const sessionP,
                      time_t     const deadline,
                      uint16_t * const httpErrorCodeP) {
/*----------------------------------------------------------------------------
   Read all the HTTP headers from the session *sessionP, which has at
   least one header coming.  Update *sessionP to reflect the
   information in the headers.

   If we find an error in the headers or while trying to read them, we
   return an appropriate HTTP error code as *httpErrorCodeP.  Otherwise,
   we return *httpErrorCodeP = 0.
-----------------------------------------------------------------------------*/
    bool endOfHeaders;

    /* Calling us doesn't make sense if there is already a valid request */
    if (sessionP->validRequest) {
       return;
    }
	
    *httpErrorCodeP = 0;  /* initial assumption */
    endOfHeaders = false;  /* Caller assures us there is at least one header */

    while (!endOfHeaders && !*httpErrorCodeP) {
        char * header;
        bool error;
        readHeader(sessionP->conn, deadline, &endOfHeaders, &header, &error);
        if (error)
            *httpErrorCodeP = 408;  /* Request Timeout */
        else {
            if (!endOfHeaders) {
                char * p;
                char * fieldName;

                p = &header[0];
                getFieldNameToken(&p, &fieldName, httpErrorCodeP);
                if (!*httpErrorCodeP) {
                    char * fieldValue;
                    
                    NextToken((const char **)&p);
                    
                    fieldValue = p;

                    TableAdd(&sessionP->request_headers,
                             fieldName, fieldValue);
                    
                    processHeader(fieldName, fieldValue, sessionP,
                                  httpErrorCodeP);
                }
            }
        }
    }
}



void
RequestRead(TSession * const sessionP,
            uint32_t   const timeout) {
/*----------------------------------------------------------------------------
   Read the headers of a new HTTP request (assuming nothing has yet been
   read on the session).

   Update *sessionP with the information from the headers.

   Leave the connection positioned to the body of the request, ready
   to be read by an HTTP request handler (via SessionRefillBuffer() and
   SessionGetReadData()).
-----------------------------------------------------------------------------*/
    time_t const deadline = time(NULL) + timeout;

    uint16_t httpErrorCode;  /* zero for no error */
    char * requestLine;  /* In connection;s internal buffer */

    readRequestHeader(sessionP, deadline, &requestLine, &httpErrorCode);
    if (!httpErrorCode) {
        TMethod httpMethod;
        const char * host;
        const char * path;
        const char * query;
        unsigned short port;
        bool moreHeaders;

        parseRequestLine(requestLine, &httpMethod, &sessionP->version,
                         &host, &port, &path, &query,
                         &moreHeaders, &httpErrorCode);

        if (!httpErrorCode) {
            initRequestInfo(&sessionP->requestInfo, sessionP->version,
                            requestLine,
                            httpMethod, host, port, path, query);

            if (moreHeaders)
                readAndProcessHeaders(sessionP, deadline, &httpErrorCode);

            if (httpErrorCode == 0)
                sessionP->validRequest = true;

            xmlrpc_strfreenull(host);
            xmlrpc_strfree(path);
            xmlrpc_strfreenull(query);
        }
    }
    if (httpErrorCode)
        ResponseStatus(sessionP, httpErrorCode);
}



char *
RequestHeaderValue(TSession *   const sessionP,
                   const char * const name) {

    return (TableFind(&sessionP->request_headers, name));
}



bool
RequestValidURI(TSession * const sessionP) {

    if (!sessionP->requestInfo.uri)
        return FALSE;
    
    if (xmlrpc_streq(sessionP->requestInfo.uri, "*"))
        return (sessionP->requestInfo.method != m_options);

    if (strchr(sessionP->requestInfo.uri, '*'))
        return FALSE;

    return TRUE;
}



bool
RequestValidURIPath(TSession * const sessionP) {

    uint32_t i;
    const char * p;

    p = sessionP->requestInfo.uri;

    i = 0;

    if (*p == '/') {
        i = 1;
        while (*p)
            if (*(p++) == '/') {
                if (*p == '/')
                    break;
                else if ((strncmp(p,"./",2) == 0) || (strcmp(p, ".") == 0))
                    ++p;
                else if ((strncmp(p, "../", 2) == 0) ||
                         (strcmp(p, "..") == 0)) {
                    p += 2;
                    --i;
                    if (i == 0)
                        break;
                }
                /* Prevent accessing hidden files (starting with .) */
                else if (*p == '.')
                    return FALSE;
                else
                    if (*p)
                        ++i;
            }
    }
    return (*p == 0 && i > 0);
}



bool
RequestAuth(TSession *   const sessionP,
            const char * const credential,
            const char * const user,
            const char * const pass) {
/*----------------------------------------------------------------------------
   Authenticate requester, in a very simplistic fashion.

   If the request specifies basic authentication (via Authorization
   header) with username 'user', password 'pass', then return TRUE.
   Else, return FALSE and set up an authorization failure response
   (HTTP response status 401) that says user must supply an identity
   in the 'credential' domain.

   When we return TRUE, we also set the username in the request info
   to 'user' so that a future SessionGetRequestInfo can get it.
-----------------------------------------------------------------------------*/
    bool authorized;
    char * authHdrPtr;

    authHdrPtr = RequestHeaderValue(sessionP, "authorization");
    if (authHdrPtr) {
        const char * authType;
        NextToken((const char **)&authHdrPtr);
        GetTokenConst(&authHdrPtr, &authType);
        authType = GetToken(&authHdrPtr);
        if (authType) {
            if (xmlrpc_strcaseeq(authType, "basic")) {
                const char * userPass;
                char userPassEncoded[80];

                NextToken((const char **)&authHdrPtr);

                xmlrpc_asprintf(&userPass, "%s:%s", user, pass);
                Base64Encode(userPass, userPassEncoded);
                xmlrpc_strfree(userPass);

                if (xmlrpc_streq(authHdrPtr, userPassEncoded)) {
                    sessionP->requestInfo.user = strdup(user);
                    authorized = TRUE;
                } else
                    authorized = FALSE;
            } else
                authorized = FALSE;
        } else
            authorized = FALSE;
    } else
        authorized = FALSE;

    if (!authorized) {
        const char * hdrValue;
        xmlrpc_asprintf(&hdrValue, "Basic realm=\"%s\"", credential);
        ResponseAddField(sessionP, "WWW-Authenticate", hdrValue);

        xmlrpc_strfree(hdrValue);

        ResponseStatus(sessionP, 401);
    }
    return authorized;
}



/*********************************************************************
** Range
*********************************************************************/

abyss_bool
RangeDecode(char *            const strArg,
            xmlrpc_uint64_t   const filesize,
            xmlrpc_uint64_t * const start,
            xmlrpc_uint64_t * const end) {

    char *str;
    char *ss;

    str = strArg;  /* initial value */

    *start=0;
    *end=filesize-1;

    if (*str=='-')
    {
        *start=filesize-strtol(str+1,&ss,10);
        return ((ss!=str) && (!*ss));
    };

    *start=strtol(str,&ss,10);

    if ((ss==str) || (*ss!='-'))
        return FALSE;

    str=ss+1;

    if (!*str)
        return TRUE;

    *end=strtol(str,&ss,10);

    if ((ss==str) || (*ss) || (*end<*start))
        return FALSE;

    return TRUE;
}

/*********************************************************************
** HTTP
*********************************************************************/

const char *
HTTPReasonByStatus(uint16_t const code) {

    struct _HTTPReasons {
        uint16_t status;
        const char * reason;
    };

    static struct _HTTPReasons const reasons[] =  {
        { 100,"Continue" }, 
        { 101,"Switching Protocols" }, 
        { 200,"OK" }, 
        { 201,"Created" }, 
        { 202,"Accepted" }, 
        { 203,"Non-Authoritative Information" }, 
        { 204,"No Content" }, 
        { 205,"Reset Content" }, 
        { 206,"Partial Content" }, 
        { 300,"Multiple Choices" }, 
        { 301,"Moved Permanently" }, 
        { 302,"Moved Temporarily" }, 
        { 303,"See Other" }, 
        { 304,"Not Modified" }, 
        { 305,"Use Proxy" }, 
        { 400,"Bad Request" }, 
        { 401,"Unauthorized" }, 
        { 402,"Payment Required" }, 
        { 403,"Forbidden" }, 
        { 404,"Not Found" }, 
        { 405,"Method Not Allowed" }, 
        { 406,"Not Acceptable" }, 
        { 407,"Proxy Authentication Required" }, 
        { 408,"Request Timeout" }, 
        { 409,"Conflict" }, 
        { 410,"Gone" }, 
        { 411,"Length Required" }, 
        { 412,"Precondition Failed" }, 
        { 413,"Request Entity Too Large" }, 
        { 414,"Request-URI Too Long" }, 
        { 415,"Unsupported Media Type" }, 
        { 500,"Internal Server Error" }, 
        { 501,"Not Implemented" }, 
        { 502,"Bad Gateway" }, 
        { 503,"Service Unavailable" }, 
        { 504,"Gateway Timeout" }, 
        { 505,"HTTP Version Not Supported" },
        { 000, NULL }
    };
    const struct _HTTPReasons * reasonP;

    reasonP = &reasons[0];

    while (reasonP->status <= code)
        if (reasonP->status == code)
            return reasonP->reason;
        else
            ++reasonP;

    return "No Reason";
}



int32_t
HTTPRead(TSession *   const s ATTR_UNUSED,
         const char * const buffer ATTR_UNUSED,
         uint32_t     const len ATTR_UNUSED) {

    return 0;
}



bool
HTTPWriteBodyChunk(TSession *   const sessionP,
                   const char * const buffer,
                   uint32_t     const len) {

    bool succeeded;

    if (sessionP->chunkedwrite && sessionP->chunkedwritemode) {
        char chunkHeader[16];

        sprintf(chunkHeader, "%x\r\n", len);

        succeeded =
            ConnWrite(sessionP->conn, chunkHeader, strlen(chunkHeader));
        if (succeeded) {
            succeeded = ConnWrite(sessionP->conn, buffer, len);
            if (succeeded)
                succeeded = ConnWrite(sessionP->conn, "\r\n", 2);
        }
    } else
        succeeded = ConnWrite(sessionP->conn, buffer, len);

    return succeeded;
}



bool
HTTPWriteEndChunk(TSession * const sessionP) {

    bool retval;

    if (sessionP->chunkedwritemode && sessionP->chunkedwrite) {
        /* May be one day trailer dumping will be added */
        sessionP->chunkedwritemode = FALSE;
        retval = ConnWrite(sessionP->conn, "0\r\n\r\n", 5);
    } else
        retval = TRUE;

    return retval;
}



bool
HTTPKeepalive(TSession * const sessionP) {
/*----------------------------------------------------------------------------
   Return value: the connection should be kept alive after the session
   *sessionP is over.
-----------------------------------------------------------------------------*/
    return (sessionP->requestInfo.keepalive &&
            !sessionP->serverDeniesKeepalive &&
            sessionP->status < 400);
}



bool
HTTPWriteContinue(TSession * const sessionP) {

    char const continueStatus[] = "HTTP/1.1 100 continue\r\n\r\n";
        /* This is a status line plus an end-of-headers empty line */

    return ConnWrite(sessionP->conn, continueStatus, strlen(continueStatus));
}



/******************************************************************************
**
** http.c
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
