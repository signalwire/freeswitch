/*=============================================================================
                              handler.c
===============================================================================
   This file contains built-in HTTP request handlers.

   Copyright information is at end of file
=============================================================================*/
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#ifdef WIN32
  #include <io.h>
#else
  #include <unistd.h>
#endif
#include <fcntl.h>

#include "xmlrpc_config.h"
#include "bool.h"
#include "int.h"
#include "girmath.h"
#include "mallocvar.h"
#include "xmlrpc-c/string_int.h"
#include "xmlrpc-c/time_int.h"

#include "xmlrpc-c/abyss.h"
#include "trace.h"
#include "session.h"
#include "file.h"
#include "conn.h"
#include "http.h"
#include "date.h"
#include "abyss_info.h"

#include "handler.h"



struct BIHandler {
    const char * filesPath;
    TList defaultFileNames;
    MIMEType * mimeTypeP;
        /* NULL means to use the global MIMEType object */
};



BIHandler *
HandlerCreate(void) {

    struct BIHandler * handlerP;

    MALLOCVAR(handlerP);

    if (handlerP) {
        handlerP->filesPath = strdup(DEFAULT_DOCS);
        ListInitAutoFree(&handlerP->defaultFileNames);
        handlerP->mimeTypeP = NULL;
    }
    return handlerP;
}



void
HandlerDestroy(BIHandler * const handlerP) {

    ListFree(&handlerP->defaultFileNames);

    xmlrpc_strfree(handlerP->filesPath);

    free(handlerP);
}



void
HandlerSetMimeType(BIHandler * const handlerP,
                   MIMEType *  const mimeTypeP) {

    handlerP->mimeTypeP = mimeTypeP;
}



void
HandlerSetFilesPath(BIHandler *  const handlerP,
                    const char * const filesPath) {

    xmlrpc_strfree(handlerP->filesPath);
    handlerP->filesPath = strdup(filesPath);
}



void
HandlerAddDefaultFN(BIHandler *  const handlerP,
                    const char * const fileName) {

    ListAdd(&handlerP->defaultFileNames, strdup(fileName));
}



typedef int (*TQSortProc)(const void *, const void *);

static int
cmpfilenames(const TFileInfo **f1,const TFileInfo **f2) {
    if (((*f1)->attrib & A_SUBDIR) && !((*f2)->attrib & A_SUBDIR))
        return (-1);
    if (!((*f1)->attrib & A_SUBDIR) && ((*f2)->attrib & A_SUBDIR))
        return 1;

    return strcmp((*f1)->name,(*f2)->name);
}



static int
cmpfiledates(const TFileInfo ** const f1PP,
             const TFileInfo ** const f2PP) {

    const TFileInfo * const f1P = *f1PP;
    const TFileInfo * const f2P = *f2PP;

    int retval;

    if ((f1P->attrib & A_SUBDIR) && !(f2P->attrib & A_SUBDIR))
        retval = -1;
    else if (!(f1P->attrib & A_SUBDIR) && (f2P->attrib & A_SUBDIR))
        retval = 1;
    else {
        assert((int)(f1P->time_write - f2P->time_write) == 
               (f1P->time_write - f2P->time_write));
        retval = (int)(f1P->time_write - f2P->time_write);
    }
    return retval;
}



static void
determineSortType(const char *  const query,
                  bool *        const ascendingP,
                  uint16_t *    const sortP,
                  bool *        const textP,
                  const char ** const errorP) {

    *ascendingP = TRUE;
    *sortP = 1;
    *textP = FALSE;
    *errorP = NULL;
    
    if (query) {
        if (xmlrpc_streq(query, "plain"))
            *textP = TRUE;
        else if (xmlrpc_streq(query, "name-up")) {
            *sortP = 1;
            *ascendingP = TRUE;
        } else if (xmlrpc_streq(query, "name-down")) {
            *sortP = 1;
            *ascendingP = FALSE;
        } else if (xmlrpc_streq(query, "date-up")) {
            *sortP = 2;
            *ascendingP = TRUE;
        } else if (xmlrpc_streq(query, "date-down")) {
            *sortP = 2;
            *ascendingP = FALSE;
        } else  {
            xmlrpc_asprintf(errorP, "invalid query value '%s'", query);
        }
    }
}



