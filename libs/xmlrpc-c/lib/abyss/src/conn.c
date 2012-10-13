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

   We never return.  We ultimately exit the thread.
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


    /* Note that ThreadExit() runs a cleanup function, which in our
       case is connDone().
    */
    ThreadExit(connectionP->threadP, 0);
}



/* This is the maximum amount of stack that 'connJob' itself uses --
   does not count what user's connection job function uses.
*/
#define CONNJOB_STACK 1024


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
           size_t              const jobStackSize,
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
                     CONNJOB_STACK + jobStackSize,
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
           size_t              const jobStackSize,
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
        connectionP->buffer.b[0]  = '\0';
        connectionP->buffersize   = 0;
        connectionP->bufferpos    = 0;
        connectionP->finished     = FALSE;
        connectionP->job          = job;
        connectionP->done         = done;
        connectionP->inbytes      = 0;
        connectionP->outbytes     = 0;
        connectionP->trace        = getenv("ABYSS_TRACE_CONN");

        makeThread(connectionP, foregroundBackground, useSigchld,
                   jobStackSize, errorP);
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
        assert(connectionP->threadP);
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

    if (connectionP->hasOwnThread) {
        assert(connectionP->threadP);
        ThreadWaitAndRelease(connectionP->threadP);
    }
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
        memmove(connectionP->buffer.b,
                connectionP->buffer.b + connectionP->bufferpos,
                connectionP->buffersize);
        connectionP->bufferpos = 0;
    } else
        connectionP->buffersize = connectionP->bufferpos = 0;

    connectionP->buffer.b[connectionP->buffersize] = '\0';

    connectionP->inbytes = connectionP->outbytes = 0;
}



