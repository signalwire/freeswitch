#ifdef WIN32
#include <sys/time.h>

typedef struct _FILETIME {
    unsigned long dwLowDateTime;
    unsigned long dwHighDateTime;
} FILETIME;

void __stdcall GetSystemTimeAsFileTime(FILETIME*);
  
//void gettimeofday(struct timeval* p, void* tz /* IGNORED */);

void gettimeofday(struct timeval* p, void* tz /* IGNORED */) {
  union {
   long long ns100; /*time since 1 Jan 1601 in 100ns units */
   FILETIME ft;
  } _now;

  GetSystemTimeAsFileTime( &(_now.ft) );
  p->tv_usec=(long)((_now.ns100 / 10LL) % 1000000LL );
  p->tv_sec= (long)((_now.ns100-(116444736000000000LL))/10000000LL);
  return;
}
#endif

