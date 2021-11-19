#ifndef TYPEDEF_H
#define TYPEDEF_H
#include <pthread.h>
#include <fcntl.h>
#include <inttypes.h>
#include <semaphore.h>

typedef int32_t    INT32;
typedef int32_t*   PINT32;
typedef uint32_t    UINT32;
typedef uint64_t    UINT64;
typedef uint16_t    UINT16;
typedef int16_t    INT16;
typedef int        BOOL;
typedef int             INT;
typedef int           *PINT;
typedef void            VOID;
typedef char          CHAR;
typedef char          WCHAR;
typedef char 	      TCHAR;
typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef float           FLOAT;
typedef unsigned int   DWORD;
typedef unsigned int    UINT;
typedef INT32            LONG;
typedef UINT32          ULONG;
typedef short           SHORT;
typedef unsigned short  USHORT;
typedef unsigned char   UCHAR;
typedef unsigned short* PUSHORT;
typedef BOOL            *LPBOOL;
typedef BYTE            *LPBYTE;
typedef BYTE            *PBYTE;
typedef WORD            *LPWORD;
typedef DWORD           *LPDWORD;
typedef void            *LPVOID;
typedef void            *PVOID;
typedef const void      *LPCVOID;
typedef CHAR            *PSTR;
typedef CHAR            *LPSTR;
typedef const CHAR      *LPCSTR;
typedef CHAR            *LPTSTR;
typedef LONG  *LPLONG;
typedef LONG  *PLONG;
typedef const CHAR      *LPCTSTR;
typedef WORD            *PWORD;
typedef DWORD           *PDWORD;
typedef	size_t 			SIZE_T;	
typedef ULONG           *PULONG;
typedef CHAR            *PCHAR;
typedef UCHAR           *PUCHAR;
typedef long long         LONGLONG;
typedef unsigned long long       ULONGLONG;
typedef CHAR  *HPSTR;
typedef WCHAR  *PWSTR;
typedef const WCHAR     *PCWSTR;
typedef int             HANDLE;
typedef HANDLE   *PHANDLE;
typedef UCHAR  BOOLEAN;
typedef LONG  NTSTATUS;
typedef BOOL  *PBOOL;
typedef WCHAR  *LPWSTR;
typedef const WCHAR     *LPCWSTR;
typedef pthread_mutex_t  CRITICAL_SECTION;
typedef pthread_mutex_t  *PCRITICAL_SECTION;
typedef pthread_mutex_t  *LPCRITICAL_SECTION;
typedef sem_t SynsSem;
typedef unsigned long long u64;
typedef int64_t __int64;
typedef u64    FILETIME;
typedef ULONG  u32;
typedef unsigned short u16;
typedef unsigned char u8;
typedef short int  *PSHORT;
#define L
#define CALLBACK
#define WINAPI
#define TRUE  1
#define FALSE  0

typedef enum _POOL_TYPE
{
    NonPagedPool,
    PagedPool,
    NonPagedPoolMustSucceed,
    DontUseThisType,
    NonPagedPoolCacheAligned,
    PagedPoolCacheAligned,
    NonPagedPoolCacheAlignedMustS,
    MaxPoolType,
    NonPagedPoolSession = 32,
    PagedPoolSession = NonPagedPoolSession + 1,
    NonPagedPoolMustSucceedSession = PagedPoolSession + 1,
    DontUseThisTypeSession = NonPagedPoolMustSucceedSession + 1,
    NonPagedPoolCacheAlignedSession = DontUseThisTypeSession + 1,
    PagedPoolCacheAlignedSession = NonPagedPoolCacheAlignedSession + 1,
    NonPagedPoolCacheAlignedMustSSession = PagedPoolCacheAlignedSession + 1,
} POOL_TYPE;

#endif



