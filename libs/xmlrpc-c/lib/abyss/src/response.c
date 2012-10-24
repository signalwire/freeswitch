/*=============================================================================
                             response
===============================================================================
  This module contains callbacks from and services for a request handler.

  Copyright information is at the end of the file
=============================================================================*/

#define _XOPEN_SOURCE 600  /* Make sure strdup() is in <string.h> */

#include <ctype.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#include "xmlrpc_config.h"
#include "bool.h"
#include "int.h"
#include "version.h"
#include "mallocvar.h"
#include "xmlrpc-c/string_int.h"
#include "xmlrpc-c/abyss.h"

#include "trace.h"
#include "server.h"
#include "session.h"
#include "file.h"
#include "conn.h"
#include "token.h"
#include "date.h"
#include "data.h"
#include "abyss_info.h"
#include "http.h"



void
ResponseError2(TSession *   const sessionP,
               const char * const explanation) {

    const char * errorDocument;

    xmlrpc_asprintf(&errorDocument,
                    "<HTML><HEAD><TITLE>Error %d</TITLE></HEAD>"
                    "<BODY>"
                    "<H1>Error %d</H1>"
                    "<P>%s</P>" SERVER_HTML_INFO 
                    "</BODY>"
                    "</HTML>",
                    sessionP->status, sessionP->status, explanation);

	ResponseAddField(sessionP, "Content-type", "text/html");
    ResponseContentLength(sessionP, strlen(errorDocument));

	if (ResponseWriteStart(sessionP))
        ConnWrite(sessionP->connP, errorDocument, strlen(errorDocument)); 

    xmlrpc_strfree(errorDocument);
}



void
ResponseError(TSession * const sessionP) {

    ResponseError2(sessionP, HTTPReasonByStatus(sessionP->status));
}




abyss_bool
ResponseChunked(TSession * const sessionP) {
    /* This is only a hope, things will be real only after a call of
       ResponseWriteStart()
    */
    assert(!sessionP->responseStarted);

    sessionP->chunkedwrite =
        (sessionP->version.major > 1) ||
        (sessionP->version.major == 1 && (sessionP->version.minor >= 1));

    sessionP->chunkedwritemode = TRUE;

    return TRUE;
}



void
ResponseStatus(TSession *     const sessionP,
               unsigned short const code) {

    sessionP->status = code;
}



xmlrpc_uint16_t
ResponseStatusFromErrno(int const errnoArg) {

    uint16_t code;

    switch (errnoArg) {
    case EACCES:
        code=403;
        break;
    case ENOENT:
        code=404;
        break;
    default:
        code=500;
    }
    return code;
}



void
ResponseStatusErrno(TSession * const sessionP) {

    ResponseStatus(sessionP, ResponseStatusFromErrno(errno));
}



static bool
isValidHttpToken(const char * const token) {

    char const separators[] = "()<>@,;:\\\"/[]?={} \t";
    const char * p;
    bool valid;

    for (p = &token[0], valid = true; *p; ++p) {
        if (!isprint(*p) || strchr(separators, *p))
            valid = false;
    }
    return valid;
}




static bool
isValidHttpText(const char * const text) {

    const char * p;
    bool valid;

    for (p = &text[0], valid = true; *p; ++p) {
        if (!isprint(*p))
            valid = false;
    }
    return valid;
}



abyss_bool
ResponseAddField(TSession *   const sessionP,
                 const char * const name,
                 const char * const value) {

    abyss_bool succeeded;
    
    if (!isValidHttpToken(name)) {
        TraceMsg("Supplied HTTP header field name is not a valid HTTP token");
        succeeded = false;
    } else if (!isValidHttpText(value)) {
        TraceMsg("Supplied HTTP header field value is not valid HTTP text");
        succeeded = false;
    } else {
        succeeded = TableAdd(&sessionP->responseHeaderFields, name, value);
    }
    return succeeded;
}



static void
addConnectionHeaderFld(TSession * const sessionP) {

    struct _TServer * const srvP = ConnServer(sessionP->connP)->srvP;

    if (HTTPKeepalive(sessionP)) {
        const char * keepaliveValue;
        
        ResponseAddField(sessionP, "Connection", "Keep-Alive");

        xmlrpc_asprintf(&keepaliveValue, "timeout=%u, max=%u",
                        srvP->keepalivetimeout, srvP->keepalivemaxconn);

        ResponseAddField(sessionP, "Keep-Alive", keepaliveValue);

        xmlrpc_strfree(keepaliveValue);
    } else
        ResponseAddField(sessionP, "Connection", "close");
}
    