static void
generateListing(TList *       const listP,
                const char *  const dirName,
                const char *  const uri,
                TPool *       const poolP,
                const char ** const errorP,
                uint16_t *    const responseStatusP) {
    
    TFileInfo fileinfo;
    TFileFind * findhandleP;

    *errorP = NULL;

    if (!FileFindFirst(&findhandleP, dirName, &fileinfo)) {
        *responseStatusP = ResponseStatusFromErrno(errno);
        xmlrpc_asprintf(errorP, "Can't read first entry in directory");
    } else {
        ListInit(listP);

        do {
            TFileInfo * fi;
            /* Files whose names start with a dot are ignored */
            /* This includes implicitly the ./ and ../ */
            if (*fileinfo.name == '.') {
                if (xmlrpc_streq(fileinfo.name, "..")) {
                    if (xmlrpc_streq(uri, "/"))
                        continue;
                } else
                    continue;
            }
            fi = (TFileInfo *)PoolAlloc(poolP, sizeof(fileinfo));
            if (fi) {
                bool success;
                memcpy(fi, &fileinfo, sizeof(fileinfo));
                success =  ListAdd(listP, fi);
                if (!success)
                    xmlrpc_asprintf(errorP, "ListAdd() failed");
            } else
                xmlrpc_asprintf(errorP, "PoolAlloc() failed.");
        } while (!*errorP && FileFindNext(findhandleP, &fileinfo));

        if (*errorP) {
            *responseStatusP = 500;
            ListFree(listP);
        }            
        FileFindClose(findhandleP);
    }
}



static void
sendDirectoryDocument(TList *      const listP,
                      bool         const ascending,
                      uint16_t     const sort,
                      bool         const text,
                      const char * const uri,
                      MIMEType *   const mimeTypeP,
                      TSession *   const sessionP) {

    char z[4096];
    char *p,z1[26],z2[20],z3[9],u;
    const char * z4;
    int16_t i;
    uint32_t k;

    if (text) {
        sprintf(z, "Index of %s" CRLF, uri);
        i = strlen(z)-2;
        p = z + i + 2;

        while (i > 0) {
            *(p++) = '-';
            --i;
        }

        *p = '\0';
        strcat(z, CRLF CRLF
               "Name                      Size      "
               "Date-Time             Type" CRLF
               "------------------------------------"
               "--------------------------------------------"CRLF);
    } else {
        sprintf(z, "<HTML><HEAD><TITLE>Index of %s</TITLE></HEAD><BODY>"
                "<H1>Index of %s</H1><PRE>",
                uri, uri);
        strcat(z, "Name                      Size      "
               "Date-Time             Type<HR WIDTH=100%>"CRLF);
    }

    HTTPWriteBodyChunk(sessionP, z, strlen(z));

    /* Sort the files */
    qsort(listP->item, listP->size, sizeof(void *),
          (TQSortProc)(sort == 1 ? cmpfilenames : cmpfiledates));
    
    /* Write the listing */
    if (ascending)
        i = 0;
    else
        i = listP->size - 1;

    while ((i < listP->size) && (i >= 0)) {
        TFileInfo * fi;
        struct tm ftm;

        fi = listP->item[i];

        if (ascending)
            ++i;
        else
            --i;
            
        strcpy(z, fi->name);

        k = strlen(z);

        if (fi->attrib & A_SUBDIR) {
            z[k++] = '/';
            z[k] = '\0';
        }

        if (k > 24) {
            z[10] = '\0';
            strcpy(z1, z);
            strcat(z1, "...");
            strcat(z1, z + k - 11);
            k = 24;
            p = z1 + 24;
        } else {
            strcpy(z1, z);
            
            ++k;
            p = z1 + k;
            while (k < 25)
                z1[k++] = ' ';
            
            z1[25] = '\0';
        }

        xmlrpc_gmtime(fi->time_write, &ftm);
        sprintf(z2, "%02u/%02u/%04u %02u:%02u:%02u",ftm.tm_mday,ftm.tm_mon+1,
                ftm.tm_year+1900,ftm.tm_hour,ftm.tm_min,ftm.tm_sec);

        if (fi->attrib & A_SUBDIR) {
            strcpy(z3, "   --  ");
            z4 = "Directory";
        } else {
            if (fi->size < 9999)
                u = 'b';
            else {
                fi->size /= 1024;
                if (fi->size < 9999)
                    u = 'K';
                else {
                    fi->size /= 1024;
                    if (fi->size < 9999)
                        u = 'M';
                    else
                        u = 'G';
                }
            }
                
            sprintf(z3, "%5" PRIu64 " %c", fi->size, u);
            
            if (xmlrpc_streq(fi->name, ".."))
                z4 = "";
            else
                z4 = MIMETypeFromFileName2(mimeTypeP, fi->name);

            if (!z4)
                z4 = "Unknown";
        }

        if (text)
            sprintf(z, "%s%s %s    %s   %s"CRLF, z1, p, z3, z2, z4);
        else
            sprintf(z, "<A HREF=\"%s%s\">%s</A>%s %s    %s   %s"CRLF,
                    fi->name, fi->attrib & A_SUBDIR ? "/" : "",
                    z1, p, z3, z2, z4);

        HTTPWriteBodyChunk(sessionP, z, strlen(z));
    }
        
    /* Write the tail of the file */
    if (text)
        strcpy(z, SERVER_PLAIN_INFO);
    else
        strcpy(z, "</PRE>" SERVER_HTML_INFO "</BODY></HTML>" CRLF CRLF);
    
    HTTPWriteBodyChunk(sessionP, z, strlen(z));
}



