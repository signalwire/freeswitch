/* Copyright information is at the end of the file. */

#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <assert.h>

#include "bool.h"
#include "mallocvar.h"
#include "xmlrpc-c/util_int.h"
#include "xmlrpc-c/string_int.h"
#include "xmlrpc-c/sleep_int.h"
#include "xmlrpc-c/abyss.h"
#include "channel.h"
#include "server.h"
#include "thread.h"
#include "file.h"

#include "conn.h"

/*********************************************************************
** Conn
*********************************************************************/

static TThreadProc connJob;

static void
connJob(void * const userHandle) {
/*----------------------------------------------------------------------------
   This is the root function for a thread that processes a connection
   (performs HTTP transactions).
-----------------------------------------------------------------------------*/
    TConn * const connectionP = userHandle;

    (connectionP->job)(connectionP);

    connectionP->finished = TRUE;
        /* Note that if we are running in a forked process, setting
           connectionP->finished has no effect, because it's just our own
           copy of *connectionP.  In this case, Parent must update his own
           copy based on a SIGCHLD signal that the OS will generate right
           after we exit.
        */

    //ThreadExit(0);
}



static void
connDone(TConn * const connectionP) {

    /* In the forked case, this is designed to run in the parent
       process after the child has terminated.
    */
    connectionP->finished = TRUE;

    if (connectionP->done)
        connectionP->done(connectionP);
}



static TThreadDoneFn threadDone;

static void
threadDone(void * const userHandle) {

    TConn * const connectionP = userHandle;
    
    connDone(connectionP);
}



static void
makeThread(TConn *             const connectionP,
           enum abyss_foreback const foregroundBackground,
           bool                const useSigchld,
           const char **       const errorP) {
           
    switch (foregroundBackground) {
    case ABYSS_FOREGROUND:
        connectionP->hasOwnThread = FALSE;
        *errorP = NULL;
        break;
    case ABYSS_BACKGROUND: {
        const char * error;
        connectionP->hasOwnThread = TRUE;
        ThreadCreate(&connectionP->threadP, connectionP,
                     &connJob, &threadDone, useSigchld,
                     &error);
        if (error) {
            xmlrpc_asprintf(errorP, "Unable to create thread to "
                            "process connection.  %s", error);
            xmlrpc_strfree(error);
        } else
            *errorP = NULL;
    } break;
    } /* switch */
}

    

void
ConnCreate(TConn **            const connectionPP,
           TServer *           const serverP,
           TChannel *          const channelP,
           void *              const channelInfoP,
           TThreadProc *       const job,
           TThreadDoneFn *     const done,
           enum abyss_foreback const foregroundBackground,
           bool                const useSigchld,
           const char **       const errorP) {
/*----------------------------------------------------------------------------
   Create an HTTP connection.

   A connection carries one or more HTTP transactions (request/response).

   *channelP transports the requests and responses.

   The connection handles those HTTP requests.

   The connection handles the requests primarily by running the
   function 'job' once.  Some connections can do that autonomously, as
   soon as the connection is created.  Others don't until Caller
   subsequently calls ConnProcess.  Some connections complete the
   processing before ConnProcess return, while others may run the
   connection asynchronously to the creator, in the background, via a
   TThread thread.  'foregroundBackground' determines which.

   'job' calls methods of the connection to get requests and send
   responses.

   Some time after the HTTP transactions are all done, 'done' gets
   called in some context.

   'channelInfoP' == NULL means no channel info supplied.
-----------------------------------------------------------------------------*/
    TConn * connectionP;

    MALLOCVAR(connectionP);

    if (connectionP == NULL)
        xmlrpc_asprintf(errorP, "Unable to allocate memory for a connection "
                        "descriptor.");
    else {
        connectionP->server       = serverP;
        connectionP->channelP     = channelP;
        connectionP->channelInfoP = channelInfoP;
        connectionP->buffer[0]    = '\0';
        connectionP->buffersize   = 0;
        connectionP->bufferpos    = 0;
        connectionP->finished     = FALSE;
        connectionP->job          = job;
        connectionP->done         = done;
        connectionP->inbytes      = 0;
        connectionP->outbytes     = 0;
        connectionP->trace        = getenv("ABYSS_TRACE_CONN");

        makeThread(connectionP, foregroundBackground, useSigchld, errorP);
    }
    *connectionPP = connectionP;
}



