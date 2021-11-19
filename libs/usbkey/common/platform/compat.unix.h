#ifndef COMPAT_UNIX_H
#define COMPAT_UNIX_H
#include <ctype.h>
#include <dlfcn.h>

#include     <sys/ipc.h> // shared memory
#include     <sys/mman.h>
#include     <sys/shm.h> // shared memory

#include     <sys/socket.h>
#include <sys/ioctl.h>
#include     <arpa/inet.h>
#include  <net/if.h>
#include     <netinet/in_systm.h>
#include     <netinet/ip.h>
#include  <netinet/in.h> //for solaris
#include  <netinet/tcp.h> //for solaris
#include <sys/resource.h>


#include<pthread.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/types.h>
#include <sys/sysinfo.h>
#include <netdb.h>
#include  <sys/file.h>
#include <stdarg.h>
#include <stdio.h>
#include <malloc.h>

#include <stdlib.h>

#include <errno.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>

#include "typedef.h"
#include        "memdebug.h"
#include       <syslog.h>
#include <dirent.h>

#if defined(OS_LINUX)
#include <linux/ioctl.h>
#include        <linux/types.h>
#include <linux/limits.h>
#include <signal.h>
#include <assert.h>//added by yy for SC-4689 
#include <semaphore.h>
#endif

