/* Copyright (C) 2001 by First Peer, Inc. All rights reserved.
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
** SUCH DAMAGE. */

#include "xmlrpc_config.h"

#include <process.h>

#include "mallocvar.h"

#include "pthreadx.h"

#undef PACKAGE
#undef VERSION

struct winStartArg {
    pthread_func * func;
    void *         arg;
};



static unsigned int __stdcall 
winThreadStart(void * const arg) {
/*----------------------------------------------------------------------------
   This is a thread start/root function for the Windows threading facility
   (i.e. this can be an argument to _beginthreadex()).

   All we do is call the real start/root function, which expects to be
   called in the pthread format.
-----------------------------------------------------------------------------*/
    struct winStartArg * const winStartArgP = arg;

    winStartArgP->func(winStartArgP->arg);

    free(winStartArgP);

    return 0;
}



int
pthread_create(pthread_t *            const newThreadIdP,
               const pthread_attr_t * const attr,
               pthread_func *               func,
               void *                 const arg) {

    HANDLE hThread;
    DWORD dwThreadID;
    struct winStartArg * winStartArgP;

    MALLOCVAR_NOFAIL(winStartArgP);

    winStartArgP->func = func;
    winStartArgP->arg  = arg;

    hThread = (HANDLE) _beginthreadex(
        NULL, 0, &winThreadStart, (LPVOID)winStartArgP, CREATE_SUSPENDED,
        &dwThreadID);

    SetThreadPriority(hThread, THREAD_PRIORITY_NORMAL); 
    ResumeThread(hThread);

    *newThreadIdP = hThread;

    return hThread ? 0 : -1;
}



/* Just kill it. */
int
pthread_cancel(pthread_t const target_thread) {

    CloseHandle(target_thread);
    return 0;
}



/* Waits for the thread to exit before continuing. */
int
pthread_join(pthread_t const target_thread,
             void **   const statusP) {

    DWORD dwResult = WaitForSingleObject(target_thread, INFINITE);
    *statusP = (void *)dwResult;
    return 0;
}



int
pthread_detach(pthread_t const target_thread) {
    return 0;
}



int
pthread_mutex_init(pthread_mutex_t *           const mp,
                   const pthread_mutexattr_t * const attr) {

    InitializeCriticalSection(mp);
    return 0;
}



int
pthread_mutex_lock(pthread_mutex_t * const mp) {

    EnterCriticalSection(mp);
    return 0;
}



int
pthread_mutex_unlock(pthread_mutex_t * const mp) {

    LeaveCriticalSection(mp);
    return 0;
}



int
pthread_mutex_destroy(pthread_mutex_t * const mp) {

    DeleteCriticalSection(mp);
    return 0;
}
