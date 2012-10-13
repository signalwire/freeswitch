#ifndef THREAD_H_INCLUDED
#define THREAD_H_INCLUDED

/*********************************************************************
** Thread
*********************************************************************/

#include "bool.h"

typedef struct abyss_thread TThread;

void
ThreadPoolInit(void);

typedef void TThreadProc(void * const userHandleP);
typedef void TThreadDoneFn(void * const userHandleP);

void
ThreadCreate(TThread **      const threadPP,
             void *          const userHandle,
             TThreadProc   * const func,
             TThreadDoneFn * const threadDone,
             bool            const useSigchld,
             size_t          const stackSize,
             const char **   const errorP);

bool
ThreadRun(TThread * const threadP);

bool
ThreadStop(TThread * const threadP);

bool
ThreadKill(TThread * const threadP);

void
ThreadWaitAndRelease(TThread * const threadP);

void
ThreadExit(TThread * const threadP,
           int       const retValue);

void
ThreadRelease(TThread * const threadP);

bool
ThreadForks(void);

void
ThreadUpdateStatus(TThread * const threadP);

#ifndef WIN32
void
ThreadHandleSigchld(pid_t const pid);
#endif

/*********************************************************************
** Mutex
*********************************************************************/

typedef struct abyss_mutex TMutex;

bool
MutexCreate(TMutex ** const mutexP);

bool
MutexLock(TMutex * const mutexP);

bool
MutexUnlock(TMutex * const mutexP);

bool
MutexTryLock(TMutex * const mutexP);

void
MutexDestroy(TMutex * const mutexP);

#endif