static bool
notRecentlyModified(TSession * const sessionP,
                    time_t     const fileModTime) {

    bool retval;
    const char * imsHdr;

    imsHdr = RequestHeaderValue(sessionP, "if-modified-since");
    if (imsHdr) {
        bool valid;
        time_t datetime;
        DateDecode(imsHdr, &valid, &datetime);
        if (valid) {
            if (MIN(fileModTime, sessionP->date) <= datetime)
                retval = TRUE;
            else
                retval = FALSE;
        } else
            retval = FALSE;
    } else
        retval = FALSE;

    return retval;
}



static void
addLastModifiedHeader(TSession * const sessionP,
                      time_t     const fileModTime) {

    const char * lastModifiedValue;

    DateToString(MIN(fileModTime, sessionP->date), &lastModifiedValue);

    if (lastModifiedValue) {
        ResponseAddField(sessionP, "Last-Modified", lastModifiedValue);
        xmlrpc_strfree(lastModifiedValue);
    }
}
    


static void
handleDirectory(TSession *   const sessionP,
                const char * const dirName,
                time_t       const fileModTime,
                MIMEType *   const mimeTypeP) {

    bool text;
    bool ascending;
    uint16_t sort;    /* 1=by name, 2=by date */
    const char * error;
    
    determineSortType(sessionP->requestInfo.query,
                      &ascending, &sort, &text, &error);

    if (error) {
        ResponseStatus(sessionP, 400);
        xmlrpc_strfree(error);
    } else if (notRecentlyModified(sessionP, fileModTime)) {
        ResponseStatus(sessionP, 304);
        ResponseWriteStart(sessionP);
    } else {
        TPool pool;
        bool succeeded;
        succeeded = PoolCreate(&pool, 1024);
        if (!succeeded)
            ResponseStatus(sessionP, 500);
        else {
            TList list;
            uint16_t responseStatus;
            const char * error;
            generateListing(&list, dirName, sessionP->requestInfo.uri,
                            &pool, &error, &responseStatus);
            if (error) {
                ResponseStatus(sessionP, responseStatus);
                xmlrpc_strfree(error);
            } else {
                ResponseStatus(sessionP, 200);
                ResponseContentType(sessionP, 
                                    text ? "text/plain" : "text/html");
            
                addLastModifiedHeader(sessionP, fileModTime);
            
                ResponseChunked(sessionP);
                ResponseWriteStart(sessionP);
            
                if (sessionP->requestInfo.method!=m_head)
                    sendDirectoryDocument(&list, ascending, sort, text,
                                          sessionP->requestInfo.uri, mimeTypeP,
                                          sessionP);
            
                HTTPWriteEndChunk(sessionP);
            
                ListFree(&list);
            }
            PoolFree(&pool);
        }
    }
}