static void
traceReadTimeout(TConn *  const connectionP,
                 uint32_t const timeout) {

    if (connectionP->trace)
        fprintf(stderr, "TIMED OUT waiting over %u seconds "
                "for data from client.\n", timeout);
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
traceBuffer(const char *          const label,
            const unsigned char * const buffer,
            unsigned int          const size) {

    const char * const buffer_t = (const char *)buffer;

    size_t cursor;  /* Index into buffer[] */

    fprintf(stderr, "%s:\n\n", label);

    for (cursor = 0; cursor < size; ) {
        /* Print one line of buffer */

        size_t const lineSize = nextLineSize(buffer_t, cursor, size);
        const char * const printableLine =
            xmlrpc_makePrintable_lp(&buffer_t[cursor], lineSize);
        
        fprintf(stderr, "%s\n", printableLine);

        cursor += lineSize;

        xmlrpc_strfree(printableLine);
    }
    fprintf(stderr, "\n");
}



static void
traceBufferText(const char * const label,
                const char * const buffer,
                unsigned int const size) {

    traceBuffer(label, (const unsigned char *)buffer, size);
}



static void
traceChannelRead(TConn *      const connectionP,
                 unsigned int const size) {

    if (connectionP->trace)
        traceBuffer("READ FROM CHANNEL",
                    connectionP->buffer.b + connectionP->buffersize, size);
}



static void
traceChannelWrite(TConn *      const connectionP,
                  const char * const buffer,
                  unsigned int const size,
                  bool         const failed) {
    
    if (connectionP->trace) {
        const char * const label =
            failed ? "FAILED TO WRITE TO CHANNEL" : "WROTE TO CHANNEL";
        traceBufferText(label, buffer, size);
    }
}



static uint32_t
bufferSpace(TConn * const connectionP) {
    
    return BUFFER_SIZE - connectionP->buffersize;
}
                    


static void
readFromChannel(TConn *       const connectionP,
                bool *        const eofP,
                const char ** const errorP) {
/*----------------------------------------------------------------------------
   Read some data from the channel of Connection *connectionP.

   Iff there is none available to read, return *eofP == true.
-----------------------------------------------------------------------------*/
    uint32_t bytesRead;
    bool readError;

    ChannelRead(connectionP->channelP,
                connectionP->buffer.b + connectionP->buffersize,
                bufferSpace(connectionP) - 1,
                &bytesRead, &readError);

    if (readError)
        xmlrpc_asprintf(errorP, "Error reading from channel");
    else {
        *errorP = NULL;
        if (bytesRead > 0) {
            *eofP = FALSE;
            traceChannelRead(connectionP, bytesRead);
            connectionP->inbytes += bytesRead;
            connectionP->buffersize += bytesRead;
            connectionP->buffer.t[connectionP->buffersize] = '\0';
        } else
            *eofP = TRUE;
    }
}



static void
dealWithReadTimeout(bool *        const timedOutP,
                    bool          const timedOut,
                    uint32_t      const timeout,
                    const char ** const errorP) {

    if (timedOutP)
        *timedOutP = timedOut;
    else {
        if (timedOut)
            xmlrpc_asprintf(errorP, "Read from Abyss client "
                            "connection timed out after %u seconds "
                            "or was interrupted",
                            timeout);
    }
}



static void
dealWithReadEof(bool *        const eofP,
                bool          const eof,
                const char ** const errorP) {

    if (eofP)
        *eofP = eof;
    else {
        if (eof)
            xmlrpc_asprintf(errorP, "Read from Abyss client "
                            "connection failed because client closed the "
                            "connection");
    }
}



void
ConnRead(TConn *       const connectionP,
         uint32_t      const timeout,
         bool *        const eofP,
         bool *        const timedOutP,
         const char ** const errorP) {
/*----------------------------------------------------------------------------
   Read some stuff on connection *connectionP from the channel.  Read it into
   the connection's buffer.

   Don't wait more than 'timeout' seconds for data to arrive.  If no data has
   arrived by then and 'timedOutP' is null, fail.  If 'timedOut' is non-null,
   return as *timedOutP whether 'timeout' seconds passed without any data
   arriving.

   Also, stop waiting upon any interruption and treat it the same as a
   timeout.  An interruption is either a signal received (and caught) at
   an appropriate time or a ChannelInterrupt() call before or during the
   wait.

   If 'eofP' is non-null, return *eofP == true, without reading anything, iff
   there will no more data forthcoming on the connection because client has
   closed the connection.  If 'eofP' is null, fail in that case.
-----------------------------------------------------------------------------*/
    uint32_t const timeoutMs = timeout * 1000;

    if (timeoutMs < timeout)
        /* Arithmetic overflow */
        xmlrpc_asprintf(errorP, "Timeout value is too large");
    else {
        bool const waitForRead  = TRUE;
        bool const waitForWrite = FALSE;

        bool readyForRead;
        bool failed;
            
        ChannelWait(connectionP->channelP, waitForRead, waitForWrite,
                    timeoutMs, &readyForRead, NULL, &failed);
            
        if (failed)
            xmlrpc_asprintf(errorP,
                            "Wait for stuff to arrive from client failed.");
        else {
            bool eof;
            if (readyForRead) {
                readFromChannel(connectionP, &eof, errorP);
            } else {
                /* Wait was interrupted, either by our requested timeout,
                   a (caught) signal, or a ChannelInterrupt().
                */
                traceReadTimeout(connectionP, timeout);
                *errorP = NULL;
                eof = FALSE;
            }
            if (!*errorP)
                dealWithReadTimeout(timedOutP, !readyForRead, timeout, errorP);
            if (!*errorP)
                dealWithReadEof(eofP, eof, errorP);
        }
    }
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
	uint32_t ChunkSize = 4096 * 2; /* read buffer size */

    if (rate > 0) {
        readChunkSize = MIN(buffersize, rate);  /* One second's worth */
        waittime = (1000 * buffersize) / rate;
    } else {
        readChunkSize = ChunkSize;
        waittime = 0;
    }

    success = FileSeek(fileP, start, SEEK_SET);
    if (!success)
        retval = FALSE;
    else {
        uint64_t const totalBytesToRead = last - start + 1;
        uint64_t bytesread = 0;

        int32_t bytesReadThisTime = 0;
        char * chunk = (char *) buffer; /* the beginning */
        do {

			if ((bytesReadThisTime = FileRead(fileP, chunk, readChunkSize)) <= 0 )
				break;
			
			bytesread += bytesReadThisTime;
			chunk += bytesReadThisTime;

			/* fix bug in ms ie as it doesn't render text/plain properly                    */
			/* if CRLFs are split between reassembled tcp packets,                          */
			/* ie "might" undeterministically render extra empty lines                      */
			/* if it ends in CR or LF, read an extra chunk until the buffer is full         */
			/* or end of file is reached. You may still have bad luck, complaints go to MS) */

/*			if (bytesReadThisTime == readChunkSize &&  chunk - (char *) buffer + readChunkSize < buffersize) { 
 *				char * end = chunk - 1;
 *				if (*end == CR || *end == LF) {
 *					continue;
 *				}
 *			}
 */				          
            if (!bytesReadThisTime || !ConnWrite(connectionP, buffer, chunk - (char *) buffer)) {
                break;
			}

			chunk = (char *) buffer; /* a new beginning */

			if (waittime > 0)
                xmlrpc_millisecond_sleep(waittime);
			
        } while (bytesReadThisTime == readChunkSize);

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
