#define _COMPAT_CPP
#include "compat.h"

#include <net/if_arp.h>

#ifdef OS_SOLARIS
#include <sys/sockio.h>
#endif

#define MAXINTERFACES 16
#include <assert.h>



#define THREAD_ID_NUM 0xbfffffff
#define EVENT_ID_NUM 0xfffffffe
#define SEM_ID_NUM 0x7fffffff
#define INVALID_HANDLE_NUM 0xffffffff
#define MAXIMUM_WAIT_OBJECTS 64     // Maximum number of wait objects
#define MAX_SEM 2400     // Maximum number of wait objects
static pthread_t th_creat_th[1600];
static sem_t sem_creat[MAX_SEM];
static DWORD tmp_thread_id;//0xbfffffff
static DWORD tmp_event_id; //0xfffffffe
static DWORD tmp_sem_id;   //0x7fffffff

typedef struct
{
    pthread_cond_t cond_wait;
    pthread_condattr_t condattr;
    pthread_mutex_t mut;
    int bManualReset;
    int bInitialState;
} SYNC_WAIT;

SYNC_WAIT  sync_wait[1600];

void InitCompat(void)
{
    tmp_thread_id = 0;
    tmp_event_id = 0;
	tmp_sem_id=0;
}

void OutputDebugString(LPCTSTR msg)
{
    fprintf(stderr, msg);
    fprintf(stderr , "\r\n");
}


BOOL QueryPerformanceFrequency(LARGE_INTEGER *lpFrequency)
{
    lpFrequency->QuadPart = 1000000;
    return TRUE;
}

BOOL QueryPerformanceCounter(LARGE_INTEGER *lpPerformanceCount)
{

    struct timeval now;

    if (gettimeofday(&now, NULL) == 0)
    {
        lpPerformanceCount->QuadPart = (uint64_t)now.tv_sec * 1000000 +now.tv_usec;
        return TRUE;
    }
    else
        return FALSE;
}

DWORD timeGetTime()
{
    DWORD nowInMillisec=0;
    struct timespec now;

    //if (gettimeofday(&now, NULL) == 0)
	//added by yy for DS-44175,2016.02.24
	if (clock_gettime(CLOCK_MONOTONIC,&now) == 0)
        nowInMillisec = now.tv_sec * 1000 + now.tv_nsec/1000000;
    return nowInMillisec;
}

int lstrcmpi(LPCTSTR s1, LPCTSTR s2)
{
    return strcmp(s1, s2);
}

HANDLE CreateThread(
    LPVOID lpThreadAttributes,
    DWORD dwStackSize,
    LPTHREAD_START_ROUTINE lpStartAddress,
    LPVOID lpParameter,
    DWORD dwCreationFlags,
    LPDWORD lpThreadId)
{
    if (tmp_thread_id > 1599)
    {
        fprintf(stderr, "\nReport ERROR @%s:%i\n", __FUNCTION__, __LINE__);
        tmp_thread_id = 0;
    }

    pthread_create(&th_creat_th[tmp_thread_id], (pthread_attr_t*)lpThreadAttributes, lpStartAddress, (LPVOID)lpParameter);

    return(0xbfffffff - tmp_thread_id++);
}

BOOL TerminateThread(HANDLE hHandle, DWORD exitCode)
{
    int tmp;
    tmp = 0xbfffffff - hHandle;
	if (th_creat_th[tmp])
	{
		pthread_cancel(th_creat_th[tmp]);
		pthread_join(th_creat_th[tmp],NULL);
		th_creat_th[tmp] = NULL;
	}
    
}

HANDLE CreateEvent(
    LPVOID lpEventAttributes,
    BOOL bManualReset,
    BOOL bInitialState,
    LPCTSTR lpName)
{
    if (tmp_event_id > 16)
    {
        fprintf(stderr, "\nReport ERROR @%s:%i\n", __FUNCTION__, __LINE__);
        tmp_event_id = 0;
    }

    pthread_mutex_init(&sync_wait[tmp_event_id].mut, NULL);

    pthread_mutex_lock(&sync_wait[tmp_event_id].mut);
    //Specifies whether a manual-reset or auto-reset event object is created. If TRUE, then you must use the ResetEvent function to manually reset the state to nonsignaled. If FALSE, the system automatically resets the state to nonsignaled after a single waiting thread has been released. 
    sync_wait[tmp_event_id].bManualReset = bManualReset;
    //Specifies the initial state of the event object. If TRUE, the initial state is signaled; otherwise, it is nonsignaled. 

    sync_wait[tmp_event_id].bInitialState = bInitialState;
//nonsignaled
	pthread_mutex_unlock(&sync_wait[tmp_event_id].mut);
	pthread_condattr_init(&(sync_wait[tmp_event_id].condattr));
	pthread_condattr_setclock(&(sync_wait[tmp_event_id].condattr), CLOCK_REALTIME);
    pthread_cond_init(&sync_wait[tmp_event_id].cond_wait, &(sync_wait[tmp_event_id].condattr));
    return(EVENT_ID_NUM - tmp_event_id++);
}

#define  EVENT_STOP_BIT        0x00000002     //appended by wy for linux transplant Thu May  6 19:18:42 CST 2004
//Use the SetEvent function to set the state of an event object to signaled.
BOOL SetEvent(HANDLE hEvent)
{
	if(hEvent == INVALID_HANDLE_NUM)
		return FALSE;
	if ((hEvent & 0x40000000) && (hEvent & 0x80000000))
{
	int tmp = EVENT_ID_NUM - hEvent;
	pthread_mutex_lock(&sync_wait[tmp].mut);
	sync_wait[tmp].bManualReset |= EVENT_STOP_BIT; //appended by wy for bug Thu May  6 19:18:42 CST 2004
	pthread_cond_broadcast(&sync_wait[tmp].cond_wait);
	sync_wait[tmp].bInitialState = TRUE;
	pthread_mutex_unlock(&sync_wait[tmp].mut);
}
	return TRUE;
}
//the ResetEvent function to reset the state of an event object to nonsignaled. 
BOOL ResetEvent(HANDLE hEvent)
{
	if(hEvent == INVALID_HANDLE_NUM)
		return FALSE;
    if ((hEvent & 0x40000000) && (hEvent & 0x80000000))
{
    int tmp = EVENT_ID_NUM - hEvent;

    pthread_mutex_lock(&sync_wait[tmp].mut);
       pthread_cond_destroy(&sync_wait[tmp].cond_wait);
        pthread_cond_init(&sync_wait[tmp].cond_wait,   NULL);
        sync_wait[tmp].bInitialState = FALSE;
    pthread_mutex_unlock(&sync_wait[tmp].mut);
}
    return TRUE;
}