static void
composeEntityHeader(const char ** const entityHeaderP,
                    const char *  const mediatype,
                    uint64_t      const start,
                    uint64_t      const end,
                    uint64_t      const filesize) {
                         
    xmlrpc_asprintf(entityHeaderP, "Content-type: %s" CRLF
                    "Content-range: "
                    "bytes %" PRIu64 "-%" PRIu64 "/%" PRIu64 CRLF
                    "Content-length: %" PRIu64 CRLF CRLF,
                    mediatype, start, end, filesize, end-start+1);
}



#define BOUNDARY    "##123456789###BOUNDARY"

static void
sendBody(TSession *      const sessionP,
         const TFile *   const fileP,
         uint64_t        const filesize,
         const char *    const mediatype,
         uint64_t        const start0,
         uint64_t        const end0) {
/*----------------------------------------------------------------------------
   'start0' and 'end0' are meaningful only if the session has ranges.
-----------------------------------------------------------------------------*/
    char buffer[4096];

    if (sessionP->ranges.size == 0)
        ConnWriteFromFile(sessionP->conn, fileP, 0, filesize - 1,
                          buffer, sizeof(buffer), 0);
    else if (sessionP->ranges.size == 1)
        ConnWriteFromFile(sessionP->conn, fileP, start0, end0,
                          buffer, sizeof(buffer), 0);
    else {
        uint64_t i;
        for (i = 0; i <= sessionP->ranges.size; ++i) {
            ConnWrite(sessionP->conn, "--", 2);
            ConnWrite(sessionP->conn, BOUNDARY, strlen(BOUNDARY));
            ConnWrite(sessionP->conn, CRLF, 2);

            if (i < sessionP->ranges.size) {
                uint64_t start;
                uint64_t end;
                bool decoded;
                    
                decoded = RangeDecode((char *)(sessionP->ranges.item[i]),
                                      filesize,
                                      &start, &end);
                if (decoded) {
                    /* Entity header, not response header */
                    const char * entityHeader;
                    
                    composeEntityHeader(&entityHeader, mediatype,
                                        start, end, filesize);

                    ConnWrite(sessionP->conn,
                              entityHeader, strlen(entityHeader));

                    xmlrpc_strfree(entityHeader);
                    
                    ConnWriteFromFile(sessionP->conn, fileP, start, end,
                                      buffer, sizeof(buffer), 0);
                }
            }
        }
    }
}



static void
sendFileAsResponse(TSession *   const sessionP,
                   TFile *      const fileP,
                   const char * const fileName,
                   time_t       const fileModTime,
                   MIMEType *   const mimeTypeP) {

    uint64_t const filesize = FileSize(fileP);
    const char * const mediatype = MIMETypeGuessFromFile2(mimeTypeP, fileName);

    uint64_t start;  /* Defined only if session has one range */
    uint64_t end;    /* Defined only if session has one range */

    switch (sessionP->ranges.size) {
    case 0:
        ResponseStatus(sessionP, 200);
        break;

    case 1: {
        bool decoded;
        decoded = RangeDecode((char *)(sessionP->ranges.item[0]), filesize,
                              &start, &end);
        if (!decoded) {
            ListFree(&sessionP->ranges);
            ResponseStatus(sessionP, 200);
        } else {
            const char * contentRange;
            xmlrpc_asprintf(&contentRange,
                            "bytes %" PRIu64 "-%" PRIu64 "/%" PRIu64,
                            start, end, filesize);
            ResponseAddField(sessionP, "Content-range", contentRange);
            xmlrpc_strfree(contentRange);

            ResponseContentLength(sessionP, end - start + 1);
            ResponseStatus(sessionP, 206);
        }
    } break;

    default:
        ResponseContentType(sessionP,
                            "multipart/ranges; boundary=" BOUNDARY);
        ResponseStatus(sessionP, 206);
        break;
    }
    
    if (sessionP->ranges.size == 0) {
        ResponseContentLength(sessionP, filesize);
        ResponseContentType(sessionP, mediatype);
    }
    
    addLastModifiedHeader(sessionP, fileModTime);

    ResponseWriteStart(sessionP);

    if (sessionP->requestInfo.method != m_head)
        sendBody(sessionP, fileP, filesize, mediatype, start, end);
}        



