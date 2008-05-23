#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <wait.h>
#include <signal.h>

#include "xmlrpc_config.h"
#include "xmlrpc-c/string_int.h"
#include "xmlrpc-c/abyss.h"

#include "mallocvar.h"
#include "thread.h"


static void
blockSignalClass(int        const signalClass,
                 sigset_t * const oldBlockedSetP) {

    sigset_t newBlockedSet;

    sigemptyset(&newBlockedSet);
    sigaddset(&newBlockedSet, signalClass);

    sigprocmask(SIG_BLOCK, &newBlockedSet, oldBlockedSetP);
}



struct abyss_thread {
    struct abyss_thread * nextInPoolP;
    TThreadDoneFn * threadDone;
    void * userHandle;
    pid_t pid;
    bool useSigchld;
        /* This means that user is going to call ThreadHandleSigchld()
           when it gets a death of a child signal for this process.  If
           false, he's going to leave us in the dark, so we'll have to
           poll to know if the process is dead or not.
        */
};


/* Because signals are global, we need this global variable in order for
   the signal handler to figure out to what thread the signal belongs.
*/

/* We use a singly linked list.  Every time we access it, we have to run
   the whole chain.  To make this scale up, we should replace it with
   a doubly linked list and some kind of index by PID.

   But large scale systems probably aren't using fork threads anyway.
*/

static struct {
    struct abyss_thread * firstP;
} ThreadPool;
   


void
ThreadPoolInit(void) {

    ThreadPool.firstP = NULL;
}



static struct abyss_thread *
findThread(pid_t const pid) {

    struct abyss_thread * p;

    for (p = ThreadPool.firstP; p && p->pid != pid; p = p->nextInPoolP);
    
    return p;
}



static void
addToPool(struct abyss_thread * const threadP) {

    if (ThreadPool.firstP == NULL)
        ThreadPool.firstP = threadP;
    else {
        struct abyss_thread * p;

        for (p = ThreadPool.firstP; p->nextInPoolP; p = p->nextInPoolP);

        /* p points to the last thread in the list */

        p->nextInPoolP = threadP;
    }
}



static void
removeFromPool(struct abyss_thread * const threadP) {

    if (threadP == ThreadPool.firstP)
        ThreadPool.firstP = threadP->nextInPoolP;
    else {
        struct abyss_thread * p;

        for (p = ThreadPool.firstP;
             p && p->nextInPoolP != threadP;
             p = p->nextInPoolP);
        
        if (p)
            /* p points to thread right before the one we want to remove */
            p->nextInPoolP = threadP->nextInPoolP;
    }
}



void
ThreadHandleSigchld(pid_t const pid) {
/*----------------------------------------------------------------------------
   Handle a death of a child signal for process 'pid', which may be one
   of our threads.
-----------------------------------------------------------------------------*/
    struct abyss_thread * const threadP = findThread(pid);

    if (threadP) {
        if (threadP->threadDone)
            threadP->threadDone(threadP->userHandle);
        threadP->pid = 0;
    }
    /* Note that threadDone might free *threadP */
}



void
ThreadUpdateStatus(TThread * const threadP) {

    if (!threadP->useSigchld) {
        if (threadP->pid) {
            if (kill(threadP->pid, 0) != 0) {
                if (threadP->threadDone)
                    threadP->threadDone(threadP->userHandle);
                threadP->pid = 0;
            }
        }
    }
}



void
ThreadCreate(TThread **      const threadPP,
             void *          const userHandle,
             TThreadProc   * const func,
             TThreadDoneFn * const threadDone,
             bool            const useSigchld,
             const char **   const errorP) {
    
    TThread * threadP;

    MALLOCVAR(threadP);
    if (threadP == NULL)
        xmlrpc_asprintf(errorP,
                        "Can't allocate memory for thread descriptor.");
    else {
        sigset_t oldBlockedSet;
        pid_t rc;

        threadP->nextInPoolP = NULL;
        threadP->threadDone  = threadDone;
        threadP->userHandle  = userHandle;
        threadP->useSigchld  = useSigchld;
        threadP->pid         = 0;

        /* We have to be sure we don't get the SIGCHLD for this child's
           death until the child is properly registered in the thread pool
           so that the handler will know who he is.
        */
        blockSignalClass(SIGCHLD, &oldBlockedSet);

        rc = fork();
        
        if (rc < 0)
            xmlrpc_asprintf(errorP, "fork() failed, errno=%d (%s)",
                            errno, strerror(errno));
        else if (rc == 0) {
            /* This is the child */
            (*func)(userHandle);
            exit(0);
        } else {
            /* This is the parent */
            threadP->pid = rc;

            addToPool(threadP);

            sigprocmask(SIG_SETMASK, &oldBlockedSet, NULL);  /* restore */

            *errorP = NULL;
            *threadPP = threadP;
        }
        if (*errorP) {
            removeFromPool(threadP);
            free(threadP);
        }
    }
}



bool
ThreadRun(TThread * const threadP ATTR_UNUSED) {
    return TRUE;    
}



bool
ThreadStop(TThread * const threadP ATTR_UNUSED) {
    return TRUE;
}



bool
ThreadKill(TThread * const threadP ATTR_UNUSED) {
    return TRUE;
}



void
ThreadWaitAndRelease(TThread * const threadP) {

    if (threadP->pid) {
        int exitStatus;

        waitpid(threadP->pid, &exitStatus, 0);

        threadP->threadDone(threadP->userHandle);

        threadP->pid = 0;
    }
    ThreadRelease(threadP);
}



void
ThreadExit(int const retValue) {

    /* Note that the OS will automatically send a SIGCHLD signal to
       the parent process after we exit.  The handler for that signal
       will run threadDone in parent's context.  Alternatively, if
       the parent is set up for signals, the parent will eventually
       poll for the existence of our PID and call threadDone when he
       sees we've gone.
    */

    exit(retValue);
}



void
ThreadRelease(TThread * const threadP) {

    removeFromPool(threadP);
    free(threadP);
}



bool
ThreadForks(void) {

    return TRUE;
}



/*********************************************************************
** Mutex
*********************************************************************/

/* As two processes don't share memory, there is nothing to synchronize,
   so locking is a no-op.
*/

bool
MutexCreate(TMutex ** const mutexP ATTR_UNUSED) {
    return TRUE;
}



bool
MutexLock(TMutex * const mutexP ATTR_UNUSED) {
    return TRUE;
}



bool
MutexUnlock(TMutex * const mutexP ATTR_UNUSED) {
    return TRUE;
}



bool
MutexTryLock(TMutex * const mutexP ATTR_UNUSED) {
    return TRUE;
}



void
MutexDestroy(TMutex * const mutexP ATTR_UNUSED) {

}
