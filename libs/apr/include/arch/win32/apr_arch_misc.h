/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef MISC_H
#define MISC_H

#include "apr.h"
#include "apr_portable.h"
#include "apr_private.h"
#include "apr_general.h"
#include "apr_pools.h"
#include "apr_getopt.h"
#include "apr_thread_proc.h"
#include "apr_file_io.h"
#include "apr_errno.h"
#include "apr_getopt.h"

#if APR_HAVE_STDIO_H
#include <stdio.h>
#endif
#if APR_HAVE_SIGNAL_H
#include <signal.h>
#endif
#if APR_HAVE_PTHREAD_H
#include <pthread.h>
#endif

/* ### create APR_HAVE_* macros for these? */
#if APR_HAVE_STDLIB_H
#include <stdlib.h>
#endif
#if APR_HAVE_STRING_H
#include <string.h>
#endif

struct apr_other_child_rec_t {
    apr_pool_t *p;
    struct apr_other_child_rec_t *next;
    apr_proc_t *proc;
    void (*maintenance) (int, void *, int);
    void *data;
    apr_os_file_t write_fd;
};

#define WSAHighByte 2
#define WSALowByte 0

/* start.c and apr_app.c helpers and communication within misc.c
 *
 * They are not for public consumption, although apr_app_init_complete
 * must be an exported symbol to avoid reinitialization.
 */
extern int APR_DECLARE_DATA apr_app_init_complete;

int apr_wastrtoastr(char const * const * *retarr, 
                    wchar_t const * const *arr, int args);

/* Platform specific designation of run time os version.
 * Gaps allow for specific service pack levels that
 * export new kernel or winsock functions or behavior.
 */
typedef enum {
        APR_WIN_UNK =       0,
        APR_WIN_UNSUP =     1,
        APR_WIN_95 =       10,
        APR_WIN_95_B =     11,
        APR_WIN_95_OSR2 =  12,
        APR_WIN_98 =       14,
        APR_WIN_98_SE =    16,
        APR_WIN_ME =       18,

	APR_WIN_UNICODE =  20, /* Prior versions support only narrow chars */

        APR_WIN_CE_3 =     23, /* CE is an odd beast, not supporting */
                               /* some pre-NT features, such as the    */
        APR_WIN_NT =       30, /* narrow charset APIs (fooA fns), while  */
        APR_WIN_NT_3_5 =   35, /* not supporting some NT-family features.  */
        APR_WIN_NT_3_51 =  36,

        APR_WIN_NT_4 =     40,
        APR_WIN_NT_4_SP2 = 42,
        APR_WIN_NT_4_SP3 = 43,
        APR_WIN_NT_4_SP4 = 44,
        APR_WIN_NT_4_SP5 = 45,
        APR_WIN_NT_4_SP6 = 46,

        APR_WIN_2000 =     50,
        APR_WIN_2000_SP1 = 51,
        APR_WIN_2000_SP2 = 52,
        APR_WIN_XP =       60,
        APR_WIN_XP_SP1 =   61,
        APR_WIN_XP_SP2 =   62,
        APR_WIN_2003 =     70
} apr_oslevel_e;

extern APR_DECLARE_DATA apr_oslevel_e apr_os_level;

apr_status_t apr_get_oslevel(apr_oslevel_e *);

/* The APR_HAS_ANSI_FS symbol is PRIVATE, and internal to APR.
 * APR only supports char data for filenames.  Like most applications,
 * characters >127 are essentially undefined.  APR_HAS_UNICODE_FS lets
 * the application know that utf-8 is the encoding method of APR, and
 * only incidently hints that we have Wide OS calls.
 *
 * APR_HAS_ANSI_FS is simply an OS flag to tell us all calls must be
 * the unicode eqivilant.
 */

#if defined(_WIN32_WCE) || defined(WINNT)
#define APR_HAS_ANSI_FS           0
#else
#define APR_HAS_ANSI_FS           1
#endif

/* IF_WIN_OS_IS_UNICODE / ELSE_WIN_OS_IS_ANSI help us keep the code trivial
 * where have runtime tests for unicode-ness, that aren't needed in any
 * build which supports only WINNT or WCE.
 */
#if APR_HAS_ANSI_FS && APR_HAS_UNICODE_FS
#define IF_WIN_OS_IS_UNICODE if (apr_os_level >= APR_WIN_UNICODE)
#define ELSE_WIN_OS_IS_ANSI else
#else /* APR_HAS_UNICODE_FS */
#define IF_WIN_OS_IS_UNICODE
#define ELSE_WIN_OS_IS_ANSI
#endif /* WINNT */

typedef enum {
    DLL_WINBASEAPI = 0,    // kernel32 From WinBase.h
    DLL_WINADVAPI = 1,     // advapi32 From WinBase.h
    DLL_WINSOCKAPI = 2,    // mswsock  From WinSock.h
    DLL_WINSOCK2API = 3,   // ws2_32   From WinSock2.h
    DLL_SHSTDAPI = 4,      // shell32  From ShellAPI.h
    DLL_NTDLL = 5,         // shell32  From our real kernel
    DLL_defined = 6        // must define as last idx_ + 1
} apr_dlltoken_e;

