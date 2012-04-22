/* basic config.h to reproduce pre-autoconf days */

#define HAVE_SYS_TYPES_H 1
#define HAVE_SYS_WAIT_H
#define HAVE_SYS_RESOURCE_H
#define HAVE_SIGNAL_H 1

/* C99 integer types header... needed for ID3 parsing */
//#define HAVE_INTTYPES_H 1

/* aggresive and realtime options were enabled per default... unless for generic build */
#ifndef GENERIC
#define HAVE_SETPRIORITY
#ifndef NO_RT
	#define HAVE_SCHED_H
	#define HAVE_SCHED_SETSCHEDULER
#endif
#endif

#define INDEX_SIZE 1000

/* Use this (or similar) if <sys/types.h> does not define or typedef off_t */
/* #define off_t long int */  
  
/* Use this (or similar) if <sys/types.h> does not define or typedef size_t. */
/* #define size_t unsigned long int */

/* Use this (or similar) if <sys/types.h> does not define or typedef ssize_t. */
/* #define ssize_t long int */  

#ifndef HAVE_NO_STRERROR
#define HAVE_STRERROR 1
#endif

#include <io.h>