bool
ConnProcess(TConn * const connectionP) {
/*----------------------------------------------------------------------------
   Drive the main processing of a connection -- run the connection's
   "job" function, which should read HTTP requests from the connection
   and send HTTP responses.

   If we succeed, we guarantee the connection's "done" function will get
   called some time after all processing is complete.  It might be before
   we return or some time after.  If we fail, we guarantee the "done"
   function will not be called.
-----------------------------------------------------------------------------*/
    bool retval;

    if (connectionP->hasOwnThread) {
        /* There's a background thread to handle this connection.  Set
           it running.
        */
        retval = ThreadRun(connectionP->threadP);
    } else {
        /* No background thread.  We just handle it here while Caller waits. */
        (connectionP->job)(connectionP);
        connDone(connectionP);
        retval = TRUE;
    }
    return retval;
}



void
ConnWaitAndRelease(TConn * const connectionP) {
    if (connectionP->hasOwnThread)
        ThreadWaitAndRelease(connectionP->threadP);
    
    free(connectionP);
}



bool
ConnKill(TConn * const connectionP) {
    connectionP->finished = TRUE;
    return ThreadKill(connectionP->threadP);
}



void
ConnReadInit(TConn * const connectionP) {

    if (connectionP->buffersize > connectionP->bufferpos) {
        connectionP->buffersize -= connectionP->bufferpos;
        memmove(connectionP->buffer,
                connectionP->buffer + connectionP->bufferpos,
                connectionP->buffersize);
        connectionP->bufferpos = 0;
    } else
        connectionP->buffersize = connectionP->bufferpos = 0;

    connectionP->buffer[connectionP->buffersize] = '\0';

    connectionP->inbytes = connectionP->outbytes = 0;
}



static size_t
nextLineSize(const char * const string,
             size_t       const startPos,
             size_t       const stringSize) {
/*----------------------------------------------------------------------------
   Return the length of the line that starts at offset 'startPos' in the
   string 'string', which is 'stringSize' characters long.

   'string' in not NUL-terminated.
   
   A line begins at beginning of string or after a newline character and
   runs through the next newline character or end of string.  The line
   includes the newline character at the end, if any.
-----------------------------------------------------------------------------*/
    size_t i;

    for (i = startPos; i < stringSize && string[i] != '\n'; ++i);

    if (i < stringSize)
        ++i;  /* Include the newline */

    return i - startPos;
}



static void
traceBuffer(const char * const label,
            const char * const buffer,
            unsigned int const size) {

    size_t cursor;  /* Index into buffer[] */

    fprintf(stderr, "%s:\n\n", label);

    for (cursor = 0; cursor < size; ) {
        /* Print one line of buffer */

        size_t const lineSize = nextLineSize(buffer, cursor, size);
        const char * const printableLine =
            xmlrpc_makePrintable_lp(&buffer[cursor], lineSize);
        
        fprintf(stderr, "%s\n", printableLine);

        cursor += lineSize;

        xmlrpc_strfree(printableLine);
    }
    fprintf(stderr, "\n");
}



static void
traceChannelRead(TConn *      const connectionP,
                 unsigned int const size) {

    if (connectionP->trace)
        traceBuffer("READ FROM CHANNEL",
                    connectionP->buffer + connectionP->buffersize, size);
}



static void
traceChannelWrite(TConn *      const connectionP,
                  const char * const buffer,
                  unsigned int const size,
                  bool         const failed) {
    
    if (connectionP->trace) {
        const char * const label =
            failed ? "FAILED TO WRITE TO CHANNEL" : "WROTE TO CHANNEL";
        traceBuffer(label, buffer, size);
    }
}



static uint32_t
bufferSpace(TConn * const connectionP) {
    
    return BUFFER_SIZE - connectionP->buffersize;
}
                    