static void
handleFile(TSession *   const sessionP,
           const char * const fileName,
           time_t       const fileModTime,
           MIMEType *   const mimeTypeP) {
/*----------------------------------------------------------------------------
   This is an HTTP request handler for a GET.  It does the classic
   web server thing: send the file named in the URL to the client.
-----------------------------------------------------------------------------*/
    TFile * fileP;
    bool success;
    
    success = FileOpen(&fileP, fileName, O_BINARY | O_RDONLY);
    if (!success)
        ResponseStatusErrno(sessionP);
    else {
        if (notRecentlyModified(sessionP, fileModTime)) {
            ResponseStatus(sessionP, 304);
            ResponseWriteStart(sessionP);
        } else
            sendFileAsResponse(sessionP, fileP,
                               fileName, fileModTime, mimeTypeP);

        FileClose(fileP);
    }
}



static void
convertToNativeFileName(char * const fileName ATTR_UNUSED) {

#ifdef WIN32
    char * p;
    p = &fileName[0];
    while (*p) {
        if ((*p) == '/')
            *p= '\\';

        ++p;
    }
#endif  /* WIN32 */
}



abyss_bool
HandlerDefaultBuiltin(TSession * const sessionP) {

    BIHandler * const handlerP = SessionGetDefaultHandlerCtx(sessionP);

    char * p;
    char z[4096];
    TFileStat fs;
    bool endingslash;

    endingslash = FALSE;  /* initial value */

    if (!RequestValidURIPath(sessionP)) {
        ResponseStatus(sessionP, 400);
        return TRUE;
    }

    /* Must check for * (asterisk uri) in the future */
    if (sessionP->requestInfo.method == m_options) {
        ResponseAddField(sessionP, "Allow", "GET, HEAD");
        ResponseContentLength(sessionP, 0);
        ResponseStatus(sessionP, 200);
        return TRUE;
    }

    if ((sessionP->requestInfo.method != m_get) &&
        (sessionP->requestInfo.method != m_head)) {
        ResponseAddField(sessionP, "Allow", "GET, HEAD");
        ResponseStatus(sessionP, 405);
        return TRUE;
    }

    strcpy(z, handlerP->filesPath);
    strcat(z, sessionP->requestInfo.uri);

    p = z + strlen(z) - 1;
    if (*p == '/') {
        endingslash = TRUE;
        *p = '\0';
    }

    convertToNativeFileName(z);

    if (!FileStat(z, &fs)) {
        ResponseStatusErrno(sessionP);
        return TRUE;
    }

    if (fs.st_mode & S_IFDIR) {
        /* Redirect to the same directory but with the ending slash
        ** to avoid problems with some browsers (IE for examples) when
        ** they generate relative urls */
        if (!endingslash) {
            strcpy(z, sessionP->requestInfo.uri);
            p = z+strlen(z);
            *p = '/';
            *(p+1) = '\0';
            ResponseAddField(sessionP, "Location", z);
            ResponseStatus(sessionP, 302);
            ResponseWriteStart(sessionP);
            return TRUE;
        }

        *p = DIRECTORY_SEPARATOR[0];
        ++p;
        {
            unsigned int i;
            i = handlerP->defaultFileNames.size;
            while (i-- > 0) {
                *p = '\0';        
                strcat(z, (handlerP->defaultFileNames.item[i]));
                if (FileStat(z, &fs)) {
                    if (!(fs.st_mode & S_IFDIR))
                        handleFile(sessionP, z, fs.st_mtime,
                                   handlerP->mimeTypeP);
                }
            }
        }

        *(p-1) = '\0';
        
        if (!FileStat(z, &fs)) {
            ResponseStatusErrno(sessionP);
            return TRUE;
        }
        handleDirectory(sessionP, z, fs.st_mtime, handlerP->mimeTypeP);
    } else
        handleFile(sessionP, z, fs.st_mtime, handlerP->mimeTypeP);

    return TRUE;
}



/******************************************************************************
**
** server.c
**
** This file is part of the ABYSS Web server project.
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