DWORD WaitForSingleObject(
    HANDLE hHandle,
    DWORD dwMilliseconds)
{
    int tmp;
    DWORD dwRet = 0;

	if(hHandle == INVALID_HANDLE_NUM)
		return FALSE;
    if ((hHandle & 0x40000000) && (hHandle & 0x80000000))
    {
        tmp = EVENT_ID_NUM - hHandle;

        struct timespec ts;


/*
        struct timeval now;
        gettimeofday(&now, NULL);

        now.tv_usec += dwMilliseconds * 1000;

        ts.tv_sec = now.tv_sec + now.tv_usec / 1000000;
        ts.tv_nsec = (now.tv_usec % 1000000) * 1000;
*/
	clock_gettime(CLOCK_REALTIME, &ts);
	ts.tv_sec += dwMilliseconds / 1000;
        ts.tv_nsec += (dwMilliseconds % 1000) * 1000000;
	ts.tv_sec += ts.tv_nsec / 1000000000;
        ts.tv_nsec = ts.tv_nsec % 1000000000;

        pthread_mutex_lock(&sync_wait[tmp].mut);

	if(sync_wait[tmp].bInitialState)//signaled
	{
		usleep(100);
		if (!(sync_wait[tmp].bManualReset&= ~ EVENT_STOP_BIT))//FALSE auto-reset event object state to nonsignale
		{
			sync_wait[tmp].bInitialState = FALSE;
		}
		pthread_mutex_unlock(&sync_wait[tmp].mut);
		return 0;
	}

	if (dwMilliseconds == INFINITE)
	{
		pthread_cond_wait(&sync_wait[tmp].cond_wait, &sync_wait[tmp].mut);
	}
	else
	{
		dwRet = pthread_cond_timedwait(&sync_wait[tmp].cond_wait, &sync_wait[tmp].mut, &ts);
		dwRet = (sync_wait[tmp].bManualReset & EVENT_STOP_BIT) ? 0 : ETIMEDOUT; //appended by wy for bug Thu May  6 19:18:42 CST 2004
	    sync_wait[tmp].bManualReset &= ~ EVENT_STOP_BIT; //appended by wy for bug Thu May  6 19:18:42 CST 2004
	}

	if (!sync_wait[tmp].bManualReset)//FALSE auto-reset event object state to nonsignaled
	{
		sync_wait[tmp].bInitialState = FALSE;
	}

        pthread_mutex_unlock(&sync_wait[tmp].mut);
    }
    else
    {
		if ((hHandle & 0x80000000))//thread
		{
			//thread
			tmp = 0xbfffffff - hHandle;
			if (th_creat_th[tmp])
			{
				pthread_join(th_creat_th[tmp], NULL);
				th_creat_th[tmp] = NULL;
			}
			
        }
		else if(hHandle & 0x40000000)//semaphore
		{
			DWORD timeout=0;
			int retCode;
			struct timespec delay;
			tmp = 0x7fffffff - hHandle;

			if (dwMilliseconds == INFINITE)
			{
				retCode = sem_wait(&sem_creat[tmp]); // event semaphore handle
				return retCode;
			}

			while (timeout < dwMilliseconds )
			{
				delay.tv_sec = 0;
				delay.tv_nsec = 1000000;  /* 1 milli sec */
				// Wait for the event be signaled
				retCode = sem_trywait(
						&sem_creat[tmp]); // event semaphore handle
				// non blocking call
				if (!retCode)  {
					/* Event is signaled */
					//break;
					return WAIT_OBJECT_0;
				}
				else {
					/* check whether somebody else has the mutex */
					if (retCode == -1 && errno == EAGAIN ) {
						/* sleep for delay time */
						nanosleep(&delay, NULL);
						timeout++ ;
					}
					else{
						fprintf(stderr, "\n%s:%s", __FUNCTION__,strerror(errno));
							/* error  */
					}
				}
			}
			return WAIT_TIMEOUT;

		}
		else
		{
		}

    }

    return dwRet;
}
DWORD WaitForMultipleObjects(
		DWORD nCount,
		 HANDLE *lpHandles,
		BOOL bWaitAll, 
		DWORD dwMilliseconds)
{

	int tmp,i;
    DWORD dwRet = 0;
	HANDLE hHandle;
	DWORD timeout[MAXIMUM_WAIT_OBJECTS]={0};
	BOOL  bWaitAllFlag[MAXIMUM_WAIT_OBJECTS]={0};
	DWORD nCnt=nCount;

	if (nCount<1 && nCount>MAXIMUM_WAIT_OBJECTS)
	{
		return dwRet;
	}

	for (i=0;i<nCount && bWaitAll;i++)
	{
		bWaitAllFlag[i] = TRUE;
	}

	while (1)
	{
		for (i=0;i<nCount;i++)
		{
			hHandle = lpHandles[i];
			if(hHandle == INVALID_HANDLE_NUM)
				return FALSE;

			if (bWaitAllFlag[i] == FALSE)
			{
				continue;
			}
			
			if ((hHandle & 0x40000000) && (hHandle & 0x80000000))
			{
				tmp = EVENT_ID_NUM - hHandle;
				
				struct timespec ts;
				
/*
				struct timeval now;
				gettimeofday(&now, NULL);
				
				now.tv_usec += dwMilliseconds * 1000;
				
				ts.tv_sec = now.tv_sec + now.tv_usec / 1000000;
				ts.tv_nsec = (now.tv_usec % 1000000) * 1000;
*/
				clock_gettime(CLOCK_REALTIME, &ts);
				ts.tv_sec += dwMilliseconds / 1000;
				ts.tv_nsec += (dwMilliseconds % 1000) * 1000000;
				ts.tv_sec += ts.tv_nsec / 1000000000;
				ts.tv_nsec = ts.tv_nsec % 1000000000;

				pthread_mutex_lock(&sync_wait[tmp].mut);
				
				if(sync_wait[tmp].bInitialState)//signaled
				{
					usleep(100);
					if (!(sync_wait[tmp].bManualReset&= ~ EVENT_STOP_BIT))//FALSE auto-reset event object state to nonsignale
					{
						sync_wait[tmp].bInitialState = FALSE;
					}
					pthread_mutex_unlock(&sync_wait[tmp].mut);
					return 0;
				}
				
				if (dwMilliseconds == INFINITE)
				{
					pthread_cond_wait(&sync_wait[tmp].cond_wait, &sync_wait[tmp].mut);
				}
				else
				{
					dwRet = pthread_cond_timedwait(&sync_wait[tmp].cond_wait, &sync_wait[tmp].mut, &ts);
					dwRet = (sync_wait[tmp].bManualReset & EVENT_STOP_BIT) ? 0 : ETIMEDOUT; //appended by wy for bug Thu May  6 19:18:42 CST 2004
					sync_wait[tmp].bManualReset &= ~ EVENT_STOP_BIT; //appended by wy for bug Thu May  6 19:18:42 CST 2004
				}
				
				if (!sync_wait[tmp].bManualReset)//FALSE auto-reset event object state to nonsignaled
				{
					sync_wait[tmp].bInitialState = FALSE;
				}
				
				pthread_mutex_unlock(&sync_wait[tmp].mut);
			}
			else
			{
				if ((hHandle & 0x80000000))//thread
				{
					//thread
					tmp = 0xbfffffff - hHandle;
					if (th_creat_th[tmp])
					{
						pthread_join(th_creat_th[tmp], NULL);
						th_creat_th[tmp] = NULL;
					}
					
				}
				else if(hHandle & 0x40000000)//semaphore
				{
					int retCode;
					struct timespec delay;
					tmp = 0x7fffffff - hHandle;
					
					
					delay.tv_sec = 0;
					delay.tv_nsec = 1000000;  /* 1 milli sec */
					// Wait for the event be signaled
					
					retCode = sem_trywait(
						&sem_creat[tmp]); // event semaphore handle
					// non blocking call
					if (!retCode )  
					{
						/* Event is signaled */
						//fprintf(stderr, "wait sig\n");
						if (!bWaitAll)
						{
							return WAIT_OBJECT_0+i;
						}
						else
						{
							bWaitAllFlag[i] = FALSE;
							nCnt --;
							if(nCnt ==0)
								return WAIT_OBJECT_0;
							else
								continue;
						}
						
					}
					else 
					{
						/* check whether somebody else has the mutex */
						if (retCode == -1 && errno == EAGAIN ) 
						{
							/* sleep for delay time */
							nanosleep(&delay, NULL);
							timeout[i]++;
							if (timeout[i]>=dwMilliseconds)
							{
								//fprintf(stderr, "timeout\n");
								bWaitAllFlag[i] = FALSE;
								nCnt --;
								if(nCnt ==0)
									return WAIT_TIMEOUT;
								else
									continue;
							}
							else
								continue;
						}
						else{
							fprintf(stderr, "\n%s:%d:%s", __FUNCTION__,i,strerror(errno));
							/* error  */
						}
						
					}
					
				}
				else
				{
				}
				
			}
	}
	}
    return dwRet;

}

#undef  EVENT_STOP_BIT  // appended by wy 

BOOL SetThreadPriority(
    HANDLE hThread,
    int nPriority)
{
    return TRUE;
}

LONG InterlockedDecrement(LONG* lpdwAddend)
{
    pthread_mutex_t mu;
    pthread_mutex_init(&mu, NULL);
    pthread_mutex_lock(&mu);
    --*lpdwAddend;
    pthread_mutex_unlock(&mu);
    pthread_mutex_destroy(&mu);
    return *lpdwAddend;
}

LONG InterlockedIncrement(LONG* lpdwAddend)
{
    pthread_mutex_t mu;
    pthread_mutex_init(&mu, NULL);
    pthread_mutex_lock(&mu);
    ++*lpdwAddend;
    pthread_mutex_unlock(&mu);
    pthread_mutex_destroy(&mu);
    return *lpdwAddend;
}

typedef void (*LPTM_I_I_DW_DW_DW)(UINT, UINT, DWORD, DWORD, DWORD);

// changed by lun at 2002.7.19
#define TM_MAX 20

typedef struct _TimeEvent
{
    pthread_t th;
    BOOL bEnable;
    BOOL bExit;
    DWORD_PTR dwUser;
    UINT uDelay;
    pthread_cond_t cond;  //,startcond;
    pthread_condattr_t attr;
    LPTM_I_I_DW_DW_DW tmProc;
} TimeEvent;

static TimeEvent tmEvent[TM_MAX];
static pthread_mutex_t tmMut[TM_MAX];

static int nTmCount = 0;
void * tm_thread(void *arg);

MMRESULT timeSetEvent(
    DWORD uDelay,
    UINT uResolution,
    LPVOID lpTimeProc,
    DWORD_PTR dwUser,
    UINT fuEvent)
{
    int i;
    pthread_attr_t attr;

    for (i = 0;i < TM_MAX;i++)
    {
        if (tmEvent[i].bEnable == FALSE)
            break;
    }

    if (i >= TM_MAX) return 0;

    pthread_mutex_init(&tmMut[i], NULL);

    pthread_mutex_lock(&tmMut[i]);

   pthread_condattr_init(&tmEvent[i].attr);
  pthread_condattr_setclock(&tmEvent[i].attr, CLOCK_REALTIME);
    pthread_cond_init(&tmEvent[i].cond, &tmEvent[i].attr);

    pthread_attr_init(&attr);

    pthread_attr_setschedpolicy(&attr, SCHED_RR);

    tmEvent[i].bEnable = TRUE;

    tmEvent[i].bExit = FALSE;

    tmEvent[i].dwUser = dwUser;

    tmEvent[i].uDelay = uDelay;

    tmEvent[i].tmProc = (LPTM_I_I_DW_DW_DW)lpTimeProc;

    int retcode = pthread_create(&tmEvent[i].th, &attr, tm_thread, &i);

    if (retcode == 0)
    {
        pthread_cond_wait(&tmEvent[i].cond, &tmMut[i]);
        pthread_attr_destroy(&attr);
    }
    else
    {
        tmEvent[i].bEnable = FALSE;
        return -1;
    }

    pthread_mutex_unlock(&tmMut[i]);

    nTmCount++;

    return i + 1;
}

void * tm_thread(void *arg)
{
    int i = *((int *)arg), bRun = 1;
    int retcode;

    //struct timeval now, next;

    struct timespec timeout;

    pthread_mutex_lock(&tmMut[i]);
    pthread_cond_signal(&tmEvent[i].cond);
    pthread_mutex_unlock(&tmMut[i]);

    while (bRun)
    {

	/*
        gettimeofday(&now, NULL);
        //timeout.tv_sec=now.tv_sec;
        //timeout.tv_nsec=now.tv_usec*1000+tmEvent[i].uDelay*1000*1000; //masked by wy
        now.tv_usec += tmEvent[i].uDelay * 1000;
        timeout.tv_sec = now.tv_sec + now.tv_usec / 1000000;
        timeout.tv_nsec = (now.tv_usec % 1000000) * 1000;
	*/
	clock_gettime(CLOCK_REALTIME, &timeout);
	//fprintf(stderr,"%x,%d:%d:%d line=%d\n",tmEvent[i].tmProc,timeout.tv_sec,timeout.tv_nsec,tmEvent[i].uDelay,__LINE__);
	timeout.tv_sec += tmEvent[i].uDelay / 1000;
	timeout.tv_nsec +=(tmEvent[i].uDelay % 1000)*1000*1000; //added by yy^M
	timeout.tv_sec += timeout.tv_nsec / 1000000000;
	timeout.tv_nsec =timeout.tv_nsec % 1000000000; //added by yy^M
	//fprintf(stderr,"%x,%d:%d:%d line=%d\n",tmEvent[i].tmProc,timeout.tv_sec,timeout.tv_nsec,tmEvent[i].uDelay,__LINE__);
	
	//fprintf(stderr,"%x,%d line=%d\n",tmEvent[i].tmProc,tmEvent[i].bExit,__LINE__);
        if (tmEvent[i].bExit) bRun = 0;

	//fprintf(stderr,"%x,%d line=%d\n",tmEvent[i].tmProc,tmEvent[i].bExit,__LINE__);
        tmEvent[i].tmProc(0, 0, tmEvent[i].dwUser, 0, 0);
	//fprintf(stderr,"%x,%d line=%d\n",tmEvent[i].tmProc,tmEvent[i].bExit,__LINE__);

        pthread_mutex_lock(&tmMut[i]);

        {
            retcode = pthread_cond_timedwait(&tmEvent[i].cond, &tmMut[i], &timeout);

            if (retcode != ETIMEDOUT)
            {
	//fprintf(stderr,"%x,%d %d,line=%d\n",tmEvent[i].tmProc,tmEvent[i].bExit,retcode,__LINE__);
                bRun = 0;  //110 is ETIMEDOUT
            }
        }

        pthread_mutex_unlock(&tmMut[i]);
    }
	//fprintf(stderr,"%x,%d:%d:%d line=%d\n",tmEvent[i].tmProc,timeout.tv_sec,timeout.tv_nsec,tmEvent[i].uDelay,__LINE__);

    return (void *)0;
}


