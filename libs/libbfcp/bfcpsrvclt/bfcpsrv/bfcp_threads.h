#ifndef _BFCP_THREADS_
#define _BFCP_THREADS_

#include <pthread.h>
typedef pthread_mutex_t bfcp_mutex_t;
#define bfcp_mutex_init(a,b) pthread_mutex_init(a,b)
#define bfcp_mutex_destroy(a) pthread_mutex_destroy(a)
#define bfcp_mutex_lock(a)				\
		pthread_mutex_lock(a);
#define bfcp_mutex_unlock(a)				\
		pthread_mutex_unlock(a);

#endif