static void
addDateHeaderFld(TSession * const sessionP) {

    if (sessionP->status >= 200) {
        const char * dateValue;

        DateToString(sessionP->date, &dateValue);

        if (dateValue) {
            ResponseAddField(sessionP, "Date", dateValue);
            xmlrpc_strfree(dateValue);
        }
    }
}



static void
addServerHeaderFld(TSession * const sessionP) {

    const char * serverValue;

    xmlrpc_asprintf(&serverValue, "Freeswitch xmlrpc-c_abyss /%s", XMLRPC_C_VERSION);

    ResponseAddField(sessionP, "Server", serverValue);

    xmlrpc_strfree(serverValue);
}



static unsigned int
leadingWsCt(const char * const arg) {

    unsigned int i;

    for (i = 0; arg[i] && isspace(arg[i]); ++i);

    return i;
}



static unsigned int
trailingWsPos(const char * const arg) {

    unsigned int i;

    for (i = strlen(arg); i > 0 && isspace(arg[i-1]); --i);

    return i;
}



static const char *
formatFieldValue(const char * const unformatted) {
/*----------------------------------------------------------------------------
   Return the string of characters that goes after the colon on the
   HTTP header field line, given that 'unformatted' is its basic value.
-----------------------------------------------------------------------------*/
    const char * retval;

    /* An HTTP header field value may not have leading or trailing white
       space.
    */
    char * buffer;

    buffer = malloc(strlen(unformatted) + 1);

    if (buffer == NULL)
        retval = xmlrpc_strnomemval();
    else {
        unsigned int const lead  = leadingWsCt(unformatted);
        unsigned int const trail = trailingWsPos(unformatted);
        assert(trail >= lead);
        strncpy(buffer, &unformatted[lead], trail - lead);
        buffer[trail - lead] = '\0';
        retval = buffer;
    }
    return retval;
}



static abyss_bool
sendHeader(TConn * const connP,
           TTable  const fields) {
/*----------------------------------------------------------------------------
   Send the HTTP response header whose fields are fields[].

   Don't include the blank line that separates the header from the body.

   fields[] contains syntactically valid HTTP header field names and values.
   But to the extent that int contains undefined field names or semantically
   invalid values, the header we send is invalid.
-----------------------------------------------------------------------------*/
    unsigned int i;
	abyss_bool ret = TRUE;
    for (i = 0; i < fields.size && ret; ++i) {
        TTableItem * const fieldP = &fields.item[i];
        const char * const fieldValue = formatFieldValue(fieldP->value);

        const char * line;

        xmlrpc_asprintf(&line, "%s: %s\r\n", fieldP->name, fieldValue);
        if (!ConnWrite(connP, line, strlen(line)))
			ret = FALSE;
        xmlrpc_strfree(line);
        xmlrpc_strfree(fieldValue);
    }
	return ret;
}


abyss_bool
ResponseWriteStart(TSession * const sessionP) {
/*----------------------------------------------------------------------------
   Begin the process of sending the response for an HTTP transaction
   (i.e. Abyss session).

   As part of this, send the entire HTTP header for the response.
-----------------------------------------------------------------------------*/
    struct _TServer * const srvP = ConnServer(sessionP->connP)->srvP;

    //assert(!sessionP->responseStarted);

	if (sessionP->responseStarted) {
		TraceMsg("Abyss client called ResponseWriteStart() more than once\n");
		return FALSE;
	}

    if (sessionP->status == 0) {
        /* Handler hasn't set status.  That's an error */
        TraceMsg("Abyss client called ResponseWriteStart() on "
                 "a session for which he has not set the request status "
                 "('status' member of TSession).  Using status 500\n");
        sessionP->status = 500;
    }

    sessionP->responseStarted = TRUE;

    {
        const char * const reason = HTTPReasonByStatus(sessionP->status);
        const char * line;
		abyss_bool ret = TRUE;
        xmlrpc_asprintf(&line,"HTTP/1.1 %u %s\r\n", sessionP->status, reason);
        ret = ConnWrite(sessionP->connP, line, strlen(line));
        xmlrpc_strfree(line);
		if (!ret) return FALSE;
    }


    addConnectionHeaderFld(sessionP);

    if (sessionP->chunkedwrite && sessionP->chunkedwritemode)
        ResponseAddField(sessionP, "Transfer-Encoding", "chunked");

    addDateHeaderFld(sessionP);

    if (srvP->advertise)
        addServerHeaderFld(sessionP);

    /* Note that sessionP->responseHeaderFields is defined to contain
       syntactically but not necessarily semantically valid header
       field names and values.
    */
    if (sendHeader(sessionP->connP, sessionP->responseHeaderFields))
		if (ConnWrite(sessionP->connP, "\r\n", 2))
			return TRUE;

	return FALSE;
}



