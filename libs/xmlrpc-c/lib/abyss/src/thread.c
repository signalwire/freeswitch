/*******************************************************************************
**
** thread.c
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
*******************************************************************************/

#ifdef ABYSS_WIN32
#include <process.h>
#endif

#include "xmlrpc-c/abyss.h"

#include "xmlrpc_config.h"

/* 16K is the minimum size of stack on Win32 */
#define  THREAD_STACK_SIZE    (240*1024)

/*********************************************************************
** Thread
*********************************************************************/

abyss_bool ThreadCreate(TThread *   const t ATTR_UNUSED,
                        TThreadProc const func,
                        void *      const arg )
{
#ifdef ABYSS_WIN32
    DWORD z;
    *t =(TThread)_beginthreadex( NULL, THREAD_STACK_SIZE, func, 
                                 arg, CREATE_SUSPENDED, &z );
    return (*t!=NULL);
#else
#   ifdef _UNIX
#       ifdef _THREAD
    {
        pthread_attr_t attr;
        pthread_attr_init( &attr );
        pthread_attr_setstacksize( &attr, THREAD_STACK_SIZE );
        if( pthread_create( t,&attr,(PTHREAD_START_ROUTINE)func,arg)==0)
        {
            pthread_attr_destroy( &attr );
            return (pthread_detach(*t)==0);
        }
        pthread_attr_destroy( &attr );
        return FALSE;
    }
#       else
    switch (fork())
    {
    case 0:
        (*func)(arg);
        exit(0);
    case (-1):
        return FALSE;
    };
    
    return TRUE;
#       endif   /* _THREAD */
#   else
    (*func)(arg);
    return TRUE;
#   endif   /*_UNIX */
#endif  /* ABYSS_WIN32 */
}

abyss_bool
ThreadRun(TThread * const t ATTR_UNUSED) {
#ifdef ABYSS_WIN32
    return (ResumeThread(*t)!=0xFFFFFFFF);
#else
    return TRUE;    
#endif  /* ABYSS_WIN32 */
}



abyss_bool
ThreadStop(TThread * const t ATTR_UNUSED) {
#ifdef ABYSS_WIN32
    return (SuspendThread(*t)!=0xFFFFFFFF);
#else
    return TRUE;
#endif  /* ABYSS_WIN32 */
}



abyss_bool
ThreadKill(TThread * const t ATTR_UNUSED) {
#ifdef ABYSS_WIN32
    return (TerminateThread(*t,0)!=0);
#else
    /*return (pthread_kill(*t)==0);*/
    return TRUE;
#endif  /* ABYSS_WIN32 */
}



void ThreadWait(uint32_t ms)
{
#ifdef ABYSS_WIN32
    Sleep(ms);
#else
    usleep(ms*1000);
#endif  /* ABYSS_WIN32 */
}



void
ThreadExit(TThread * const t ATTR_UNUSED,
           int       const ret_value ATTR_UNUSED) {
#ifdef ABYSS_WIN32
    _endthreadex(ret_value);
#elif defined(_THREAD)
    pthread_exit(&ret_value);
#else
    ;
#endif  /* ABYSS_WIN32 */
}



void
ThreadClose(TThread * const t ATTR_UNUSED) {
#ifdef ABYSS_WIN32
    CloseHandle(*t);
#endif  /* ABYSS_WIN32 */
}



/*********************************************************************
** Mutex
*********************************************************************/



abyss_bool
MutexCreate(TMutex * const m ATTR_UNUSED) {
#if defined(ABYSS_WIN32)
    return ((*m=CreateMutex(NULL,FALSE,NULL))!=NULL);
#elif defined(_THREAD)
    return (pthread_mutex_init(m, NULL)==0);
#else
    return TRUE;
#endif  
}



abyss_bool
MutexLock(TMutex * const m ATTR_UNUSED) {
#if defined(ABYSS_WIN32)
    return (WaitForSingleObject(*m,INFINITE)!=WAIT_TIMEOUT);
#elif defined(_THREAD)
    return (pthread_mutex_lock(m)==0);
#else
    return TRUE;
#endif
}



abyss_bool
MutexUnlock(TMutex * const m ATTR_UNUSED) {
#if defined(ABYSS_WIN32)
    return ReleaseMutex(*m);
#elif defined(_THREAD)
    return (pthread_mutex_unlock(m)==0);
#else
    return TRUE;
#endif
}



abyss_bool
MutexTryLock(TMutex * const m ATTR_UNUSED) {
#if defined(ABYSS_WIN32)
    return (WaitForSingleObject(*m,0)!=WAIT_TIMEOUT);
#elif defined(_THREAD)
    return (pthread_mutex_trylock(m)==0);
#else
    return TRUE;
#endif
}



void
MutexFree(TMutex * const m ATTR_UNUSED) {
#if defined(ABYSS_WIN32)
    CloseHandle(*m);
#elif defined(_THREAD)
    pthread_mutex_destroy(m);
#else
    ;
#endif
}