VOID timeKillEvent(UINT uTimerID)
{
    int i = uTimerID - 1;
   if ( uTimerID == 0)//no set time Event
{
    return;
}
    nTmCount--;

    pthread_mutex_lock(&tmMut[i]);

    if (tmEvent[i].uDelay != 8)
        pthread_cond_signal(&tmEvent[i].cond);


    tmEvent[i].bExit = TRUE;

    pthread_mutex_unlock(&tmMut[i]);

    pthread_join(tmEvent[i].th, NULL);

    tmEvent[i].bEnable = FALSE;
	pthread_condattr_destroy(&tmEvent[i].attr);

    pthread_cond_destroy(&tmEvent[i].cond);

    pthread_mutex_destroy(&tmMut[i]);
}

static int  pShm[4];

int get_shm_value(int flag)
{
    return -1;
}

void set_shm_value(int flag, int value)
{
    return;
}

void rm_shm()
{
    return;
}

//////////////////////////////////////////////////////////////////////
// Shared library operation

LPVOID LoadLibrary(LPCSTR lpLibFileName)
{
    void *hdl = dlopen(lpLibFileName, RTLD_NOW);

    if (hdl == NULL)
        fprintf(stderr, "LOADLIBRARY  %s\n", dlerror());

    return(hdl);
}

BOOL FreeLibrary(LPVOID hLibModule)
{    
		if (hLibModule == NULL)
        return FALSE;
    return(dlclose(hLibModule));
}


DWORD GetTempPath(
  DWORD nBufferLength,  // size of buffer
  LPTSTR lpBuffer       // path buffer
)
{
	sprintf(lpBuffer, "/usr/tmp");
	return sizeof(lpBuffer);
}

UINT GetTempFileName(
  LPCTSTR lpPathName,      // directory name
  LPCTSTR lpPrefixString,  // file name prefix
  UINT uUnique,            // integer
  LPTSTR lpTempFileName    // file name buffer
)
{
	sprintf(lpTempFileName,"%s/%s-XXXXXX",lpPathName,lpPrefixString);
	mkstemp(lpTempFileName);
	return sizeof(lpTempFileName);
}

LPVOID GetProcAddress(LPVOID hModule, LPCSTR lpProcName)
{
    return(dlsym(hModule, lpProcName));
}

//////////////////////////////////////////////////////////////////////
//I/O and file operation

int _lopen(LPCSTR lpPathName, int iReadWrite)
{
    return open(lpPathName, iReadWrite);
}

int _lcreat(LPCSTR lpPathName, int iAttribute)
{
    return open(lpPathName, O_CREAT | O_TRUNC | O_WRONLY, 0666);
}

UINT _lread(int fd, LPVOID lpBuffer, UINT uBytes)
{
    return read(fd, lpBuffer, uBytes);
}

UINT _lwrite(int fd, LPCSTR lpBuffer, UINT uBytes)
{
    return write(fd, lpBuffer, uBytes);
}

int  _lclose(int fd)
{
    return close(fd);
}

//int _lseek(int fd, LONG lOffset, int iOrigin)
//{
// return lseek(fd, lOffset, iOrigin);
//}

//++++START++++ appended by cqb for Linux Transplant
int _llseek(int fd, LONG lOffset, int iOrigin)
{
    return lseek(fd, lOffset, iOrigin);
}

//++++END++++ appended by cqb for Linux Transplant

// HFILE_ERROR == -1
int OpenFile(LPCSTR lpFileName, LPVOID lpReOpenBuff, UINT uStyle)
{
    int rn;

    if (lpReOpenBuff != NULL)
        ;

    if (uStyle & O_CREAT)
    {
        rn = open(lpFileName, uStyle, 0666);
    }
    else
    {
        rn = open(lpFileName, uStyle);
    }

    //if (rn != -1) 
    if ((rn != -1)&&(uStyle & (O_RDWR | O_WRONLY))) 
    {
        if (flock(rn, LOCK_EX|LOCK_NB) == -1)//for SC4856 by xzg 2010.6.24
        {
            fprintf(stderr, "\n%s", strerror(errno));
            close(rn);
            rn = -1;
        }
        else
            lseek(rn, 0, SEEK_SET);
    }

    return rn;
}

int CreateFile(LPCSTR lpFileName,
               DWORD dwDesiredAccess,
               DWORD dwShareMode,
               LPVOID lpSecurityAttributes,
               DWORD dwCreationDisposition,
               DWORD dwFlagsAndAttributes,
               LPVOID hTemplateFile)
{
    int rn;

    if ((dwCreationDisposition | dwDesiredAccess) & O_CREAT)
    {
        if (dwDesiredAccess & (O_RDWR | O_WRONLY))
            rn = open(lpFileName, O_CREAT | O_RDWR, 0666);
        else
            rn = open(lpFileName, O_CREAT, 0666);
    }
    else
    {
        if (dwDesiredAccess & (O_RDWR | O_WRONLY))
            rn = open(lpFileName, O_RDWR);
        else
            rn = open(lpFileName, O_RDONLY);
    }

    //if (rn != -1) 
    if ((rn != -1)&&(dwDesiredAccess & (O_RDWR | O_WRONLY))) 
    {   
        if (flock(rn, LOCK_EX|LOCK_NB) == -1)//for SC4856 by xzg 2010.6.24
        {
            fprintf(stderr, "\n%s", strerror(errno));
            close(rn);
            rn = -1;
        }
        else
            lseek(rn, 0, SEEK_SET);
    }

    return rn;
}

BOOL ReadFile(int fd,
              LPVOID lpBuffer,
              DWORD nNumberOfBytesToRead,
              LPDWORD lpNumberOfBytesRead,
              LPVOID lpOverlapped)
{
    if (lpOverlapped != NULL) ;

    ssize_t tmp = read(fd, lpBuffer, nNumberOfBytesToRead);

    if (tmp == -1)
    {
        fprintf(stderr, "\n%s", strerror(errno));
        return FALSE;
    }

    *lpNumberOfBytesRead = (DWORD)tmp;

    return TRUE;
}

BOOL WriteFile(int fd,
               LPCVOID lpBuffer,
               DWORD nNumberOfBytesToWrite,
               LPDWORD lpNumberOfBytesWritten,
               LPVOID lpOverlapped)
{
    if (lpOverlapped != NULL)
        ;

    ssize_t tmp = write(fd, lpBuffer, nNumberOfBytesToWrite);

    if (tmp == -1) return FALSE;

    *lpNumberOfBytesWritten = (DWORD)tmp;

    return TRUE;
}

DWORD GetFileSize(int fd,
                  LPDWORD lpFileSizeHigh)
{
// if (lpFileSizeHigh != NULL)
//  return 0xFFFFFFFF;

    struct stat st;

    if (fstat(fd, &st) == -1)
        return 0xFFFFFFFF;

    return (DWORD)st.st_size;

}

BOOL SetEndOfFile(int fd)
{
    off_t off = lseek(fd, 0, SEEK_CUR);

    if (off == -1) return FALSE;

    ftruncate(fd, off);

    return TRUE;
}

int FlushFileBuffers(int fd)
{
    return fsync(fd);
}

// lpDistanceToMoveHigh = NULL
DWORD SetFilePointer(int fd,
                     LONG lDistanceToMove,
                     PLONG lpDistanceToMoveHigh,
                     DWORD  dwMoveMethod)
{
// if (lpDistanceToMoveHigh != NULL)
//  return 0xFFFFFFFF;

    off_t off = lseek(fd, lDistanceToMove, dwMoveMethod);

    if (off == -1)
        return 0xFFFFFFFF;

    return off;
}

//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// Profile operation

//+++start+++ added by yy for Q5341-21,2016.01.13
#define LEFT_BRACE '['
#define RIGHT_BRACE ']'
static int newline(char c)
{
	return ('\n' == c ||  '\r' == c )? 1 : 0;
}
static int end_of_string(char c)
{
	return '\0'==c? 1 : 0;
}
static int left_barce(char c)
{
	return LEFT_BRACE == c? 1 : 0;
}
static int isright_brace(char c )
{
	return RIGHT_BRACE == c? 1 : 0;
}

static int parse_file(const char *section, const char *key, const char *buf,int *sec_s,int *sec_e,
					  int *key_s,int *key_e, int *value_s, int *value_e)
{
	const char *p = buf;
	int i=0;

	assert(buf!=NULL);
	assert(section != NULL && strlen(section));
	assert(key != NULL && strlen(key));
	int key_len=strlen(key);

	*sec_e = *sec_s = *key_e = *key_s = *value_s = *value_e = -1;

	while( !end_of_string(p[i]) ) {
		//find the section
		if( ( 0==i ||  newline(p[i-1]) ) && left_barce(p[i]) )
		{
			int section_start=i+1;

			//find the ']'
			do {
				i++;
			} while( !isright_brace(p[i]) && !end_of_string(p[i]));

			if( 0 == strncmp(p+section_start,section, i-section_start)) {
				int newline_start=0;

				i++;

				//Skip over space char after ']'
				while(isspace(p[i])) {
					i++;
				}

				//find the section
				*sec_s = section_start;
				*sec_e = i;

				while( ! (newline(p[i-1]) && left_barce(p[i])) 
				&& !end_of_string(p[i]) ) {
					int j=0;
					int max=0;
					//get a new line
					newline_start = i;

					while( !newline(p[i]) &&  !end_of_string(p[i]) ) {
						i++;
					}
					
					//now i  is equal to end of the line
					j = newline_start;

					if(';' != p[j]) //skip over comment
					{
						while(j < i && p[j]!='=') {
							j++;
							if('=' == p[j]) {
								max = (((key_len) >= (j-newline_start)) ? (key_len) : (j-newline_start));
								if(strncmp(key,p+newline_start,max)==0)
								{
									//find the key ok
									*key_s = newline_start;
									*key_e = j-1;

									*value_s = j+1;
									*value_e = i;

									return 1;
								}
							}
						}
					}

					i++;
				}
			}
		}
		else
		{
			i++;
		}
	}
	return 0;
}