abyss_bool
ResponseWriteBody(TSession *      const sessionP,
                  const char *    const data,
                  xmlrpc_uint32_t const len) {

    return HTTPWriteBodyChunk(sessionP, data, len);
}



abyss_bool
ResponseWriteEnd(TSession * const sessionP) {

    return HTTPWriteEndChunk(sessionP);
}



abyss_bool
ResponseContentType(TSession *   const serverP,
                    const char * const type) {

    return ResponseAddField(serverP, "Content-type", type);
}



abyss_bool
ResponseContentLength(TSession *      const sessionP,
                      xmlrpc_uint64_t const len) {

    char contentLengthValue[32];
    
    sprintf(contentLengthValue, "%" PRIu64, len);

    return ResponseAddField(sessionP, "Content-length", contentLengthValue);
}



void
ResponseAccessControl(TSession *        const abyssSessionP, 
                      ResponseAccessCtl const accessControl) {

    if (accessControl.allowOrigin) {
        ResponseAddField(abyssSessionP, "Access-Control-Allow-Origin",
                         accessControl.allowOrigin);
        ResponseAddField(abyssSessionP, "Access-Control-Allow-Methods",
                         "POST");
        if (accessControl.expires) {
            char buffer[64];
            sprintf(buffer, "%u", accessControl.maxAge);
            ResponseAddField(abyssSessionP, "Access-Control-Max-Age", buffer);
        }
    }
}



/*********************************************************************
** MIMEType
*********************************************************************/

struct MIMEType {
    TList typeList;
    TList extList;
    TPool pool;
};


static MIMEType * globalMimeTypeP = NULL;



MIMEType *
MIMETypeCreate(void) {
 
    MIMEType * MIMETypeP;

    MALLOCVAR(MIMETypeP);

    if (MIMETypeP) {
        ListInit(&MIMETypeP->typeList);
        ListInit(&MIMETypeP->extList);
        PoolCreate(&MIMETypeP->pool, 1024);
    }
    return MIMETypeP;
}



void
MIMETypeDestroy(MIMEType * const MIMETypeP) {

    PoolFree(&MIMETypeP->pool);

    free(MIMETypeP);
}



void
MIMETypeInit(void) {

    if (globalMimeTypeP != NULL)
        abort();

    globalMimeTypeP = MIMETypeCreate();
}



void
MIMETypeTerm(void) {

    if (globalMimeTypeP == NULL)
        abort();

	ListFree(&globalMimeTypeP->extList);
	ListFree(&globalMimeTypeP->typeList);

    MIMETypeDestroy(globalMimeTypeP);

    globalMimeTypeP = NULL;
}



static void
mimeTypeAdd(MIMEType *   const MIMETypeP,
            const char * const type,
            const char * const ext,
            bool *       const successP) {
    
    uint16_t index;
    void * mimeTypesItem;
    bool typeIsInList;

    assert(MIMETypeP != NULL);

    typeIsInList = ListFindString(&MIMETypeP->typeList, type, &index);
    if (typeIsInList)
        mimeTypesItem = MIMETypeP->typeList.item[index];
    else
        mimeTypesItem = (void*)PoolStrdup(&MIMETypeP->pool, type);

    if (mimeTypesItem) {
        bool extIsInList;
        extIsInList = ListFindString(&MIMETypeP->extList, ext, &index);
        if (extIsInList) {
            MIMETypeP->typeList.item[index] = mimeTypesItem;
            *successP = TRUE;
        } else {
            void * extItem = (void*)PoolStrdup(&MIMETypeP->pool, ext);
            if (extItem) {
                bool addedToMimeTypes;

                addedToMimeTypes =
                    ListAdd(&MIMETypeP->typeList, mimeTypesItem);
                if (addedToMimeTypes) {
                    bool addedToExt;
                    
                    addedToExt = ListAdd(&MIMETypeP->extList, extItem);
                    *successP = addedToExt;
                    if (!*successP)
                        ListRemove(&MIMETypeP->typeList);
                } else
                    *successP = FALSE;
                if (!*successP)
                    PoolReturn(&MIMETypeP->pool, extItem);
            } else
                *successP = FALSE;
        }
    } else
        *successP = FALSE;
}




abyss_bool
MIMETypeAdd2(MIMEType *   const MIMETypeArg,
             const char * const type,
             const char * const ext) {

    MIMEType * MIMETypeP = MIMETypeArg ? MIMETypeArg : globalMimeTypeP;

    bool success;

    if (MIMETypeP == NULL)
        success = FALSE;
    else 
        mimeTypeAdd(MIMETypeP, type, ext, &success);

    return success;
}



abyss_bool
MIMETypeAdd(const char * const type,
            const char * const ext) {

    return MIMETypeAdd2(globalMimeTypeP, type, ext);
}