bool
ConnRead(TConn *  const connectionP,
         uint32_t const timeout) {
/*----------------------------------------------------------------------------
   Read some stuff on connection *connectionP from the channel.

   Don't wait more than 'timeout' seconds for data to arrive.  Fail if
   nothing arrives within that time.

   'timeout' must be before the end of time.
-----------------------------------------------------------------------------*/
    time_t const deadline = time(NULL) + timeout;

    bool cantGetData;
    bool gotData;

    cantGetData = FALSE;
    gotData = FALSE;
    
    while (!gotData && !cantGetData) {
        int const timeLeft = (int)(deadline - time(NULL));

        if (timeLeft <= 0)
            cantGetData = TRUE;
        else {
            bool const waitForRead  = TRUE;
            bool const waitForWrite = FALSE;
            
            bool readyForRead;
            bool failed;
            
            ChannelWait(connectionP->channelP, waitForRead, waitForWrite,
                        timeLeft * 1000, &readyForRead, NULL, &failed);
            
            if (failed)
                cantGetData = TRUE;
            else {
                uint32_t bytesRead;
                bool readFailed;

                ChannelRead(connectionP->channelP,
                            connectionP->buffer + connectionP->buffersize,
                            bufferSpace(connectionP) - 1,
                            &bytesRead, &readFailed);

                if (readFailed)
                    cantGetData = TRUE;
                else {
                    if (bytesRead > 0) {
                        traceChannelRead(connectionP, bytesRead);
                        connectionP->inbytes += bytesRead;
                        connectionP->buffersize += bytesRead;
                        connectionP->buffer[connectionP->buffersize] = '\0';
                        gotData = TRUE;
                    } else
                        /* Other end has disconnected */
                        cantGetData = TRUE;
                }
            }
        }
    }
    if (gotData)
        return TRUE;
    else
        return FALSE;
}


            
bool
ConnWrite(TConn *      const connectionP,
          const void * const buffer,
          uint32_t     const size) {

    bool failed;

    ChannelWrite(connectionP->channelP, buffer, size, &failed);

    traceChannelWrite(connectionP, buffer, size, failed);

    if (!failed)
        connectionP->outbytes += size;

    return !failed;
}



bool
ConnWriteFromFile(TConn *       const connectionP,
                  const TFile * const fileP,
                  uint64_t      const start,
                  uint64_t      const last,
                  void *        const buffer,
                  uint32_t      const buffersize,
                  uint32_t      const rate) {
/*----------------------------------------------------------------------------
   Write the contents of the file stream *fileP, from offset 'start'
   up through 'last', to the HTTP connection *connectionP.

   Meter the reading so as not to read more than 'rate' bytes per second.

   Use the 'bufferSize' bytes at 'buffer' as an internal buffer for this.
-----------------------------------------------------------------------------*/
    bool retval;
    uint32_t waittime;
    bool success;
    uint32_t readChunkSize;

    if (rate > 0) {
        readChunkSize = MIN(buffersize, rate);  /* One second's worth */
        waittime = (1000 * buffersize) / rate;
    } else {
        readChunkSize = buffersize;
        waittime = 0;
    }

    success = FileSeek(fileP, start, SEEK_SET);
    if (!success)
        retval = FALSE;
    else {
        uint64_t const totalBytesToRead = last - start + 1;
        uint64_t bytesread;

        bytesread = 0;  /* initial value */

        while (bytesread < totalBytesToRead) {
            uint64_t const bytesLeft     = totalBytesToRead - bytesread;
            uint64_t const bytesToRead64 = MIN(readChunkSize, bytesLeft);
            uint32_t const bytesToRead   = (uint32_t)bytesToRead64;
            
            uint32_t bytesReadThisTime;

            assert(bytesToRead == bytesToRead64); /* readChunkSize is uint32 */

            bytesReadThisTime = FileRead(fileP, buffer, bytesToRead);
            bytesread += bytesReadThisTime;
            
            if (bytesReadThisTime > 0)
                ConnWrite(connectionP, buffer, bytesReadThisTime);
            else
                break;
            
            if (waittime > 0)
                xmlrpc_millisecond_sleep(waittime);
        }
        retval = (bytesread >= totalBytesToRead);
    }
    return retval;
}



TServer *
ConnServer(TConn * const connectionP) {
    return connectionP->server;
}



void
ConnFormatClientAddr(TConn *       const connectionP,
                     const char ** const clientAddrPP) {

    ChannelFormatPeerInfo(connectionP->channelP, clientAddrPP);
}



/*******************************************************************************
**
** conn.c
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