BOOL WritePrivateProfileString(
    LPCTSTR lpAppName,
    LPCTSTR lpKeyName,
    LPCTSTR lpString,
    LPCTSTR lpFileName)
{

	//char buf[]={0};
	//char w_buf[MAX_FILE_SIZE]={0};
	int sec_s,sec_e,key_s,key_e, value_s, value_e;
	int file_size;
	FILE *out;
	//DWORD rtn = 0;

	//check parameters
	assert(lpAppName != NULL && strlen(lpAppName));
	assert(lpKeyName != NULL && strlen(lpKeyName));
	assert(lpString != NULL);
	assert(lpFileName !=NULL &&strlen(lpFileName));

	int value_len = (int)strlen(lpString);

	int fd = open(lpFileName, O_RDWR);

    if (fd == -1) return FALSE;

    struct stat st;

    if (fstat(fd, &st) == -1)
    {
        close(fd);
        return FALSE;
    }
	LPSTR buf = (LPSTR)malloc(st.st_size + 1);
	LPSTR w_buf = (LPSTR)malloc(st.st_size + MAX_PATH+1);
    if (buf == NULL || w_buf == NULL)
    {
        close(fd);
        return FALSE;
    }

    memset(buf, '\0', st.st_size + 1);
	memset(w_buf, '\0', st.st_size + +MAX_PATH +1);

    lseek(fd, 0, SEEK_SET);
    read(fd, buf, st.st_size);

	file_size = st.st_size;

	
	parse_file(lpAppName,lpKeyName,buf,&sec_s,&sec_e,&key_s,&key_e,&value_s,&value_e);
	

	if( -1 == sec_s)
	{
		if(0==file_size)
		{
			sprintf(w_buf+file_size,"[%s]\n%s=%s\n",lpAppName,lpKeyName,lpString);
		}
		else
		{
			//not find the section, then add the new section at end of the file
			memcpy(w_buf,buf,file_size);
			sprintf(w_buf+file_size,"\n[%s]\n%s=%s\n",lpAppName,lpKeyName,lpString);
		}
	}
	else if(-1 == key_s)
	{
		//not find the key, then add the new key=value at end of the section
		memcpy(w_buf,buf,sec_e);
		sprintf(w_buf+sec_e,"%s=%s\n",lpKeyName,lpString);
		sprintf(w_buf+sec_e+strlen(lpKeyName)+strlen(lpString)+2,buf+sec_e, file_size - sec_e);
	}
	else
	{
		//update value with new value
		memcpy(w_buf,buf,value_s);
		memcpy(w_buf+value_s,lpString, value_len);
		memcpy(w_buf+value_s+value_len, buf+value_e, file_size - value_e);
	}

	lseek(fd, 0, SEEK_SET);
	
	if(-1 == write(fd,w_buf,strlen(w_buf)))
	{
		close(fd);
		free(buf);
		free(w_buf);
		return FALSE;
	}

	close(fd);
	free(buf);
	free(w_buf);
	return TRUE;

}

BOOL WritePrivateProfileSection(
    LPCTSTR lpAppName,
    LPCTSTR lpString,
    LPCTSTR lpFileName)
{
    return 0;
}
//+++start+++ added by yy for Q5341-21,2016.01.13

#define PROFAPP 0
#define PROFKEY 1
#define PROFSEC 2
#if defined(OS_SUN)
#define NAME_MAX 64
#endif
int GetProfStr(char* sz, int* offset, char* buf, int length , int flag)
{
    int len = *offset, i, rtn = 0;
    char* szApp = sz;
    char* szKey = sz;

    for (; buf[len] && flag == PROFAPP;)   // Matching lpAppName
    {
        for (; buf[len] == ' '; len++) ;

        for (i = 0; tolower(buf[len]) == tolower(szApp[i]) && buf[len]; i++, len++) ;

        for (; buf[len] != '\n' && buf[len]; len++) ;

        if (buf[len] == '\n') len++;

        if (!szApp[i]) break;
    }

    for (; buf[len] && flag == PROFKEY;)   // Matching lpAppName
    {
        for (; buf[len] == ' '; len++) ;

        if (buf[len] == '[')
        {
            rtn = -1; // To next szApp
            break;
        }

        for (i = 0; tolower(buf[len]) == tolower(szKey[i]) && buf[len]; i++, len++) ;

        if (!szKey[i] && (buf[len] == ' ' || buf[len] == '=')) break;

        for (; buf[len] != '\n' && buf[len]; len++) ;

        if (buf[len] == '\n') len++;
    }

    if (flag == PROFKEY)
    {
        for (; (len < length) && buf[len] == ' '; len++) ;

        if ((len >= length) || buf[len++] != '=') rtn = -1;

        for (; (len < length) && buf[len] == ' '; len++) ;
    }

    for (; buf[len] && flag == PROFSEC;)   // Matching lpAppName
    {
        for (; buf[len] == ' '; len++) ;

        if (buf[len] == '[')
        {
            *offset = len;
            return len;
        }

        for (; buf[len] != '\n' && buf[len]; len++) ;

        if (buf[len] == '\n') len++;
    }

    *offset = len;

    if (!buf[len]) rtn = -1;

    return rtn;
}

UINT GetPrivateProfileInt(LPCTSTR lpAppName, LPCTSTR lpKeyName,
                          INT nDefault, LPCTSTR lpFileName)
{
    int rtn = nDefault;
    int fd = open(lpFileName, O_RDONLY);

    if (fd == -1) return rtn;

    struct stat st;

    if (fstat(fd, &st) == -1)
    {
        close(fd);
        return rtn;
    }

    LPSTR buf = (LPSTR)malloc(st.st_size + 1);

    if (buf == NULL)
    {
        close(fd);
        return rtn;
    }

    memset(buf, '\0', st.st_size + 1);

    lseek(fd, 0, SEEK_SET);
    read(fd, buf, st.st_size);

    INT len = 0;
    CHAR szApp[NAME_MAX], szKey[NAME_MAX];

    sprintf(szApp, "[%s]", lpAppName);

    if (GetProfStr(szApp, &len, buf, st.st_size , PROFAPP) < 0)
    {
        free(buf); //append by wy 六  3月 26 15:46:46 CST 2005
        close(fd);
        return rtn;
    }

    sprintf(szKey, "%s", lpKeyName);

    if (GetProfStr(szKey, &len, buf, st.st_size, PROFKEY) < 0)
    {
        free(buf); //append by wy 六  3月 26 15:46:46 CST 2005
        close(fd);
        return rtn;
    }

    // octal:0, decimal, hex:0x,0X,,,
    int base = 10;

    if (buf[len+1])
    {
        if (buf[len] == '0' && (buf[len+1] == 'x'
                                || buf[len+1] == 'X')) base = 16;
        else if (buf[len] == '0') base = 8;
    }

    rtn = strtoul(&buf[len], NULL, base);
    if (rtn == 0)
    {
	if (sscanf(&buf[len],"%d",&rtn)<1)
	{
		rtn  = nDefault;
	}else {
	rtn = 0;
	}
    }

    close(fd);
    free(buf); //append by wy 六  3月 26 15:46:46 CST 2005
    return rtn;
}

DWORD GetPrivateProfileString(
    LPCTSTR lpAppName, LPCTSTR lpKeyName,
    LPCTSTR lpDefault, LPTSTR lpReturnedString,
    DWORD nSize, LPCTSTR lpFileName)
{
    int j = 0;
    char str[1000];
    UINT i;
    INT len = 0;
    CHAR szApp[NAME_MAX], szKey[NAME_MAX];


if (!lpDefault)
	lpDefault = "";
    DWORD rtn = strlen(lpDefault);//midify by yy for The return value is the number of characters copied to the buffer, not including the terminating null character, 2011.09.06

    if (lpDefault) strcpy(lpReturnedString, lpDefault);

    int fd = open(lpFileName, O_RDONLY);

    if (fd == -1) return rtn;

    struct stat st;

    if (fstat(fd, &st) == -1)
    {
        close(fd);
        return rtn;
    }

    LPSTR buf = (LPSTR)malloc(st.st_size + 1);

    if (buf == NULL)
    {

        close(fd);
        return rtn;
    }

    memset(buf, '\0', st.st_size + 1);

    lseek(fd, 0, SEEK_SET);
    read(fd, buf, st.st_size);
    sprintf(szApp, "[%s]", lpAppName);

    if (GetProfStr(szApp, &len, buf, st.st_size, PROFAPP) < 0)
    {
        free(buf); //append by wy 六  3月 26 15:46:46 CST 2005
        close(fd);
        return rtn;
    }

    if (lpKeyName == NULL)
    {
        int  beg_sec = len;
        int  end_sec = len + 1;
        int  len_sec = 0;
        int  pre_msg;
        int  new_line_flag = 0;
        char cur_msg;
        char mask_msg = ';';
        char in_mask_process = 0;
		int temp;
        len_sec = GetProfStr(NULL, &end_sec, buf, st.st_size, PROFSEC);
		/****start added by tlg for MN-375**2012.05.02******/
		temp = beg_sec;
		while ( temp < end_sec )
		{
			if(buf[temp] != '\n' && buf[temp] != '\r' && buf[temp] !='\0' )
			{
				beg_sec = temp;
				break;
			}
			temp++;
		}
		/****end added by tlg for MN-375**2012.05.02******/
        for (i = 0; beg_sec < end_sec; beg_sec++)
        {
            if (in_mask_process)
            {
                if (buf[beg_sec] == '\n') in_mask_process = 0;
            }
            else
            {
                if (buf[beg_sec] == ';' || buf[beg_sec] == '=')
                {
                    if (i > 0)
                    {
                        if (lpReturnedString[i-1] != '\0')
                            lpReturnedString[i++] = '\0';
                    }

                    in_mask_process = 1;

                    continue;
                }

                if (!isblank(buf[beg_sec]) && buf[beg_sec] != '\r')
                {
                    if (buf[beg_sec] == '\n'  && (i > 0) && lpReturnedString[i-1] == '\0')//added by tlg for MN-375
                    {
                        continue;
                    }
                    else if (buf[beg_sec] == '\n')
                    {
                        lpReturnedString[i++] = '\0';
                    }
                    else
                    {
                        lpReturnedString[i++] = buf[beg_sec];
                    }
                }
            }
        }

        lpReturnedString[i++] = '\0';

        free(buf);
        close(fd);
        return i;


    }

    sprintf(szKey, "%s", lpKeyName);

    if (GetProfStr(szKey, &len, buf, st.st_size, PROFKEY) < 0)
    {
        free(buf);
        close(fd);
        return rtn;
    }

    for (i = 0; isprint(buf[len]) && buf[len] && i < nSize; len++, i++)
        lpReturnedString[i]  = buf[len];

    if (i < nSize) lpReturnedString[i] = '\0';
#if 0
    for (j = 0;j < 1000;j++)
    {
        if (lpReturnedString[j] == 32)
        {
            str[j] = '\0';
            continue;
        }
        else
        {
            str[j] = lpReturnedString[j];
        }
    }

    strcpy(lpReturnedString, str);
#endif


    close(fd);
    free(buf);

    return i;
}

//////////////////////////////////////////////////////////////////////
// Critical operation