FARPROC apr_load_dll_func(apr_dlltoken_e fnLib, char *fnName, int ordinal);

/* The apr_load_dll_func call WILL fault if the function cannot be loaded */

#define APR_DECLARE_LATE_DLL_FUNC(lib, rettype, calltype, fn, ord, args, names) \
    typedef rettype (calltype *apr_winapi_fpt_##fn) args; \
    static apr_winapi_fpt_##fn apr_winapi_pfn_##fn = NULL; \
    __inline rettype apr_winapi_##fn args \
    {   if (!apr_winapi_pfn_##fn) \
            apr_winapi_pfn_##fn = (apr_winapi_fpt_##fn) \
                                      apr_load_dll_func(lib, #fn, ord); \
        return (*(apr_winapi_pfn_##fn)) names; }; \

/* Provide late bound declarations of every API function missing from
 * one or more supported releases of the Win32 API
 *
 * lib is the enumerated token from apr_dlltoken_e, and must correspond
 * to the string table entry in start.c used by the apr_load_dll_func().
 * Token names (attempt to) follow Windows.h declarations prefixed by DLL_
 * in order to facilitate comparison.  Use the exact declaration syntax
 * and names from Windows.h to prevent ambigutity and bugs.
 *
 * rettype and calltype follow the original declaration in Windows.h
 * fn is the true function name - beware Ansi/Unicode #defined macros
 * ord is the ordinal within the library, use 0 if it varies between versions
 * args is the parameter list following the original declaration, in parens
 * names is the parameter list sans data types, enclosed in parens
 *
 * #undef/re#define the Ansi/Unicode generic name to abate confusion
 * In the case of non-text functions, simply #define the original name
 */

#if !defined(_WIN32_WCE) && !defined(WINNT)

#ifdef GetFileAttributesExA
#undef GetFileAttributesExA
#endif
APR_DECLARE_LATE_DLL_FUNC(DLL_WINBASEAPI, BOOL, WINAPI, GetFileAttributesExA, 0, (
    IN LPCSTR lpFileName,
    IN GET_FILEEX_INFO_LEVELS fInfoLevelId,
    OUT LPVOID lpFileInformation),
    (lpFileName, fInfoLevelId, lpFileInformation));
#define GetFileAttributesExA apr_winapi_GetFileAttributesExA
#undef GetFileAttributesEx
#define GetFileAttributesEx apr_winapi_GetFileAttributesExA

#ifdef GetFileAttributesExW
#undef GetFileAttributesExW
#endif
APR_DECLARE_LATE_DLL_FUNC(DLL_WINBASEAPI, BOOL, WINAPI, GetFileAttributesExW, 0, (
    IN LPCWSTR lpFileName,
    IN GET_FILEEX_INFO_LEVELS fInfoLevelId,
    OUT LPVOID lpFileInformation),
    (lpFileName, fInfoLevelId, lpFileInformation));
#define GetFileAttributesExW apr_winapi_GetFileAttributesExW

APR_DECLARE_LATE_DLL_FUNC(DLL_WINBASEAPI, BOOL, WINAPI, CancelIo, 0, (
    IN HANDLE hFile),
    (hFile));
#define CancelIo apr_winapi_CancelIo

APR_DECLARE_LATE_DLL_FUNC(DLL_WINBASEAPI, BOOL, WINAPI, TryEnterCriticalSection, 0, (
    LPCRITICAL_SECTION lpCriticalSection),
    (lpCriticalSection));
#define TryEnterCriticalSection apr_winapi_TryEnterCriticalSection

APR_DECLARE_LATE_DLL_FUNC(DLL_WINBASEAPI, BOOL, WINAPI, SwitchToThread, 0, (
    void),
    ());
#define SwitchToThread apr_winapi_SwitchToThread

APR_DECLARE_LATE_DLL_FUNC(DLL_WINADVAPI, BOOL, WINAPI, GetEffectiveRightsFromAclW, 0, (
    IN PACL pacl,
    IN PTRUSTEE_W pTrustee,
    OUT PACCESS_MASK pAccessRights),
    (pacl, pTrustee, pAccessRights));
#define GetEffectiveRightsFromAclW apr_winapi_GetEffectiveRightsFromAclW

APR_DECLARE_LATE_DLL_FUNC(DLL_WINADVAPI, BOOL, WINAPI, GetNamedSecurityInfoW, 0, (
    IN LPWSTR pObjectName,
    IN SE_OBJECT_TYPE ObjectType,
    IN SECURITY_INFORMATION SecurityInfo,
    OUT PSID *ppsidOwner,
    OUT PSID *ppsidGroup,
    OUT PACL *ppDacl,
    OUT PACL *ppSacl,
    OUT PSECURITY_DESCRIPTOR *ppSecurityDescriptor),
    (pObjectName, ObjectType, SecurityInfo, ppsidOwner, ppsidGroup, 
	 ppDacl, ppSacl, ppSecurityDescriptor));
#define GetNamedSecurityInfoW apr_winapi_GetNamedSecurityInfoW

APR_DECLARE_LATE_DLL_FUNC(DLL_WINADVAPI, BOOL, WINAPI, GetNamedSecurityInfoA, 0, (
    IN LPSTR pObjectName,
    IN SE_OBJECT_TYPE ObjectType,
    IN SECURITY_INFORMATION SecurityInfo,
    OUT PSID *ppsidOwner,
    OUT PSID *ppsidGroup,
    OUT PACL *ppDacl,
    OUT PACL *ppSacl,
    OUT PSECURITY_DESCRIPTOR *ppSecurityDescriptor),
    (pObjectName, ObjectType, SecurityInfo, ppsidOwner, ppsidGroup, 
	 ppDacl, ppSacl, ppSecurityDescriptor));
#define GetNamedSecurityInfoA apr_winapi_GetNamedSecurityInfoA
#undef GetNamedSecurityInfo
#define GetNamedSecurityInfo apr_winapi_GetNamedSecurityInfoA

APR_DECLARE_LATE_DLL_FUNC(DLL_WINADVAPI, BOOL, WINAPI, GetSecurityInfo, 0, (
    IN HANDLE handle,
    IN SE_OBJECT_TYPE ObjectType,
    IN SECURITY_INFORMATION SecurityInfo,
    OUT PSID *ppsidOwner,
    OUT PSID *ppsidGroup,
    OUT PACL *ppDacl,
    OUT PACL *ppSacl,
    OUT PSECURITY_DESCRIPTOR *ppSecurityDescriptor),
    (handle, ObjectType, SecurityInfo, ppsidOwner, ppsidGroup, 
	 ppDacl, ppSacl, ppSecurityDescriptor));
#define GetSecurityInfo apr_winapi_GetSecurityInfo

APR_DECLARE_LATE_DLL_FUNC(DLL_SHSTDAPI, LPWSTR *, WINAPI, CommandLineToArgvW, 0, (
    LPCWSTR lpCmdLine, 
    int *pNumArgs),
    (lpCmdLine, pNumArgs));
#define CommandLineToArgvW apr_winapi_CommandLineToArgvW

#endif /* !defined(_WIN32_WCE) && !defined(WINNT) */

#if !defined(_WIN32_WCE)

APR_DECLARE_LATE_DLL_FUNC(DLL_NTDLL, DWORD, WINAPI, NtQueryTimerResolution, 0, (
    ULONG *pMaxRes,  /* Minimum NS Resolution */
    ULONG *pMinRes,  /* Maximum NS Resolution */
    ULONG *pCurRes), /* Current NS Resolution */
    (pMaxRes, pMinRes, pCurRes));
#define QueryTimerResolution apr_winapi_NtQueryTimerResolution

APR_DECLARE_LATE_DLL_FUNC(DLL_NTDLL, DWORD, WINAPI, NtSetTimerResolution, 0, (
    ULONG ReqRes,    /* Requested NS Clock Resolution */
    BOOL  Acquire,   /* Aquire (1) or Release (0) our interest */
    ULONG *pNewRes), /* The NS Clock Resolution granted */
    (ReqRes, Acquire, pNewRes));
#define SetTimerResolution apr_winapi_NtSetTimerResolution

/* ### These are ULONG_PTR values, but that's int32 for all we care
 * until the Win64 port is prepared.
 */
typedef struct PBI {
    DWORD ExitStatus;
    PVOID PebBaseAddress;
    ULONG AffinityMask;
    LONG  BasePriority;
    ULONG UniqueProcessId;
    ULONG InheritedFromUniqueProcessId;
} PBI, *PPBI;

APR_DECLARE_LATE_DLL_FUNC(DLL_NTDLL, DWORD, WINAPI, NtQueryInformationProcess, 0, (
    HANDLE hProcess,  /* Obvious */
    INT   info,       /* Use 0 for PBI documented above */
    PVOID pPI,        /* The PIB buffer */
    ULONG LenPI,      /* Use sizeof(PBI) */
    ULONG *pSizePI),  /* returns pPI buffer used (may pass NULL) */
    (hProcess, info, pPI, LenPI, pSizePI));
#define QueryInformationProcess apr_winapi_NtQueryInformationProcess

APR_DECLARE_LATE_DLL_FUNC(DLL_NTDLL, DWORD, WINAPI, NtQueryObject, 0, (
    HANDLE hObject,   /* Obvious */
    INT   info,       /* Use 0 for PBI documented above */
    PVOID pOI,        /* The PIB buffer */
    ULONG LenOI,      /* Use sizeof(PBI) */
    ULONG *pSizeOI),  /* returns pPI buffer used (may pass NULL) */
    (hObject, info, pOI, LenOI, pSizeOI));
#define QueryObject apr_winapi_NtQueryObject

#endif /* !defined(_WIN32_WCE) */

#endif  /* ! MISC_H */