#ifdef  __cplusplus
extern "C"
{
#ifdef NULL
#undef NULL
#define NULL  0
#endif

#else

#ifdef NULL
#undef NULL
#define NULL (void*)0
#endif

#endif

#define __try
#define _try
#define __finally
#define _finally
#define __catch(arg...)  while(0)
#define __except(arg...)  while(0)
#define _except(arg...) while(0)

    typedef union _LARGE_INTEGER
    {

        struct
        {
            DWORD LowPart;
            DWORD HighPart;
        };

        uint64_t QuadPart;
    } LARGE_INTEGER;

#define INFINITE 0xffffffff
#define WAIT_OBJECT_0 0
#define WAIT_TIMEOUT  ETIMEDOUT

//	typedef unsigned int GROUP;
	typedef unsigned int GROUP;
	typedef unsigned long DWORD_PTR;
	typedef unsigned int *   PUINT;
    typedef LPVOID  HINSTANCE;
    typedef int     OFSTRUCT; // OpenFile 2 arg
    typedef LPVOID HGLOBAL;

    typedef void* (*LPV_LPV)(void*);

	typedef void* (WINAPI *PTHREAD_START_ROUTINE)(
    LPVOID lpThreadParameter
    );
	typedef PTHREAD_START_ROUTINE LPTHREAD_START_ROUTINE;

    typedef struct _SYSTEMTIME
    {
        WORD wYear;
        WORD wMonth;
        WORD wDayOfWeek;
        WORD wDay;
        WORD wHour;
        WORD wMinute;
        WORD wSecond;
        WORD wMilliseconds;
    } SYSTEMTIME, *PSYSTEMTIME, *LPSYSTEMTIME;

//++++END++++ appended by cqb for Linux Transplant

    typedef struct tagDEVIOCTL
    {
        u32* InAddr;
        u32  InSize;
        u32* OutAddr;
        u32  OutSize;
        u32* RtnAddr;
        u8   uKnlBId;
    } DEVIOCTL, *PDEVIOCTL;

//++++END++++ appended by cqb for Linux Transplant

    typedef struct _WSABUF
    {
        ULONG len; /* the length of the buffer */
        char *buf; /* the pointer to the buffer */
    } WSABUF, *LPWSABUF;

    typedef int       MMRESULT;
    typedef int  SOCKET;

    typedef struct WSAData
    {
        WORD                    wVersion;
        WORD                    wHighVersion;
        char                    szDescription[11];
        char                    szSystemStatus[11];
        unsigned short          iMaxSockets;
        unsigned short          iMaxUdpDg;
        char *                  lpVendorInfo;
    } WSADATA, *LPWSADATA;

    typedef void *  LPOVERLAPPED;
    typedef void *  HLOCAL;
    typedef HINSTANCE HMODULE;
    typedef int  *LPOFSTRUCT;
    typedef int  *POFSTRUCT;
    typedef int  HFILE;

//+++START+++added by lqf for DS-32274 Linux支持NTP 2014.10.20
//Start Added by sww 2008.01.15
/*
typedef struct tag_PIP_ADAPTER_INFO
{
    struct tag_PIP_ADAPTER_INFO *Next;
    char IPStr[20];
    int Index;
    short Status;
}IP_ADAPTER_INFO, *PIP_ADAPTER_INFO;

*/
typedef struct {
    char String[4 * 4];
} IP_ADDRESS_STRING, *PIP_ADDRESS_STRING, IP_MASK_STRING, *PIP_MASK_STRING;

typedef struct _IP_ADDR_STRING {
    struct _IP_ADDR_STRING* Next;
    IP_ADDRESS_STRING IpAddress;
    IP_MASK_STRING IpMask;
    DWORD Context;
} IP_ADDR_STRING, *PIP_ADDR_STRING;

#define MAX_ADAPTER_DESCRIPTION_LENGTH  128 // arb.
#define MAX_ADAPTER_NAME_LENGTH         256 // arb.
#define MAX_ADAPTER_ADDRESS_LENGTH      8   // arb.

typedef struct _IP_ADAPTER_INFO {
    struct _IP_ADAPTER_INFO* Next;
    char IPStr[20];
    short Status;
    DWORD ComboIndex;
    char AdapterName[MAX_ADAPTER_NAME_LENGTH + 4];
    char Description[MAX_ADAPTER_DESCRIPTION_LENGTH + 4];
    UINT AddressLength;
    BYTE Address[MAX_ADAPTER_ADDRESS_LENGTH];
    DWORD Index;
    UINT Type;
    UINT DhcpEnabled;
    PIP_ADDR_STRING CurrentIpAddress;
    IP_ADDR_STRING IpAddressList;
    IP_ADDR_STRING GatewayList;
    IP_ADDR_STRING DhcpServer;
    BOOL HaveWins;
    IP_ADDR_STRING PrimaryWinsServer;
    IP_ADDR_STRING SecondaryWinsServer;
    time_t LeaseObtained;
    time_t LeaseExpires;
} IP_ADAPTER_INFO, *PIP_ADAPTER_INFO;
//+++START+++added by lqf for DS-32274 Linux支持NTP 2014.10.20 
   
 typedef struct tag_MIB_IFROW
    {
        int    dwIndex;
        short    dwOperStatus;
    } MIB_IFROW;
	typedef struct _MEMORYSTATUSEX{
		DWORD dwLength; 
		DWORD dwMemoryLoad; 
		DWORD ullTotalPhys; 
		DWORD ullAvailPhys; 
		DWORD ullTotalPageFile; 
		DWORD ullAvailPageFile; 
		DWORD ullTotalVirtual; 
		DWORD ullAvailVirtual; 
		DWORD ullAvailExtendedVirtual;
	}MEMORYSTATUSEX, *LPMEMORYSTATUSEX;

//End Added by sww 2008.01.15



#ifdef DPRINTF
#define dprintf(fmt, arg...) printf("DEBUG:" fmt, ##arg)
#else
#define dprintf(fmt, arg...)
#endif

#define wsprintf sprintf
#define _snprintf snprintf
#define wvsprintf vsprintf
#define _strdup  strdup
#define stricmp strcasecmp
#define strnicmp strncasecmp

#define far
#define  WINAPI
#define  __stdcall
#define  CALLBACK

#define LPTIMECALLBACK void *
#define TIME_CALLBACK_FUNCTION 0x0000
#define TIME_PERIODIC 0x0001
#define FILE_ATTRIBUTE_NORMAL 0x00000080 

#define THREAD_BASE_PRIORITY_MAX    2
#define THREAD_BASE_PRIORITY_MIN    -2
#define THREAD_BASE_PRIORITY_IDLE   -15
#define MAXLONG     0x7fffffff

#define THREAD_PRIORITY_LOWEST          THREAD_BASE_PRIORITY_MIN
#define THREAD_PRIORITY_BELOW_NORMAL    (THREAD_PRIORITY_LOWEST+1)
#define THREAD_PRIORITY_NORMAL          0
#define THREAD_PRIORITY_HIGHEST         THREAD_BASE_PRIORITY_MAX
#define THREAD_PRIORITY_ABOVE_NORMAL    (THREAD_PRIORITY_HIGHEST-1)
#define THREAD_PRIORITY_ERROR_RETURN    (MAXLONG)

#define THREAD_PRIORITY_TIME_CRITICAL   THREAD_BASE_PRIORITY_LOWRT
#define THREAD_PRIORITY_IDLE            THREAD_BASE_PRIORITY_IDLE

#define MAKEWORD(a, b)      ((WORD)(((BYTE)(a)) | ((WORD)((BYTE)(b))) << 8))
#define MAKELONG(a, b)      ((LONG)(((WORD)(a)) | ((DWORD)((WORD)(b))) << 16))
#define max(x, y)	(((x) < (y)) ? (y) : (x))
#define min(x, y)	(((x) < (y)) ? (x) : (y))


    /* shared memory*/
#define SEGSIZE 100
#define ISSSMLOADED                     0

#ifndef FAR
#define FAR                             // far on OS/2
#endif
#define IN
#define OUT
#define TRUE    1
#define FALSE    0
#define MAX_PATH   260
#define READ_SIZE   8192

#define HIBYTE(w) ((BYTE)((WORD)(w)>>8)&0xFF)
#define LOBYTE(w) ((BYTE)(w))
#define HIWORD(d) ((WORD)((DWORD)(d)>>16)&0xFFFF)
#define LOWORD(d) ((WORD)(d))

////////////////////////////////////////////////////////////
// Define function with macro,,,
// The way can be not excellent,,,
////////////////////////////////////////////////////////////

#define ERROR_SHARING_VIOLATION         0
#define INVALID_HANDLE_VALUE  -1
#define INVALID_FILE_SIZE  0xffffffff

#define GMEM_FIXED            0x0000
#define LMEM_FIXED            GMEM_FIXED
#define GMEM_MOVEABLE        0x0002
#define GMEM_NOCOMPACT   0x0010
#define GMEM_NODISCARD   0x0020
#define GMEM_ZEROINIT   0x0040
#define GMEM_MODIFY   0x0080
#define GMEM_DISCARDABLE  0x0100
#define GMEM_NOT_BANKED   0x1000
#define GMEM_SHARE   0x2000
#define GMEM_DDESHARE   0x2000
#define GMEM_NOTIFY   0x4000
#define GMEM_LOWER   GMEM_NOT_BANKED
#define GMEM_VALID_FLAGS  0x7F72
#define GMEM_INVALID_HANDLE  0x8000
#define GHND    (GMEM_MOVEABLE | GMEM_ZEROINIT)
#define GPTR    (GMEM_FIXED | GMEM_ZEROINIT)
#define LPTR    (GMEM_FIXED | GMEM_ZEROINIT)

#define CF_ACCEPT       0x0000
#define CF_REJECT       0x0001
#define CF_DEFER        0x0002
#define OF_READ           O_RDONLY
#define OF_WRITE          O_WRONLY
#define OF_READWRITE      O_RDWR
#define OF_SHARE_COMPAT                 0
#define OF_SHARE_EXCLUSIVE              0
#define OF_SHARE_DENY_WRITE             0
#define OF_SHARE_DENY_READ              0
#define OF_SHARE_DENY_NONE              0x00000000
#define OF_PARSE                        0
#define OF_DELETE                       0
#define OF_VERIFY                       0
#define OF_CANCEL                       0
#define OF_CREATE         O_CREAT|O_TRUNC
#define OF_PROMPT                       0
#define OF_EXIST                        0
#define OF_REOPEN                       0
#define HFILE_ERROR         -1

#define CREATE_NEW                      O_EXCL
#define CREATE_ALWAYS                   O_CREAT|O_TRUNC
#define OPEN_EXISTING                   O_RDONLY
#define OPEN_ALWAYS                     O_CREAT|O_RDONLY
#define TRUNCATE_EXISTING               O_RDWR|O_TRUNC

#define GENERIC_READ                    O_RDONLY
#define GENERIC_WRITE                   O_WRONLY
#define GENERIC_EXECUTE
#define GENERIC_ALL                     O_RDWR

#define FILE_SHARE_READ                 1
#define FILE_SHARE_WRITE                2

#define FILE_BEGIN   SEEK_SET
#define FILE_CURRENT   SEEK_CUR
#define FILE_END   SEEK_END

#define TIME_PERIODIC   0x0001
#define INFINITE              0xffffffff

#define INVALID_SOCKET (int)(-1)
#define SOCKET_ERROR (int)(-1)

#ifdef _COMPAT_CPP
#define COMPAT_CPP
#else
#define COMPAT_CPP extern
#endif

// Shared library operation
    COMPAT_CPP INT
    get_shm_value(INT flag);

    COMPAT_CPP VOID
    set_shm_value(INT flag, INT value);

    COMPAT_CPP VOID
    rm_shm();

    COMPAT_CPP LPVOID
    LoadLibrary(LPCSTR lpLibFileName);
    COMPAT_CPP BOOL
    FreeLibrary(LPVOID hLibModule);
    COMPAT_CPP LPVOID
    GetProcAddress(
        LPVOID hModule, LPCSTR lpProcName);

//I/O and file operation
    COMPAT_CPP INT
    _lopen(LPCSTR lpPathName,
           INT iReadWrite);
    COMPAT_CPP INT
    _lcreat(LPCSTR lpPathName,
            INT iAttribute);
    COMPAT_CPP UINT
    _lread(INT fd,
           LPVOID lpBuffer, UINT uBytes);
    COMPAT_CPP UINT
    _lwrite(INT fd,
            LPCSTR lpBuffer, UINT uBytes);
    COMPAT_CPP INT
    _lclose(INT fd);
    COMPAT_CPP INT
    _llseek(INT fd,
            LONG lOffset, INT iOrigin);
//++++START++++ appended by cqb for Linux Transplant
//COMPAT_CPP INT
// _lseek(INT fd,
// LONG lOffset,INT iOrigin);
#define _lseek lseek
//++++END++++ appended by cqb for Linux Transplant

    COMPAT_CPP INT OpenFile(LPCSTR
                            lpFileName,
                            LPVOID lpReOpenBuff,
                            UINT uStyle);
    COMPAT_CPP INT CreateFile(
        LPCSTR lpFileName,
        DWORD dwDesiredAccess,
        DWORD dwShareMode,
        LPVOID lpSecurityAttributes,
        DWORD dwCreationDisposition,
        DWORD dwFlagsAndAttributes,
        LPVOID hTemplateFile);
    COMPAT_CPP BOOL CloseHandle(INT fd);
    COMPAT_CPP BOOL ReadFile(INT fd,
                             LPVOID lpBuffer,
                             DWORD nNumberOfBytesToRead,
                             LPDWORD lpNumberOfBytesRead,
                             LPVOID lpOverlapped);
    COMPAT_CPP BOOL WriteFile(INT fd,
                              LPCVOID lpBuffer,
                              DWORD nNumberOfBytesToWrite,
                              LPDWORD lpNumberOfBytesWritten,
                              LPVOID lpOverlapped);
    COMPAT_CPP DWORD GetFileSize(INT fd,
                                 LPDWORD lpFileSizeHigh);

    COMPAT_CPP DWORD GetTempPath(
			    DWORD nBufferLength,  // size of buffer
			    LPTSTR lpBuffer       // path buffer
			    );

    COMPAT_CPP UINT GetTempFileName(
	    LPCTSTR lpPathName,      // directory name
	    LPCTSTR lpPrefixString,  // file name prefix
	    UINT uUnique,            // integer
	    LPTSTR lpTempFileName    // file name buffer
	    );

    COMPAT_CPP BOOL SetEndOfFile(INT fd);
    COMPAT_CPP int FlushFileBuffers(INT fd);
    COMPAT_CPP DWORD SetFilePointer(INT fd,
                                    LONG lDistanceToMove,
                                    LPLONG lpDistanceToMoveHigh,
                                    DWORD dwMoveMethod);

// Profile operation
    COMPAT_CPP BOOL
    WritePrivateProfileSection(
        LPCTSTR lpAppName,
        LPCTSTR lpString,
        LPCTSTR lpFileName);
    COMPAT_CPP BOOL
    WritePrivateProfileString(
        LPCTSTR lpAppName,
        LPCTSTR lpKeyName,
        LPCTSTR lpString,
        LPCTSTR lpFileName);
    COMPAT_CPP UINT
    GetPrivateProfileInt(
        LPCTSTR lpAppName,
        LPCTSTR lpKeyName,
        INT nDefault,
        LPCTSTR lpFileName);
    COMPAT_CPP DWORD
    GetPrivateProfileString(
        LPCTSTR lpAppName,
        LPCTSTR lpKeyName,
        LPCTSTR lpDefault,
        LPTSTR lpReturnedString,
        DWORD nSize, LPCTSTR lpFileName);

// Critical operation
    COMPAT_CPP VOID
    InitializeCriticalSection(
        LPCRITICAL_SECTION cri_mutex);
    COMPAT_CPP INT
    EnterCriticalSection(
        LPCRITICAL_SECTION cri_mutex);
    COMPAT_CPP INT
    LeaveCriticalSection(
        LPCRITICAL_SECTION cri_mutex);
    COMPAT_CPP VOID
    DeleteCriticalSection(
        LPCRITICAL_SECTION cri_mutex);

// Other operation
    COMPAT_CPP LONG
    InterlockedExchange(
        LONG* Target, LONG Value);
    COMPAT_CPP DWORD
    GetLastError();
    COMPAT_CPP DWORD
    GetCurrentDirectory(
        DWORD nBufferLength,
        LPTSTR lpBuffer);
    COMPAT_CPP UINT
    GetWindowsDirectory(
        LPTSTR lpBuffer,
        UINT uSize);
    COMPAT_CPP LPSTR
    strupr(char* string);
	COMPAT_CPP LPSTR strlwr(char* string);
    COMPAT_CPP LPSTR
    lstrcpyn(char* s1 , const char* s2 , size_t nLen);

    COMPAT_CPP LPVOID
    GlobalAlloc(
        UINT uFlags,
        size_t dwBytes);
	COMPAT_CPP size_t 
	GlobalSize(
		HGLOBAL hMem   // handle to global memory object
		);

	COMPAT_CPP BOOL
	GlobalMemoryStatusEx(
	LPMEMORYSTATUSEX lpBuffer
	);
    COMPAT_CPP LPVOID
    GlobalFree(LPVOID hMem);
    COMPAT_CPP LPVOID
    LocalLock(LPVOID hMem);
    COMPAT_CPP BOOL
    LocalUnlock(LPVOID hMem);
    COMPAT_CPP LPVOID
    LocalLock(LPVOID hMem);
    COMPAT_CPP BOOL
    LocalUnlock(LPVOID hMem);
    COMPAT_CPP LPVOID
    GlobalLock(LPVOID hMem);

    COMPAT_CPP BOOL
    GlobalUnlock(LPVOID hMem);

    COMPAT_CPP MMRESULT
    timeSetEvent(DWORD lDelay,
                 UINT uResolution,
                 LPVOID timer_func,
                 DWORD_PTR dwUser,
                 UINT fuEvent);
    COMPAT_CPP void
    timeKillEvent(
        unsigned int TimerID);
    COMPAT_CPP LONG
    InterlockedDecrement(
        LONG* lpdwAddend);
    COMPAT_CPP LONG
    InterlockedIncrement(
        LONG* lpdwAddend);
    COMPAT_CPP INT closesocket(INT s);
    COMPAT_CPP INT WSACleanup();
    COMPAT_CPP INT
    WSAStartup(
        WORD wVersionRequested,
        LPWSADATA lpWSAData);
    COMPAT_CPP void
    CopyMemory(
        void *dest,
        const void *src,
        DWORD count);
    COMPAT_CPP HANDLE CreateThread(
        LPVOID lpThreadAttributes,
        DWORD dwStackSize,
        LPTHREAD_START_ROUTINE lpStartAddress,
        LPVOID lpParameter,
        DWORD dwCreationFlags,
        LPDWORD lpThreadId);
    COMPAT_CPP BOOL TerminateThread(HANDLE hHandle, DWORD exitCode);
    COMPAT_CPP HANDLE CreateEvent(
        LPVOID lpEventAttributes,
        BOOL bManualReset,
        BOOL bInitialState,
        LPCTSTR lpName);
    COMPAT_CPP BOOL SetEvent(HANDLE hEvent);
    COMPAT_CPP BOOL ResetEvent(HANDLE hEvent);
    COMPAT_CPP DWORD WaitForSingleObject(
        HANDLE hHandle,
        DWORD dwMilliseconds);
	COMPAT_CPP DWORD WaitForMultipleObjects(
		DWORD nCount,
		 HANDLE *lpHandles,
		BOOL bWaitAll, 
		DWORD dwMilliseconds);
    COMPAT_CPP BOOL SetThreadPriority(
        HANDLE hThread,
        int nPriority);
    COMPAT_CPP int WSARecv(
        SOCKET s,
        LPWSABUF lpBuffers,
        DWORD dwBufferCount,
        LPDWORD lpNumberOfBytesRecvd,
        LPDWORD lpFlags,
        LPVOID lpOl,
        LPVOID lpComp);
    COMPAT_CPP int WSASend(
        SOCKET s,
        LPWSABUF lpBuffers,
        DWORD dwBufferCount,
        LPDWORD lpNumberOfBytesSent,
        DWORD dwFlags,
        LPVOID lpOl,
        LPVOID lpComp);
    COMPAT_CPP int  WSAGetLastError(void);
    COMPAT_CPP VOID GetLocalTime(
        LPSYSTEMTIME lpSystemTime);

    COMPAT_CPP void OutputDebugString(LPCTSTR lpStr);
    COMPAT_CPP int lstrcmpi(LPCTSTR str1, LPCTSTR str2);
    COMPAT_CPP BOOL QueryPerformanceFrequency(LARGE_INTEGER *lpFrequency);
    COMPAT_CPP BOOL QueryPerformanceCounter(LARGE_INTEGER *lpPerformanceCount);
    COMPAT_CPP DWORD timeGetTime();
//+++++++++++++START+++++++++++++add  by wy
//for  WinSock32  库函数封装  五  4月 30 09:11:38 CST 2004
#define   MAX_EVENTS  64
#define   WS_FD_SETSIZE         MAX_EVENTS
#define   FD_MAX_EVENTS         5

#define   FD_READ  0x00000001
#define   FD_WRITE  0x00000002
#define   FD_ACCEPT  0x00000004
#define   FD_CONNECT  0x00000008
#define   FD_CLOSE  0x00000010

#define   FD_READ_BIT  0x00000000
#define   FD_WRITE_BIT          0x00000001
#define   FD_ACCEPT_BIT  0x00000002
#define   FD_CONNECT_BIT 0x00000003
#define   FD_CLOSE_BIT   0x00000004

#define   WSA_INVALID_EVENT     0
#define   WSA_WAIT_FAILED   (-1L)
#define   WSA_WAIT_TIMEOUT (-2L)
#define   WSA_WAIT_EVENT_0 0
#define   WSA_MAXIMUM_WAIT_EVENTS   MAX_EVENTS
#define WSAENETDOWN  ENETDOWN
#define WSAEFAULT  EFAULT
#define WSAEINTR  EINTR
#define WSAEINPROGRESS   EINPROGRESS
#define WSAEINVAL  EINVAL
#define WSAEMFILE        EMFILE
#define WSAENOBUFS  ENOBUFS
#define WSAENOTSOCK  ENOTSOCK
#define WSAEOPNOTSUPP  EOPNOTSUPP
#define WSAEWOULDBLOCK  EAGAIN
#define WSAEADDRINUSE  EADDRINUSE
#define WSAEALREADY   EALREADY
#define WSAEADDRNOTAVAIL EADDRNOTAVAIL
#define WSAEAFNOSUPPORT     EAFNOSUPPORT
#define WSAECONNREFUSED     ECONNREFUSED
#define WSAEISCONN     EISCONN
#define WSAENETUNREACH     ENETUNREACH
#define WSAEPROTONOSUPPORT EPROTONOSUPPORT
#define WSAETIMEDOUT  ETIMEDOUT
#define WSAEACCES  EACCES
#define WSAECONNRESET   ECONNRESET
#define WSAECONNABORTED  ECONNABORTED

#define ISNBLOCK(fd)    ( fcntl( (fd), F_GETFL, 0 ) & O_NONBLOCK )
#define ISBLOCK(fd)    (!ISNBLOCK(fd))
#define SETNONBLOCK(fd)    (fcntl((fd),F_SETFL,(fcntl((fd), F_GETFL,0) |O_NONBLOCK)))
#define SETBLOCK(fd)    (fcntl((fd),F_SETFL,(fcntl((fd), F_SETFL,0) & (~O_NONBLOCK)))

    typedef  struct sockaddr*  PSOCKADDR;
	typedef int far             *LPINT;
	typedef   LPVOID     LPQOS;
//typedef   HANDLE     WSAEVENT;


    typedef   struct
    {
        DWORD lNetworkEvents;
        int  iErrorCode[FD_MAX_EVENTS];

    }WSANETWORKEVENTS, *LPWSANETWORKEVENTS;

    typedef struct tagWSAEVENT
    {
        int   fd;
        int   msk;
        WSANETWORKEVENTS  ne;
    }*WSAEVENT;

	
//+++start+++  add by byl 2010/3/16
    typedef struct tagWIN32_FIND_DATA
    {
         DWORD dwFileAttributes; //文件属性
         FILETIME ftCreationTime; // 文件创建时间
         FILETIME ftLastAccessTime; // 文件最后一次访问时间
         FILETIME ftLastWriteTime; // 文件最后一次修改时间
         DWORD nFileSizeHigh; // 文件长度高32位
         DWORD nFileSizeLow; // 文件长度低32位
         DWORD dwReserved0; // 系统保留
         DWORD dwReserved1; // 系统保留
         TCHAR cFileName[ MAX_PATH ]; // 长文件名
         TCHAR cAlternateFileName[ 14 ]; // 8.3格式文件名
     }WIN32_FIND_DATA, *PWIN32_FIND_DATA;
//+++end+++   add by byl 2010/3/16

    typedef long INT_PTR;
#define FIELD_OFFSET(type, field)  ((LONG)(INT_PTR)&(((type *)0)->field))

    COMPAT_CPP WSAEVENT   WINAPI    WSACreateEvent(void);
    COMPAT_CPP BOOL       WINAPI    WSACloseEvent(WSAEVENT);
    COMPAT_CPP int        WINAPI    WSAEnumNetworkEvents(SOCKET, WSAEVENT, LPWSANETWORKEVENTS);
    COMPAT_CPP int        WINAPI    WSAConnect(SOCKET, const struct sockaddr*, int ,
            LPWSABUF, LPWSABUF, LPQOS, WSAEVENT);
    COMPAT_CPP int     WINAPI    WSAEventSelect(SOCKET, WSAEVENT, int);
    COMPAT_CPP int     WINAPI    WSAWaitForMultipleEvents(DWORD, const WSAEVENT*, BOOL, DWORD, BOOL);
	typedef int (CALLBACK * LPCONDITIONPROC)(
			IN LPWSABUF lpCallerId,
			IN LPWSABUF lpCallerData,
			IN OUT LPQOS lpSQOS,
			IN OUT LPQOS lpGQOS,
			IN LPWSABUF lpCalleeId,
			IN LPWSABUF lpCalleeData,
			OUT GROUP FAR * g,
			IN DWORD_PTR dwCallbackData
			);
	COMPAT_CPP int WINAPI WSAAccept(
				IN SOCKET s,
				OUT struct sockaddr FAR * addr,
				IN OUT LPINT addrlen,
				IN LPCONDITIONPROC lpfnCondition,
				IN DWORD_PTR dwCallbackData
				);

//++++++++++END+++++++++++appended  by  wy  for  WinSock32 库函数封装  五  4月 30 09:10:35 CST 2004

//++++++++++START+++++++++add by wy for linux transplant
#define   FILE_MAP_READ      0x00000002
#define   FILE_MAP_WRITE     0x00000004
#define   FILE_MAP_ALL_ACCESS 0x00000004
#define   SEC_COMMIT         0x08000000
#define   PAGE_READONLY      0X00000002
#define   PAGE_READWRITE     0x00000004
#define   ERROR_ALREADY_EXISTS 183
    COMPAT_CPP HANDLE   WINAPI      CreateFileMapping(HANDLE hfile, LPVOID attr, DWORD flags, DWORD low, DWORD high, LPCSTR name);
    COMPAT_CPP HANDLE   WINAPI      OpenFileMapping(DWORD  access, BOOL  inherit, LPCSTR name);
    COMPAT_CPP LPVOID   WINAPI      MapViewOfFile(HANDLE hobj, DWORD flags, DWORD offset_high,
            DWORD  offset_low, DWORD count);
    COMPAT_CPP BOOL     WINAPI      UnmapViewOfFile(LPVOID addr);
	COMPAT_CPP BOOL FlushViewOfFile(
			LPCVOID lpBaseAddress,         // starting address
			SIZE_T dwNumberOfBytesToFlush  // number of bytes in range
			);

//+++++++++END++++++++++++++add by wy for linux transplant

//++++START+++ appended by cqb for Linux Transplant
    COMPAT_CPP int DeleteFile(const char* pathname);
    COMPAT_CPP int DeleteFileW(LPCWSTR pathname);
    COMPAT_CPP BOOL DeviceIoControl(
        int fd,
        DWORD dwIoControlCode,
        LPVOID lpInBuffer,
        DWORD nInBufferSize,
        LPVOID lpOutBuffer,
        DWORD nOutBufferSize,
        LPDWORD lpBytesReturned,
        BYTE lpKnlBId);

    COMPAT_CPP LPVOID
    ExAllocatePool(
        UINT uFlags,
        size_t dwBytes);


    COMPAT_CPP LPVOID
    ExFreePoolEx(
        void* obj,
        DWORD dwBytes);
    COMPAT_CPP VOID
    ExFreePool(
        void* obj);
    COMPAT_CPP LPVOID GlobalAllocPtr(UINT flags, size_t cb);
    COMPAT_CPP LPVOID GlobalFreePtr(LPVOID lp);




//COMPAT_CPP int wsprintf(char* buf, const char * fmt, ...);

    COMPAT_CPP void _strupr(char* buf);
    COMPAT_CPP LPVOID LocalAlloc(UINT flags, DWORD cb);
    COMPAT_CPP LPVOID LocalFree(LPVOID lp);
    COMPAT_CPP void Sleep(DWORD x);
    COMPAT_CPP char* lstrcat(char *dest, const char *src);
    COMPAT_CPP size_t lstrlen(const char *s);
    COMPAT_CPP char* lstrcpy(char *dest, const char *src);
    COMPAT_CPP void DbgPrint(const char *fmt, ...);
    COMPAT_CPP BOOL RtlEqualMemory(const void *Destination, const void *Source, size_t Length);
    COMPAT_CPP void *RtlMoveMemory(void *Destination, const void *Source, size_t Length);
    COMPAT_CPP void *RtlCopyMemory(void *Destination, const void *Source, size_t Length);
    COMPAT_CPP void *RtlFillMemory(void *Destination, size_t Length, size_t Fill);
    COMPAT_CPP void *RtlZeroMemory(void *Destination, size_t Length);
    COMPAT_CPP void  InitCompat(void);
    COMPAT_CPP DWORD  GetTickCount(void);
    COMPAT_CPP int  GetLocalTimer(char* p);
    COMPAT_CPP unsigned int GetAdaptersInfo(PIP_ADAPTER_INFO *IPinfo, int *size);
    COMPAT_CPP char * _itoa(int val, char *str, int mode);
    COMPAT_CPP char * _ltoa(LONG val, char *str, int mode);
    COMPAT_CPP int _mkdir(char *dirname);
    COMPAT_CPP void ZeroMemory(PVOID DstStr, DWORD Len);
    COMPAT_CPP char *UpperString(char *src);
	COMPAT_CPP int  FindFirstFile(const CHAR *dirname,WIN32_FIND_DATA *pFindData);
	COMPAT_CPP int  CreateDirectory(const CHAR *dirname,LPTSTR lpBuffer);
	COMPAT_CPP void FindClose(int hFileSearch);
	void* TlsGetValue(DWORD tls_idx);
	
	typedef struct _SECURITY_ATTRIBUTES
	{
		DWORD nLength;
		/* [size_is] */ LPVOID lpSecurityDescriptor;
		BOOL bInheritHandle;
	} 	SECURITY_ATTRIBUTES;
	
	typedef struct _SECURITY_ATTRIBUTES *PSECURITY_ATTRIBUTES;
	
	typedef struct _SECURITY_ATTRIBUTES *LPSECURITY_ATTRIBUTES;
	COMPAT_CPP HANDLE  CreateSemaphore(
		LPSECURITY_ATTRIBUTES lpSemaphoreAttributes, // SD
		LONG lInitialCount,                          // initial count
		LONG lMaximumCount,                          // maximum count
		LPCTSTR lpName                               // object name
		);

	COMPAT_CPP BOOL ReleaseSemaphore(
		HANDLE hSemaphore,       // handle to semaphore
		LONG lReleaseCount,      // count increment amount
		LPLONG lpPreviousCount   // previous count
		);

//++++END++++ appended by cqb for Linux Transplant

#ifdef __cplusplus
}

#endif

#endif