void InitializeCriticalSection(LPCRITICAL_SECTION cri_mutex)
{
	pthread_mutexattr_t  attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);//For compatiable with windows. one thread can load multtime.
    pthread_mutex_init(cri_mutex,&attr);
		 pthread_mutexattr_destroy(&attr);
    return;
}

int EnterCriticalSection(LPCRITICAL_SECTION cri_mutex)
{
    if (pthread_mutex_lock(cri_mutex)) return -1;

    return 0;
}

int LeaveCriticalSection(LPCRITICAL_SECTION cri_mutex)
{
    if (pthread_mutex_unlock(cri_mutex)) return -1;

    return 0;
}

void DeleteCriticalSection(LPCRITICAL_SECTION cri_mutex)
{
    pthread_mutex_destroy(cri_mutex);
    return;
}

//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// Other operation

LONG InterlockedExchange(LONG* Target, LONG Value)
{
    pthread_mutex_t tmp_mutex = PTHREAD_MUTEX_INITIALIZER;

    if (pthread_mutex_lock(&tmp_mutex) != 0) return (DWORD) - 1;

    *Target = Value;

    if (pthread_mutex_unlock(&tmp_mutex) != 0) return (DWORD) - 1;

    return Value;
}

DWORD GetLastError()
{

    return errno;
}

DWORD GetCurrentDirectory(DWORD nBufferLength, LPTSTR lpBuffer)
{
    getcwd(lpBuffer, nBufferLength);
    return sizeof(lpBuffer);
}

UINT GetWindowsDirectory(LPTSTR lpBuffer, UINT uSize)
{
    sprintf(lpBuffer, "/usr/lib");
    return sizeof(lpBuffer);
}

//+++start+++ add by byl 2010/3/16
int  FindFirstFile(const CHAR *dirname,WIN32_FIND_DATA *pFindData)
{
    DIR * dir;

	dir = opendir(dirname);
	if(!dir) 
	{ 
	    return INVALID_HANDLE_VALUE;
	}
	
    closedir(dir);
	return TRUE;
}

int  CreateDirectory(const CHAR *dirname,LPTSTR lpBuffer)
{
    int nRet;

	nRet = mkdir(dirname,0777);
    if(nRet == INVALID_HANDLE_VALUE)
    {
       return 0;
	}

	return TRUE;
}

void FindClose(int hFileSearch)
{


}
//+++end+++ add by byl 2010/3/16

char* strupr(char* string)
{
    int i = 0;
    char c;

    while (string[i] != '\0')
    {
        c = (char)toupper((int)string[i]);
        string[i] = c;
        i++;
    }

    return string;
}

//added by yy for Q5341-1
char* strlwr(char* string)
{
    int i = 0;
    char c;

    while (string[i] != '\0')
    {
        c = (char)tolower((int)string[i]);
        string[i] = c;
        i++;
    }

    return string;
}

char* lstrcpyn(char* s1 , const char* s2 , size_t nLen)
{
    return strncpy(s1 , s2 , nLen);
}

COMPAT_CPP BOOL
	GlobalMemoryStatusEx(
	LPMEMORYSTATUSEX lpBuffer
	)
{
	int nResult;
	struct sysinfo info;
	nResult = sysinfo(&info);
	lpBuffer->ullTotalPhys = info.totalram;
	return TRUE;
}
LPVOID GlobalAlloc(
    UINT uFlags,
    size_t dwBytes)
{

    switch (uFlags)
    {
    case GMEM_FIXED:
    {
        return malloc((size_t)dwBytes);
    }

    case GMEM_FIXED|GMEM_ZEROINIT:
    {
        return calloc(sizeof(char), (size_t)dwBytes);
    }

    default:
    {
        return calloc(sizeof(char), (size_t)dwBytes);
    }
    }
}

size_t GlobalSize(
		  HGLOBAL hMem   // handle to global memory object
		)
{
	return malloc_usable_size(hMem);
}

LPVOID GlobalFree(LPVOID hMem)
{

    free(hMem);
    return NULL;
}

LPVOID GlobalLock(LPVOID hMem)
{
    return hMem;
}

BOOL GlobalUnlock(LPVOID hMem)
{
    return TRUE;
}

LPVOID LocalLock(LPVOID hMem)
{
    return hMem;
}

BOOL LocalUnlock(LPVOID hMem)
{
    return TRUE;
}

int closesocket(int s)
{
    close(s);
    return 0;
}

void CopyMemory(void *dest, const void *src, DWORD count)
{
    memcpy(dest, src, (size_t)count);
}

VOID GetLocalTime(
    LPSYSTEMTIME lpSystemTime)
{
    LPSYSTEMTIME st = lpSystemTime;
    //time_t t;
    //time(&t);

    struct tm *ptm;
    //ptm = localtime(&t);//masked by yy for SC-5160
	struct timeval valtime;
	struct tm  result;
	gettimeofday(&valtime, NULL);
    ptm = localtime_r(&valtime.tv_sec,&result);//add by yy for SC-5160 save thread

    st->wYear = (WORD)ptm->tm_year + 1900;
    st->wMonth = (WORD)ptm->tm_mon + 1;
    st->wDayOfWeek = (WORD)ptm->tm_wday;
    st->wDay = (WORD)ptm->tm_mday;
    st->wHour = (WORD)ptm->tm_hour;
    st->wMinute = (WORD)ptm->tm_min;
    st->wSecond = (WORD)ptm->tm_sec;
    //st->wMilliseconds = 0;//masked by yy for SC-5160
    st->wMilliseconds = (WORD)(valtime.tv_usec / 1000);//added by yy for SC-5160

// asctime(st);
}

int  WSAGetLastError(void)
{
    return errno;
}

#if !defined(MSG_NOSIGNAL)
#define MSG_NOSIGNAL 0 //for solaris 
#endif
int WSASend(
    SOCKET s,
    LPWSABUF lpBuffers,
    DWORD dwBufferCount,
    LPDWORD lpNumberOfBytesSent,
    DWORD dwFlags,
    LPVOID lpOl,
    LPVOID lpComp)
{
    DWORD i;
    int j = 0, rtn;

    for (i = 0; i < dwBufferCount; i++)
        j = send(s, lpBuffers[i].buf, lpBuffers[i].len, MSG_NOSIGNAL | MSG_DONTWAIT);


    /*changeby wy  for  预防 pipe broken, 以防写socket时，程式被SIGPIPE 中断.*/

    *lpNumberOfBytesSent = j;

    rtn = j < 0 ? SOCKET_ERROR : j;

    return rtn;
}

int WSARecv(
    SOCKET s,
    LPWSABUF lpBuffers,
    DWORD dwBufferCount,
    LPDWORD lpNumberOfBytesRecvd,
    LPDWORD lpFlags,
    LPVOID lpOl,
    LPVOID lpComp)
{
    DWORD i;
    int j = 0, rtn;

    for (i = 0; i < dwBufferCount; i++)
        j = recv(s, lpBuffers[i].buf, lpBuffers[i].len, MSG_NOSIGNAL | MSG_DONTWAIT); // appended by wy for Linux Transplant

    *lpNumberOfBytesRecvd = j;

    if (j <= 0) rtn = SOCKET_ERROR;

    rtn = j <= 0 ? SOCKET_ERROR : j;

    return rtn;
}

//++++++++START++++++++  appended  by wy  for parts of WinSock32 API  30 09:15:49 CST 2004
//macro defined

//#define BASEADDR                     0xffffff00
//#define WSABASEADDR                 (BASEADDR+MAX_EVENTS)
#define GETSOCKETERROR()     (errno)
#define SETSOCKETERROR(x)     ((errno)=(x))
#define ISSOCKETERROR(x)        (errno == (x))

//#define  GSKARRAY         g_socket_fd_array
//#define  GEMARRAY                      g_event_msk_array
//#define  GNEARRAY         g_net_event_array

//#define  IDX_TO_SOCKET(index)          GSKARRAY[(index)]
//#define  IDX_TO_SOCKET_MSK(index)      GEMARRAY[(index)]
//#define  IDX_TO_NET_EVENT(index)       GNEARRAY[(index)]

#define  ISMASKSET(fdmsk,msk)         ((fdmsk) & (msk))
#define  ISNMASKSET(fdmsk, msk)  (!(ISMASKSET(fdmsk,msk)))
#define  MAXFD(fd1,fd2)   (((fd1) > (fd2))? (fd1) : (fd2))

#define  SET_NET_EVENT(WSAEObj, msk)   (WSAEObj->ne.lNetworkEvents |= (msk))
#define  CLR_NET_EVENT(WSAEObj, msk)   (WSAEObj->ne.lNetworkEvents &= ~(msk))

#define  IS_NET_EVENT_SET(WSAEObj, msk)  (WSAEObj->ne.lNetworkEvents & msk)

