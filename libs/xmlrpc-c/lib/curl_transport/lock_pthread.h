#ifndef CURL_LOCK_PTHREAD_H_INCLUDED
#define CURL_LOCK_PTHREAD_H_INCLUDED

#include "lock.h"

lock *
curlLock_create_pthread(void);

#endif