static const char *
mimeTypeFromExt(MIMEType *   const MIMETypeP,
                const char * const ext) {

    const char * retval;
    uint16_t extindex;
    bool extIsInList;

    assert(MIMETypeP != NULL);

    extIsInList = ListFindString(&MIMETypeP->extList, ext, &extindex);
    if (!extIsInList)
        retval = NULL;
    else
        retval = MIMETypeP->typeList.item[extindex];
    
    return retval;
}



const char *
MIMETypeFromExt2(MIMEType *   const MIMETypeArg,
                 const char * const ext) {

    const char * retval;

    MIMEType * MIMETypeP = MIMETypeArg ? MIMETypeArg : globalMimeTypeP;

    if (MIMETypeP == NULL)
        retval = NULL;
    else
        retval = mimeTypeFromExt(MIMETypeP, ext);

    return retval;
}



const char *
MIMETypeFromExt(const char * const ext) {

    return MIMETypeFromExt2(globalMimeTypeP, ext);
}



static void
findExtension(const char *  const fileName,
              const char ** const extP) {

    unsigned int extPos = 0;  /* stifle unset variable warning */
        /* Running estimation of where in fileName[] the extension starts */
    bool extFound;
    unsigned int i;

    /* We're looking for the last dot after the last slash */
    for (i = 0, extFound = FALSE; fileName[i]; ++i) {
        char const c = fileName[i];
        
        if (c == '.') {
            extFound = TRUE;
            extPos = i + 1;
        }
        if (c == '/')
            extFound = FALSE;
    }

    if (extFound)
        *extP = &fileName[extPos];
    else
        *extP = NULL;
}



static const char *
mimeTypeFromFileName(MIMEType *   const MIMETypeP,
                     const char * const fileName) {

    const char * retval;
    const char * ext;

    assert(MIMETypeP != NULL);
    
    findExtension(fileName, &ext);

    if (ext)
        retval = MIMETypeFromExt2(MIMETypeP, ext);
    else
        retval = "application/octet-stream";

    return retval;
}



const char *
MIMETypeFromFileName2(MIMEType *   const MIMETypeArg,
                      const char * const fileName) {

    const char * retval;
    
    MIMEType * MIMETypeP = MIMETypeArg ? MIMETypeArg : globalMimeTypeP;

    if (MIMETypeP == NULL)
        retval = NULL;
    else
        retval = mimeTypeFromFileName(MIMETypeP, fileName);

    return retval;
}



const char *
MIMETypeFromFileName(const char * const fileName) {

    return MIMETypeFromFileName2(globalMimeTypeP, fileName);
}



static bool
fileContainsText(const char * const fileName) {
/*----------------------------------------------------------------------------
   Return true iff we can read the contents of the file named 'fileName'
   and see that it appears to be composed of plain text characters.
-----------------------------------------------------------------------------*/
    bool retval;
    bool fileOpened;
    TFile * fileP;

    fileOpened = FileOpen(&fileP, fileName, O_BINARY | O_RDONLY);
    if (fileOpened) {
        char const ctlZ = 26;
        unsigned char buffer[80];
        int32_t readRc;
        unsigned int i;

        readRc = FileRead(fileP, buffer, sizeof(buffer));
       
        if (readRc >= 0) {
            unsigned int bytesRead = readRc;
            bool nonTextFound;

            nonTextFound = FALSE;  /* initial value */
    
            for (i = 0; i < bytesRead; ++i) {
                char const c = buffer[i];
                if (c < ' ' && !isspace(c) && c != ctlZ)
                    nonTextFound = TRUE;
            }
            retval = !nonTextFound;
        } else
            retval = FALSE;
        FileClose(fileP);
    } else
        retval = FALSE;

    return retval;
}


 
static const char *
mimeTypeGuessFromFile(MIMEType *   const MIMETypeP,
                      const char * const fileName) {

    const char * retval;
    const char * ext;

    findExtension(fileName, &ext);

    retval = NULL;

    if (ext && MIMETypeP)
        retval = MIMETypeFromExt2(MIMETypeP, ext);


    if (!retval) {
        if (fileContainsText(fileName))
            retval = "text/plain";
        else
            retval = "application/octet-stream";  
    }

	if (!strcmp(retval, "text/plain"))
		retval = "text/plain; charset=utf-8";
    return retval;
}



const char *
MIMETypeGuessFromFile2(MIMEType *   const MIMETypeArg,
                       const char * const fileName) {

    return mimeTypeGuessFromFile(MIMETypeArg ? MIMETypeArg : globalMimeTypeP,
                                 fileName);
}



const char *
MIMETypeGuessFromFile(const char * const fileName) {

    return mimeTypeGuessFromFile(globalMimeTypeP, fileName);
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
