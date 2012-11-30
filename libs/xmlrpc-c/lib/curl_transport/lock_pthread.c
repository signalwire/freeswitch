#include <stdlib.h>

#include "mallocvar.h"
#include "pthreadx.h"

#include "lock.h"

#include "lock_pthread.h"

static lockAcquireFn acquire;

static void
acquire(struct lock * const lockP) {
    pthread_mutex_lock(&lockP->theLock);
}



static lockReleaseFn release;

static void
release(struct lock * const lockP) {
    pthread_mutex_unlock(&lockP->theLock);
}



static lockDestroyFn destroy;

static void
destroy(struct lock * const lockP) {
    pthread_mutex_destroy(&lockP->theLock);
    free(lockP);
}



struct lock *
curlLock_create_pthread(void) {
    struct lock * lockP;
    MALLOCVAR(lockP);
    if (lockP) {
        pthread_mutex_init(&lockP->theLock, NULL);
        lockP->acquire = &acquire;
        lockP->release = &release;
        lockP->destroy = &destroy;
    }
    return lockP;
}