#define  SET_ERR_CODE(WSAEObj,msk) \
    ((WSAEObj->ne.iErrorCode[(msk##_BIT)])=GETSOCKETERROR())

#define  SET_ERR_CODEEX(WSAEObj,msk,bit_err) \
    ((WSAEObj->ne.iErrorCode[(msk##_BIT)])=(bit_err))

#define  CLR_ERR_CODE(WSAEObj,msk) ((WSAEObj->ne.iErrorCode[(msk##_BIT)])=0)
//#define  ETOINDEX(event)  (WSABASEADDR -(event))
//#define  ETOINDEX(event)  (0xffffff60 -(event))



//end  macro defined


/**********************Global start************************/
static WSANETWORKEVENTS   clean_net_event;
/********************Global-end**********************/



static inline void
ERR_SET_BY_WSAEVENT(WSAEVENT WsaEvent , int shift_bit, int bit_value)
{
#if 0
    int iterator = 0;

    do
    {
        if (IDX_TO_SOCKET(iterator) == s)
        {
            IDX_TO_NET_EVENT(iterator).lNetworkEvents = (1 << shift_bit);
            IDX_TO_NET_EVENT(iterator).iErrorCode[shift_bit] = bit_value;
            break;
        }
    }
    while ((++iterator) < MAX_EVENTS);

#endif
	if(WsaEvent)
	{
		
		WsaEvent->ne.lNetworkEvents = (1 << shift_bit);
	
		WsaEvent->ne.iErrorCode[shift_bit] = bit_value;
	}

}

#if 0
static  inline int
Get_event_by_fd(SOCKET s)
{
    int iterator = 0;

    do
    {
        if (IDX_TO_SOCKET(iterator) == s) return iterator;
    }
    while ((++iterator) < MAX_EVENTS);

    return -1;  //no match fd in array;
}

#endif


static inline int
check_s_close(SOCKET s)
{
    char pbuf;
    return recv(s, &pbuf, 1, MSG_DONTWAIT | MSG_PEEK | MSG_NOSIGNAL);
}


static inline void
RECVPEEK(SOCKET s, WSAEVENT event)
{

    switch (check_s_close(s))
    {
    case  0:
        SET_NET_EVENT(event, FD_CLOSE);
        break;

    case  -1:

        if (IS_NET_EVENT_SET(event, FD_CONNECT))
            SET_ERR_CODE(event, FD_CONNECT);
        else
        {
            SET_NET_EVENT(event, FD_READ);
            SET_ERR_CODE(event, FD_READ);
        }

        break;

    default:
        SET_NET_EVENT(event, FD_READ);
    }
}


#if 0
static inline void
CPNETEVENTTOUSER(LPWSANETWORKEVENTS p, int event)
{
    LPWSANETWORKEVENTS  p_network_event = p;
    *p = IDX_TO_NET_EVENT(event);
    IDX_TO_NET_EVENT(event) = IDX_TO_NET_EVENT(MAX_EVENTS); //clear the dirty struct entry
}

#endif

#define  FIXME(arg) DEBUG(arg)
#define  DEBUG(args...) \
    do{ \
        fprintf(stderr,"\nDEBUG===>%s--line--%i,%s", __FILE__,__LINE__,## args); \
    }while(0)


#if 0

static inline int
NBCONNECT(SOCKET s, int timeout)
{

    struct timeval timer;
    fd_set  r_fdset;
    FD_ZERO(&r_fdset);
    timer.tv_sec = 0;
    timer.tv_usec = timeout * 1000;

    switch (select(s + 1, &r_fdset, NULL, NULL, &timer))
    {
    case 0:
    case -1:
        ERR_SET_BY_FD(s, FD_CONNECT_BIT, errno);
        return SOCKET_ERROR;

    default:

        if (check_s_close(s) != 1)
        {
            ERR_SET_BY_FD(s, FD_CONNECT_BIT, errno);
            return SOCKET_ERROR;
        }

        ERR_SET_BY_FD(s, FD_CONNECT_BIT, 0);

        return !SOCKET_ERROR;
    }
}

#endif


static inline int
check_s_status(SOCKET s)
{
    int optvalue;
    socklen_t optlen = sizeof(int);

    switch (getsockopt(s, SOL_SOCKET, SO_ERROR, &optvalue, &optlen))
    {
    case -1:
        return  errno;

    default:
        return optvalue;
    }

}


void WSAStart(void)
{

    //pthread_mutex_init(&wsa_create_event_mut, NULL);
}

void  WSAClean(void)
{
#if 0
    int iterator = 0;
    pthread_mutex_lock(&wsa_create_event_mut);

    for (; iterator < MAX_EVENTS;)
    {
        IDX_TO_SOCKET_MSK(iterator) = 0;
        IDX_TO_NET_EVENT(iterator) = IDX_TO_NET_EVENT(MAX_EVENTS); //clear the struct item
        IDX_TO_SOCKET(iterator++) = 0;
    }

    pthread_mutex_unlock(&wsa_create_event_mut);

#endif

    //pthread_mutex_destroy(&wsa_create_event_mut);
}

int  WSACleanup(void)
{

    WSAClean();

    return 0;
}

int WSAStartup(WORD wVersionRequested, LPWSADATA lpWSAData)
{

    WSAStart();

    return 0;
}

WSAEVENT  WSACreateEvent(void)
{
#if 0
    int iterator = 0;

    pthread_mutex_lock(&wsa_create_event_mut);

    for (; IDX_TO_SOCKET_MSK(iterator);)
    {
        iterator++;
    }

    if (iterator < MAX_EVENTS)
    {
        IDX_TO_SOCKET_MSK(iterator) = 1;
        pthread_mutex_unlock(&wsa_create_event_mut);
        return ETOINDEX(iterator);
    }

    pthread_mutex_unlock(&wsa_create_event_mut);

    return 0;

#endif
    WSAEVENT ptr = (WSAEVENT)malloc(sizeof(struct tagWSAEVENT));

    if (!ptr)
    {
        fprintf(stderr, "Error WSACreateEvent %i", errno);
        return (WSAEVENT)0;
    }

    memset(ptr, 0, sizeof(struct tagWSAEVENT));

    return ptr;
}


BOOL   WSACloseEvent(WSAEVENT event)
{
#if 0
    int iterator = ETOINDEX(event);
    pthread_mutex_lock(&wsa_create_event_mut);

    if (valid_wsaevent(event, MAX_EVENTS) && IDX_TO_SOCKET_MSK(iterator))
    {

        IDX_TO_SOCKET_MSK(iterator) = 0;
        IDX_TO_SOCKET(iterator) = 0;
        pthread_mutex_unlock(&wsa_create_event_mut);
        return TRUE;
    }

    pthread_mutex_unlock(&wsa_create_event_mut);

    return FALSE;
#endif
    WSAEVENT ptr = event;

    if (ptr)
    {
        free(ptr);
        return TRUE;
    }

    fprintf(stderr, "WSACloseEvent event is NULL");

    return FALSE;

}


 int WINAPI WSAAccept(
		 IN SOCKET s,
		 OUT struct sockaddr FAR * addr,
		 IN OUT LPINT addrlen,
		 IN LPCONDITIONPROC lpfnCondition,
		 IN DWORD_PTR dwCallbackData
		 )
{

	GROUP g;
	int ret_cond,ret_acce;
	if(lpfnCondition)
	{
		LPWSABUF lpCallerId  = (LPWSABUF)malloc(sizeof(WSABUF));
		memset(lpCallerId, 0, sizeof(WSABUF));

		lpCallerId->buf = (char *)addr;
		lpCallerId->len =strlen((char *)addr);

		ret_cond = ((LPCONDITIONPROC)lpfnCondition)(lpCallerId,NULL,0,0,NULL,NULL,&g,0);
		free(lpCallerId);
		switch(ret_cond)
		{
			case CF_REJECT:
				return WSAECONNREFUSED;
			case CF_ACCEPT:
				ret_acce =	accept(s,addr,(socklen_t*)addrlen);
				return ret_acce;
		}
	}
	else
	{

		ret_acce =	accept(s,addr,(socklen_t*)addrlen);
		return ret_acce;
	}


}

int WSAConnect(SOCKET s,  const struct sockaddr* servername, int namelen,
               LPWSABUF lpCallerData, LPWSABUF   lpCalleeData,
               LPQOS lpqosdata, WSAEVENT WsaEvent)
{
    int ret_conn;

    if (WsaEvent && (WsaEvent->fd != s))
    {
        fprintf(stderr, "WSAConnect(socket != WsaEvent->fd");
        return SOCKET_ERROR;
    }

    ret_conn = connect(s, servername, namelen);

    switch (ret_conn)
    {
    case -1:

        if (ISSOCKETERROR(EISCONN))
        {
#if 0
            ERR_SET_BY_FD(s, FD_CONNECT_BIT, 0);
#endif
            ERR_SET_BY_WSAEVENT(WsaEvent, FD_CONNECT_BIT, 0);
            return !SOCKET_ERROR;
        }
        else
        {
#if 0
            ERR_SET_BY_FD(s, FD_CONNECT_BIT, errno);
#endif
            ERR_SET_BY_WSAEVENT(WsaEvent, FD_CONNECT_BIT, errno);
            return SOCKET_ERROR;

        }

    default:

#if 0
        ERR_SET_BY_FD(s, FD_CONNECT_BIT, 0);
#endif
        ERR_SET_BY_WSAEVENT(WsaEvent, FD_CONNECT_BIT, 0);
        return  !SOCKET_ERROR;
    }

}


int
WSAEventSelect(SOCKET client, WSAEVENT event, int  msk)
{
#if 0
    int index;

    if (!valid_wsaevent(event, MAX_EVENTS) || client < 0 || !msk)
    {
        return SOCKET_ERROR;
    }

    index = ETOINDEX(event) ;

    IDX_TO_SOCKET(index) = client;
    IDX_TO_SOCKET_MSK(index) = msk;
    return  !SOCKET_ERROR;
#endif

    if (client < 0 || !msk || !event)
    {
        return SOCKET_ERROR;
    }

    event->fd = client;

    event->msk = msk;
    return !SOCKET_ERROR;
}


int WSAEnumNetworkEvents(SOCKET s , WSAEVENT event, LPWSANETWORKEVENTS lpNetworkEvent)
{
#if 0

    if (!valid_wsaevent(event, MAX_EVENTS)) return SOCKET_ERROR;

    if (IDX_TO_SOCKET(ETOINDEX(event)) != s)
    {
        return SOCKET_ERROR;
    }

    CPNETEVENTTOUSER(lpNetworkEvent, ETOINDEX(event));

    return !SOCKET_ERROR;
#endif

    if (!event || (event->fd != s) || !lpNetworkEvent) return SOCKET_ERROR;

    *lpNetworkEvent = event->ne;

    event->ne = clean_net_event;

    return !SOCKET_ERROR;


}

int  WSAWaitForMultipleEvents(DWORD totals , const WSAEVENT *eventarray, BOOL unspd1,
                              DWORD timeout, BOOL unspd2)
{

    WSAEVENT out_event, *p_event_array = (WSAEVENT*)eventarray;
    int    ret_code;
    int   ret;
    int   select_ret;
    int   iterator, highfd = 0;
    SOCKET    socket_fd = 0;
    LONG       fd_msk = 0;
    int     sock_err;
    unsigned short  g_inc = 0;
    unsigned short index;

    struct timeval   timer;
    fd_set   r_fdset, w_fdset, e_fdset ;
    FD_ZERO(&r_fdset);
    FD_ZERO(&w_fdset);
    FD_ZERO(&e_fdset);

    if (timeout)
    {
        timer.tv_sec = 0;
        timer.tv_usec = (timeout * 1000);
    }

    for (iterator = 0;  iterator < totals ; iterator++)
    {
        out_event = p_event_array[iterator];
        socket_fd = out_event->fd;
        fd_msk = out_event->msk;

        if (ISMASKSET(fd_msk, (FD_ACCEPT | FD_READ | FD_CLOSE)))
        {
            FD_SET(socket_fd, &r_fdset);
            highfd = MAXFD(socket_fd, highfd);
        }

        if (ISMASKSET(fd_msk, (FD_WRITE | FD_CONNECT)))
        {
            FD_SET(socket_fd,   &w_fdset);
            highfd = MAXFD(socket_fd, highfd);

        }

        if (ISMASKSET(fd_msk, (FD_CLOSE)))
        {
            FD_SET(socket_fd,   &e_fdset);
            highfd = MAXFD(socket_fd, highfd);
        }


    }

    if (iterator == 0)
        ret_code = select(0, NULL, NULL, NULL, &timer);
    else
        ret_code = select(highfd + 1, &r_fdset, &w_fdset, NULL, &timer);

    switch (ret_code)
    {
    case 0:
        return  WSA_WAIT_TIMEOUT;

    case -1:

        return WSA_WAIT_FAILED;

    default:
        select_ret = ret_code;

        for (iterator = 0; iterator < totals ; iterator++)

        {
            index   = (g_inc++) % (totals);
            out_event = p_event_array[index];
            socket_fd = out_event->fd;
            fd_msk = out_event->msk;
            ret = 0;

            if (FD_ISSET(socket_fd, &r_fdset))
            {
                if (ISMASKSET(fd_msk, FD_ACCEPT))
                {
                    sock_err = check_s_status(socket_fd);
                    SET_NET_EVENT(out_event, FD_ACCEPT);
                    SET_ERR_CODEEX(out_event, FD_ACCEPT, sock_err);
                    continue;

                }
                else if (ISMASKSET(fd_msk, FD_READ | FD_CLOSE))
                {
                    if (out_event->ne.lNetworkEvents & FD_CONNECT)
                    {
                        continue;
                    }

                    RECVPEEK(socket_fd, out_event);
                }

            }

            if (FD_ISSET(socket_fd, &w_fdset))
            {
                sock_err = check_s_status(socket_fd);

                if (out_event->ne.lNetworkEvents & FD_CONNECT)
                {
                    SET_ERR_CODEEX(out_event, FD_CONNECT, sock_err);
                    return index;
                }
                else if (ISMASKSET(fd_msk, (FD_WRITE)))
                {
                    if (!IS_NET_EVENT_SET(out_event, FD_CLOSE))
                    {
                        SET_NET_EVENT(out_event, FD_WRITE);
                        SET_ERR_CODEEX(out_event, FD_WRITE, sock_err);
                    }
                }

            }

        }

        return select_ret;
    }

}

//+++++++++++++END+++++++++++++ appended by wy for WinSock32 parts of API 30 09:16:59 CST 2004


//++++++START++++++++  IPC  Share Memory API implemation parts of MapFile WINAPI
//add by wy  for linux transplant  Thu May  6 18:20:38 CST 2004

typedef struct tagMapFile
{
int used_ids: 1;
int shm_ids: 1;
int b_viewed: 1;
int destory_self: 1;
    void*   pshm;
    union { int     handler; int     shmid; };
}MapFile_t, *lpMapFile_t;


#define   MAX_MAP          10
#define   MAPBASEADDR          0xffffff00
#define   ITERTOINDEX(iter)  (MAPBASEADDR - (iter))
#define   UP(mutex)  pthread_mutex_lock(mutex)
#define   DOWN(mutex)    pthread_mutex_unlock(mutex)
#define   GMAPARRAY      g_MapFile_array
#define   MAPARRAY(iter)      GMAPARRAY[(iter)]
#define   FILE_DIRECTORY      "/tmp"
static MapFile_t   g_MapFile_array[MAX_MAP+1];
static      pthread_mutex_t  g_map_file_mut;

static  inline BOOL  valid_handle(HANDLE  testobj)
{

    return  ITERTOINDEX(testobj) >= 0 ? (ITERTOINDEX(testobj) < MAX_MAP ? TRUE : FALSE) : FALSE;

}

static  inline BOOL  valid_iterator(int  iterator)
{

    return  iterator >= 0 ? (iterator < MAX_MAP ? TRUE : FALSE) : FALSE;
}

static  inline  int find_empty4MapFile(lpMapFile_t* lparam, int max)
{
    int iterator = 0;
    lpMapFile_t*  p = lparam;

    for (; (*p)->used_ids; (*p)++)
        iterator++;

    if (iterator < max)
    {
        (*p)->used_ids = 1;
        return ITERTOINDEX(iterator);
    }

    return 0;
}

static inline  HANDLE create_handle4MapFile(HANDLE fd, BOOL bShm, BOOL destory_self)
{
    static BOOL  bInit = FALSE;
    HANDLE      ret_handle;

    lpMapFile_t lpMapFile = (lpMapFile_t)g_MapFile_array;

    bInit ? : pthread_mutex_init(&g_map_file_mut, NULL), bInit = TRUE;

    UP(&g_map_file_mut);

    if ((ret_handle = find_empty4MapFile(&lpMapFile, MAX_MAP)) != 0)
    {
        lpMapFile->shm_ids = 1;
        lpMapFile->shmid = fd;
        lpMapFile->destory_self = destory_self;
    }

    DOWN(&g_map_file_mut);

    return  ret_handle;

}


static  inline  BOOL is_shm_ids_true(HANDLE handler)
{

    return MAPARRAY(ITERTOINDEX(handler)).shm_ids ? TRUE : FALSE;
}

static  inline BOOL  is_first_mapping(HANDLE  handler)
{
    return !MAPARRAY(ITERTOINDEX(handler)).b_viewed;
}


static  inline int get_shmid_from_h(HANDLE  handler)
{

    return MAPARRAY(ITERTOINDEX(handler)).shmid;
}

static inline  int get_iter_from_p(LPVOID p)
{
    int   iterator = 0;

    do
    {
        if (MAPARRAY(iterator).pshm == p)  return iterator;
    }
    while (iterator++ < MAX_MAP);

    return -1;  //don't find equal pointer
}

static inline   BOOL is_destroy_self_set(HANDLE  handler)
{
    return  MAPARRAY(ITERTOINDEX(handler)).destory_self;
}

static inline void clear_handle4MapFile(HANDLE  handler)
{

    UP(&g_map_file_mut);

    if (is_destroy_self_set(handler))
        shmctl(get_shmid_from_h(handler), IPC_RMID, 0);

    MAPARRAY(ITERTOINDEX(handler)) = MAPARRAY(MAX_MAP);

    DOWN(&g_map_file_mut);
}



#define   SVSSHM_MOD        (SHM_R|SHM_W|SHM_R>>3|SHM_R>>6)
HANDLE    CreateFileMapping(HANDLE hfile, LPVOID attr, DWORD flags, DWORD low,
                            DWORD  high, LPCSTR objname)
{
    key_t  key;
    int    shmid, fd;
    char   filename[256];

    switch (hfile)
    {
    case -1:

        if (NULL == objname) return 0;

        sprintf(filename, "%s/.%s", FILE_DIRECTORY, objname);

        if ((fd = open(filename, O_CREAT, 00666)) < 0)
        {
            //fprintf(stderr,"\n%s:%s:%i, errno=%i",__FILE__,__FUNCTION__,__LINE__,errno);
            return 0;
        }

        close(fd);

        if ((key = ftok(filename, 0)) == -1)
        {
            //fprintf(stderr,"\n%s:%s:%i, errno=%i",__FILE__,__FUNCTION__,__LINE__,errno);
            return 0;
        }

        if ((shmid = shmget(key, high, 00666 | IPC_CREAT)) == -1)
        {
            //fprintf(stderr,"\n%s:%s:%i, errno=%i",__FILE__,__FUNCTION__,__LINE__,errno);
            return 0;
        }

        return create_handle4MapFile(shmid, TRUE, TRUE);

    default :
        return 0;  //share memory support by mmap will be added in next version

    }

}

HANDLE   OpenFileMapping(DWORD  access, BOOL  inherit, LPCSTR objname)
{
    key_t  key;
    char   filename[256];
    int    shmid,  shmflg = 0;

    if (objname == NULL) return 0;

    sprintf(filename, "%s/.%s", FILE_DIRECTORY, objname);

    if ((key = ftok(filename, 0)) < 0) return 0;

    if (access & FILE_MAP_READ)  shmflg |= SHM_R;
    else shmflg |= (SHM_W | SHM_R);

    if ((shmid = shmget(key, 0, shmflg)) == -1) return 0;

    return create_handle4MapFile(shmid, TRUE, FALSE);



}

static inline  void*  process_ref(MapFile_t* parray, void* p)
{
    MapFile_t*  sparray = parray;
    sparray->b_viewed = TRUE;
    return   sparray->pshm = p;
}


LPVOID    MapViewOfFile(HANDLE hobj, DWORD flags, DWORD offset_high, DWORD  offset_low, DWORD count)
{
    void* rc;
    int shmid, shmflg = 0;
    HANDLE  iterator = hobj;
#if 0

    if (!valid_handle(iterator) || !is_first_mapping(iterator)
            || !is_shm_ids_true(iterator))
#endif
        if (!valid_handle(iterator))
        {
            //fprintf(stderr,"%s:%s%i",__FILE__,__FUNCTION__,__LINE__);
            return NULL;
        }
        else if (!is_first_mapping(iterator))
        {
            //fprintf(stderr,"%s:%s%i",__FILE__,__FUNCTION__,__LINE__);
            return NULL;
        }
        else if (!is_shm_ids_true(iterator))
        {
            //fprintf(stderr,"%s:%s%i",__FILE__,__FUNCTION__,__LINE__);
            return NULL;
        }

    shmid = get_shmid_from_h(iterator);

    if (flags & FILE_MAP_READ)  shmflg |= SHM_RDONLY;

    if ((rc = (void*)shmat(shmid, NULL, shmflg)) == (void*) - 1)
    {
        return NULL;
    }

    MapFile_t*  lpMapFile = &g_MapFile_array[ITERTOINDEX(iterator)];

    return  process_ref(lpMapFile, (void*)rc) ;

}


BOOL     UnmapViewOfFile(LPVOID addr)
{
    return !shmdt(addr);
}
BOOL FlushViewOfFile(
		  LPCVOID lpBaseAddress,         // starting address
		    SIZE_T dwNumberOfBytesToFlush  // number of bytes in range
		)
{
	return TRUE;

}


BOOL CloseHandle(int fd)
{

	if(fd == INVALID_HANDLE_NUM)
		return FALSE;
    if ((fd & 0x40000000) && (fd & 0x80000000))//event
    {
        int tmp = EVENT_ID_NUM - fd;

        if (tmp < 16)
        {
		pthread_condattr_destroy(&sync_wait[tmp].condattr);
		pthread_cond_destroy(&sync_wait[tmp].cond_wait);
            pthread_mutex_destroy(&sync_wait[tmp].mut);
            return TRUE;
        }
        else if (valid_handle(fd)) clear_handle4MapFile(fd);

    }
    else
    {
        if ((fd & 0x80000000))//thread
        {
			//thread
        }
		else if(fd & 0x40000000)//semaphore
		{
			int tmp;
			tmp = 0x7fffffff - fd;

			if(sem_destroy(&sem_creat[tmp]) < 0)   // Event semaphore handle
				return FALSE;
			//sem_creat[tmp] = NULL; 

		}
		else
		{
            if (close(fd) < 0) return FALSE;
		}
			
    }

    return TRUE;
}

BOOL DeviceIoControl(
    int fd,
    DWORD dwIoControlCode,
    LPVOID lpInBuffer,
    DWORD nInBufferSize,
    LPVOID lpOutBuffer,
    DWORD nOutBufferSize,
    LPDWORD lpBytesReturned,
    BYTE lpKnlBId)
{
    DEVIOCTL dev;
    BOOL rn = TRUE;

    dev.InAddr = (u32*)lpInBuffer;
    dev.InSize = (u32)nInBufferSize;
    dev.OutAddr = (u32*)lpOutBuffer;
    dev.OutSize = (u32)nOutBufferSize;
    dev.RtnAddr = (u32*)lpBytesReturned;

    if (lpKnlBId == (BYTE)NULL)
    {
        dev.uKnlBId = 0;
    }
    else
    {
        dev.uKnlBId = lpKnlBId;
    }

#if 0
    fprintf(stderr, "\ndwioctl sizeof(dev)=%u", sizeof(dev));

    fprintf(stderr, "\nioctl :0x%x"
            "\n dev.InAddr:%p,dev.InSize:%u"
            "\n dev.ouAddr:%p,dev.ouSize:%u"
            "\n dev.RtnAdr:%p,dev.uKnlId:%u",
            dwIoControlCode,
            dev.InAddr, dev.InSize,
            dev.OutAddr, dev.OutSize,
            dev.RtnAddr, dev.uKnlBId);

#endif

    if (ioctl(fd, dwIoControlCode, &dev) < 0) rn = FALSE;

    return rn;
}

LPVOID
ExAllocatePool(
    UINT uFlags,
    size_t dwBytes)
{
    return malloc(dwBytes);
}

LPVOID
ExFreePoolEx(
    void* obj,
    DWORD dwBytes)
{
    free(obj);
    return NULL;
}

VOID  ExFreePool(
    void* obj)
{
    return free(obj);

}


LPVOID GlobalAllocPtr(UINT flags, size_t cb)
{
	if (cb)
	{
		return GlobalAlloc(flags, cb);
	}
	else
	{
		return NULL;
	}
}

LPVOID GlobalFreePtr(LPVOID lp)
{
    return GlobalFree(lp);
}

#if 0
int wsprintf(char * buf, const char * fmt, ...)
{
    va_list args;
    int i;

    va_start(args, fmt);
    i = vsprintf(buf, fmt, args);
    va_end(args);
    return i;
}

#endif

void _strupr(char* buf)
{
    strupr(buf);
}

LPVOID LocalAlloc(UINT flags, DWORD cb)
{
    return GlobalAlloc(flags, cb);
}

LPVOID LocalFree(LPVOID lp)
{
    return GlobalFree(lp);
}

void Sleep(DWORD x)
{
    usleep(x*1000);
}

char* lstrcat(char *dest, const char *src)
{
    return strcat(dest , src);
}

size_t lstrlen(const char *s)
{
    return strlen(s);
}

char* lstrcpy(char *dest, const char *src)
{
    return strcpy(dest , src);
}

void DbgPrint(const char *fmt, ...)
{
    va_list args;
    int i;

    va_start(args, fmt);
    fprintf(stderr , fmt, args);
	fprintf(stderr , "\r\n");
    va_end(args);
}

BOOL RtlEqualMemory(const void* Destination, const void* Source, size_t Length)
{
    return !memcmp(Destination, Source, Length);
}

void *RtlMoveMemory(void *Destination, const void *Source, size_t Length)
{
    if (Destination == NULL ||  Source == NULL)
    {
        syslog(LOG_INFO | LOG_LOCAL6, "RtlMoveMemory(0x%08x,0x%08x,%u)", Destination, Source, Length);
    }
    else return memmove(Destination, Source, Length);
}

void *RtlCopyMemory(void *Destination, const void *Source, size_t Length)
{
    return memcpy(Destination, Source, Length);
}

void *RtlFillMemory(void *Destination, size_t Length, size_t Fill)
{
    return memset(Destination, Fill, Length);
}

void *RtlZeroMemory(void *Destination, size_t Length)
{
    return memset(Destination, 0, Length);
}

int DeleteFile(const char* pathname)
{
    int ret;
	ret = remove(pathname);
	return ret;
}

int DeleteFileW(LPCWSTR pathname)
{
    int ret;
	ret = remove(pathname);
	return ret;
}

DWORD GetTickCount()
{
    //return  times(NULL);
    struct timespec tv;
    //gettimeofday(&tv, NULL);
	clock_gettime(CLOCK_MONOTONIC,&tv);//added by yy for DS-44175,2016.02.24
	return ((tv.tv_sec * 1000) + (tv.tv_nsec / 1000000));
}

int  GetLocalTimer(char* p)
{

    struct tm* ptm;

    struct timeval valtime;
    gettimeofday(&valtime, NULL);
    ptm = localtime(&valtime.tv_sec);

    if (p)
    {
        return  sprintf(p, "%02d-%02d %02d:%02d:%02d:%03d",
                        ptm->tm_mon + 1,
                        ptm->tm_mday,
                        ptm->tm_hour,
                        ptm->tm_min,
                        ptm->tm_sec,
                        valtime.tv_usec / 1000);
    }

    return -1;
}

unsigned int GetAdaptersInfo(PIP_ADAPTER_INFO *IPinfo, int *size)
{

    register int fd, interface;
    unsigned int  retn = 0;

    struct ifreq buf[MAXINTERFACES];

    struct arpreq arp;

    struct ifconf ifc;
    PIP_ADAPTER_INFO ptr;
    *IPinfo = ptr = NULL;


    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) >= 0)
    {

        ifc.ifc_len = sizeof buf;
        ifc.ifc_buf = (caddr_t) buf;

        if (!ioctl(fd, SIOCGIFCONF, (char *) &ifc))
        {

            interface = ifc.ifc_len / sizeof(struct ifreq);
            //printf("interface num is interface=%d\n",interface);

            while (interface)
            {

                PIP_ADAPTER_INFO adapterinfo = (PIP_ADAPTER_INFO)malloc(sizeof(IP_ADAPTER_INFO) + 1);
                assert(adapterinfo != NULL);
                // if(adapterinfo == NULL);
                // {
                //  return 2;
                // }

                adapterinfo->Index = interface-1;
                adapterinfo->Next = NULL;//add by xzg 2010.9.29
                //printf ("net device %s\n", buf[interface].ifr_name);

                if (!(ioctl(fd, SIOCGIFFLAGS, (char *) &buf[interface-1])))
                {
                    if (buf[interface-1].ifr_flags & IFF_PROMISC)
                    {
                        //  puts ("the interface is PROMISC");
                        retn++;
                    }
                }
                else
                {
                    char str[256];
                    sprintf(str, "cpm: ioctl device %s", buf[interface-1].ifr_name);
                    // perror (str);
                }

                if (buf[interface-1].ifr_flags & IFF_UP)
                {
                    adapterinfo->Status = IFF_UP;
                    // puts("the interface status is UP");
                }
                else
                {
                    adapterinfo->Status = 0;
                    // puts("the interface status is DOWN");
                }

                if (!(ioctl(fd, SIOCGIFADDR, (char *) &buf[interface-1])))
                {
                    // puts ("IP address is:");
                    char *str = inet_ntoa(((struct sockaddr_in*)(&buf[interface-1].ifr_addr))->sin_addr);
                    sprintf(adapterinfo->IPStr, "%s", str);

                }

                if (*IPinfo == NULL)
                {
                    // printf("1111%s \n", adapterinfo->IPStr);
                    *IPinfo = adapterinfo;
                    ptr = adapterinfo;
                }
                else
                {
                    // printf("2222%s \n", adapterinfo->IPStr);
                    ptr->Next = adapterinfo;
                    ptr = adapterinfo;
                }

                interface--;
            }
        }
	close(fd);

    }

    return retn;
}

char * _itoa(int val, char *str, int mode)
{
    if (mode != 10)
    {
        fprintf(stderr, "_itoa's mode=%d\n", mode);
        sprintf(str, " ");
        return NULL;
    }

    sprintf(str, "%d", val);

    return str;
}

char * _ltoa(LONG val, char *str, int mode)
{
    if (mode != 10)
    {
        fprintf(stderr, "_ltoa's mode=%d\n", mode);
        sprintf(str, " ");
        return NULL;
    }

    sprintf(str, "%d", val);

    return str;
}

int _mkdir(char *dirname)
{
    int err ;

    struct stat buf;

	err = stat(dirname, &buf);
    if (!err)
        return 1;
    else if (errno == ENOENT)
    {
        err = mkdir(dirname, 0x700);
        return err;
    }
}

void ZeroMemory(PVOID DstStr, DWORD Len)
{
    memset(DstStr, 0, sizeof(char)*Len);
}

char *UpperString(char *src)
{
    int   i = 0;

    while (src[i]!='\0')
    {

	src[i] = toupper(src[i]);
        i++;
    }

    return src;
}

void* TlsGetValue(DWORD tls_idx)
{
	return NULL;
}

HANDLE CreateSemaphore(
  LPSECURITY_ATTRIBUTES lpSemaphoreAttributes, // SD
  LONG lInitialCount,                          // initial count
  LONG lMaximumCount,                          // maximum count
  LPCTSTR lpName                               // object name
)
{
	int   retCode ;
	int nLoopCnt=0;
	if (tmp_sem_id >= MAX_SEM)
	{
		fprintf(stderr, "\nReport ERROR @%s:%i\n", __FUNCTION__, __LINE__);
		tmp_sem_id = 0;
	}
	/*
	while(sem_creat[tmp_sem_id] != NULL )
	{
		tmp_sem_id++;
		if (tmp_sem_id >= MAX_SEM)
		{
			tmp_sem_id = 0;
		}
		nLoopCnt++;
		if(nLoopCnt >= MAX_SEM)
		{
			fprintf(stderr, "\nReport ERROR @%s:%i\n", __FUNCTION__, __LINE__);
			return NULL;
		}

	}
	*/
		

	// Initialize event semaphore
	retCode = sem_init(
			&sem_creat[tmp_sem_id],   // handle to the event semaphore
			0,     // not shared
			lInitialCount);    // initially set to non signaled state
	if(retCode != 0)
		return NULL;

	return (0x7fffffff - tmp_sem_id++);
	
}

BOOL ReleaseSemaphore(HANDLE hSemaphore, LONG lReleaseCount, LPLONG lpPreviousCount)
{
	// Condition met
	// now signal the event semaphore
	int retCode;
	int tmp;
	tmp = 0x7fffffff - hSemaphore;

	retCode = sem_post(
			&sem_creat[tmp]);    // Event semaphore Handle

	if (lpPreviousCount)
	{
		sem_getvalue(&sem_creat[tmp], lpPreviousCount);//get the value of a semaphore
	}
	

	return retCode;
}
