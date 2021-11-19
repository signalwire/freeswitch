#ifndef   TDEBUG_MEM_H
#define   TDEBUG_MEM_H
#ifdef dmalloc
#include <dmalloc.h>
#endif

#define  MEM_DEBUG_INIT() \
    do{\
        dmalloc_verify(0); \
        dmalloc_debug_setup("log-stats,log-non-free,check-fence,log=malloc.log");\
    }while(0)
#define  MEM_DEBUG_SHUTDOWN()\
    do{\
        dmalloc_shutdown();\
    }while(0)

#endif
