/*
 * libZRTP SDK library, implements the ZRTP secure VoIP protocol.
 * Copyright (c) 2006-2009 Philip R. Zimmermann.  All rights reserved.
 * Contact: http://philzimmermann.com
 * For licensing and other legal details, see the file zrtp_legal.c.
 * 
 * Viktor Krykun <v.krikun at zfoneproject.com> 
 */

#include "zrtp.h"

#if (defined(ZRTP_USE_BUILTIN) && (ZRTP_USE_BUILTIN == 1))

/*============================================================================*/
/*   Default realization of Mutexes synchronization routine					  */
/*============================================================================*/

/*---------------------------------------------------------------------------*/
#if   (ZRTP_PLATFORM == ZP_WIN32_KERNEL)
#include <Ndis.h>

struct zrtp_mutex_t
{
	NDIS_SPIN_LOCK	mutex;
};

zrtp_status_t zrtp_mutex_init(zrtp_mutex_t **mutex)
{
	zrtp_mutex_t* new_mutex = zrtp_sys_alloc(sizeof(zrtp_mutex_t));
	if (!new_mutex)
		return zrtp_status_alloc_fail;
	NdisAllocateSpinLock(&new_mutex->mutex);
	*mutex = new_mutex;
	return zrtp_status_ok;
}

zrtp_status_t zrtp_mutex_destroy(zrtp_mutex_t* mutex)
{
	NdisFreeSpinLock(&mutex->mutex);
	zrtp_sys_free(mutex);
	return zrtp_status_ok;
}

zrtp_status_t zrtp_mutex_lock(zrtp_mutex_t* mutex)
{
	NdisAcquireSpinLock(&mutex->mutex);
	return zrtp_status_ok;
}

zrtp_status_t zrtp_mutex_unlock(zrtp_mutex_t* mutex)
{
	NdisReleaseSpinLock(&mutex->mutex);
	return zrtp_status_ok;
}

/*---------------------------------------------------------------------------*/
#elif (ZRTP_PLATFORM == ZP_WIN32) || (ZRTP_PLATFORM == ZP_WIN64) || (ZRTP_PLATFORM == ZP_WINCE)

#include <Windows.h>

struct zrtp_mutex_t
{
	HANDLE	mutex;
};

zrtp_status_t zrtp_mutex_init(zrtp_mutex_t** mutex)
{
	zrtp_mutex_t* new_mutex = zrtp_sys_alloc(sizeof(zrtp_mutex_t));
	if (!new_mutex)
		return zrtp_status_alloc_fail;
	new_mutex->mutex = CreateMutex(NULL, FALSE, NULL);
	if (!new_mutex->mutex) {
		zrtp_sys_free(new_mutex);
		return zrtp_status_fail;
	}
	*mutex = new_mutex;
	return zrtp_status_ok;
}

zrtp_status_t zrtp_mutex_destroy(zrtp_mutex_t* mutex)
{
	zrtp_status_t s = (0 == CloseHandle(mutex->mutex)) ? zrtp_status_fail : zrtp_status_ok;
    zrtp_sys_free(mutex);
	return s;
}

zrtp_status_t zrtp_mutex_lock(zrtp_mutex_t* mutex)
{
    return (WaitForSingleObject(mutex->mutex, INFINITE) == WAIT_FAILED) ? zrtp_status_fail : zrtp_status_ok;
}

zrtp_status_t zrtp_mutex_unlock(zrtp_mutex_t* mutex)
{
    return (0 == ReleaseMutex(mutex->mutex)) ? zrtp_status_fail : zrtp_status_ok;
}

/*---------------------------------------------------------------------------*/
#elif (ZRTP_PLATFORM == ZP_LINUX) || (ZRTP_PLATFORM == ZP_DARWIN) || (ZRTP_PLATFORM == ZP_BSD) || (ZRTP_PLATFORM == ZP_ANDROID)

#if defined ZRTP_HAVE_PTHREAD_H
#	include <pthread.h>
#endif

struct zrtp_mutex_t
{
	pthread_mutex_t mutex;
};


zrtp_status_t zrtp_mutex_init(zrtp_mutex_t** mutex)
{
	zrtp_mutex_t* new_mutex = zrtp_sys_alloc(sizeof(zrtp_mutex_t));
	if (new_mutex) {
		zrtp_status_t s = pthread_mutex_init(&new_mutex->mutex, NULL) == 0 ? zrtp_status_ok : zrtp_status_fail;
		if (s == zrtp_status_fail)
			zrtp_sys_free(new_mutex);
		else
			*mutex = new_mutex;
		return s;
	} 
	return zrtp_status_alloc_fail;
}

zrtp_status_t zrtp_mutex_destroy(zrtp_mutex_t* mutex)
{
	zrtp_status_t s = (pthread_mutex_destroy(&mutex->mutex) == 0) ? zrtp_status_ok : zrtp_status_fail;
    zrtp_sys_free(mutex);
	return s;
}

zrtp_status_t zrtp_mutex_lock(zrtp_mutex_t* mutex)
{
    return (pthread_mutex_lock(&mutex->mutex) == 0) ? zrtp_status_ok : zrtp_status_fail;
}

zrtp_status_t zrtp_mutex_unlock(zrtp_mutex_t* mutex)
{
    return (pthread_mutex_unlock(&mutex->mutex) == 0) ? zrtp_status_ok : zrtp_status_fail;
}

#endif


/*============================================================================*/
/*   Default realization of Semaphores synchronization routine			      */
/*============================================================================*/

#if   (ZRTP_PLATFORM == ZP_WIN32_KERNEL)

struct zrtp_sem_t
{
	KSEMAPHORE sem;
};

zrtp_status_t zrtp_sem_init(zrtp_sem_t** sem, uint32_t val, uint32_t limit)
{
	zrtp_sem_t *new_sem =  zrtp_sys_alloc(sizeof(zrtp_sem_t));
	if (NULL == new_sem) {
		return zrtp_status_alloc_fail;
	}

	KeInitializeSemaphore(&new_sem->sem, val, limit); 
	*sem = new_sem;
	return zrtp_status_ok;
}

zrtp_status_t zrtp_sem_destroy(zrtp_sem_t* sem) 
{
	return zrtp_status_ok;
}

zrtp_status_t zrtp_sem_wait(zrtp_sem_t* sem)
{
	return KeWaitForSingleObject(&sem->sem, Executive, KernelMode, FALSE, NULL) == STATUS_SUCCESS ? 
		zrtp_status_ok : zrtp_status_fail;
}

zrtp_status_t zrtp_sem_trtwait(zrtp_sem_t* sem)
{
	LARGE_INTEGER timeout;
	timeout.QuadPart = 0;

	return KeWaitForSingleObject(&sem->sem, Executive, KernelMode, FALSE, &timeout) == STATUS_SUCCESS ? 
		zrtp_status_ok : zrtp_status_fail;
}

zrtp_status_t zrtp_sem_post(zrtp_sem_t* sem)
{
	KeReleaseSemaphore(&sem->sem, IO_NO_INCREMENT, 1, FALSE);
	return zrtp_status_ok;
} 


#elif (ZRTP_PLATFORM == ZP_WIN32) || (ZRTP_PLATFORM == ZP_WIN64) || (ZRTP_PLATFORM == ZP_WINCE)

struct zrtp_sem_t
{
	HANDLE sem;
};

zrtp_status_t zrtp_sem_init(zrtp_sem_t** sem, uint32_t val, uint32_t limit)
{
	zrtp_sem_t *new_sem =  zrtp_sys_alloc(sizeof(zrtp_sem_t));
	if (NULL == new_sem) {
		return zrtp_status_alloc_fail;
	}

	new_sem->sem = CreateSemaphore(NULL, val, limit, NULL); 
	if (!new_sem->sem) {
		zrtp_sys_free(new_sem);
		return zrtp_status_fail;
	}
	*sem = new_sem;
	return zrtp_status_ok;
}

zrtp_status_t zrtp_sem_destroy(zrtp_sem_t* sem)
{
	zrtp_status_t s = (0 == CloseHandle(sem->sem)) ? zrtp_status_fail : zrtp_status_ok;
	zrtp_sys_free(sem);
	return s;
}

zrtp_status_t zrtp_sem_wait(zrtp_sem_t* sem)
{
	return (WaitForSingleObject(sem->sem, INFINITE) == WAIT_FAILED) ? zrtp_status_fail : zrtp_status_ok;
}

zrtp_status_t zrtp_sem_trtwait(zrtp_sem_t* sem)
{
	return (WaitForSingleObject(sem->sem, 0) == WAIT_OBJECT_0) ? zrtp_status_ok : zrtp_status_fail;
}

zrtp_status_t zrtp_sem_post(zrtp_sem_t* sem)
{
	return (0 == ReleaseSemaphore(sem->sem, 1, NULL)) ? zrtp_status_fail : zrtp_status_ok;
} 

#elif (ZRTP_PLATFORM == ZP_LINUX) || (ZRTP_PLATFORM == ZP_DARWIN) || (ZRTP_PLATFORM == ZP_BSD) || (ZRTP_PLATFORM == ZP_ANDROID)

#if defined ZRTP_HAVE_STDIO_H
#	include <stdio.h>
#endif
#if ZRTP_HAVE_SEMAPHORE_H
#	include <semaphore.h>
#endif
#if ZRTP_HAVE_FCNTL_H
#	include <fcntl.h>
#endif
#if ZRTP_HAVE_ERRNO_H
#	include <errno.h>
#endif


#if (ZRTP_PLATFORM == ZP_DARWIN)

struct zrtp_sem_t
{
	sem_t* sem;
};

zrtp_status_t zrtp_sem_init(zrtp_sem_t** sem, uint32_t value, uint32_t limit)
{
	zrtp_status_t s = zrtp_status_ok;
	char name_buff[48];
	zrtp_time_t now = zrtp_time_now();
	
	zrtp_sem_t *new_sem = (zrtp_sem_t*)zrtp_sys_alloc(sizeof(zrtp_sem_t));
	if (0 == new_sem) {
		return zrtp_status_alloc_fail;
	}
	
	/*
     * This bogusness is to follow what appears to be the lowest common
	 * denominator in Posix semaphore naming:
     *   - start with '/'
     *   - be at most 15 chars
     *   - be unique and not match anything on the filesystem
     * We suppose to generate unique name for every semaphore in the system.
     */
	
    sprintf(name_buff, "/libzrtp.%llxZ%llx", now/1000, now);
    new_sem->sem = sem_open(name_buff, O_CREAT | O_EXCL, S_IRUSR|S_IWUSR, value);
    if ((sem_t *)SEM_FAILED == new_sem->sem) {
        if (errno == ENAMETOOLONG) {
            name_buff[13] = '\0';
        } else if (errno == EEXIST) {
            sprintf(name_buff, "/libzrtp.%llxZ%llx", now, now/1000);
        } else {
			s = zrtp_status_fail;
        }
        new_sem->sem = sem_open(name_buff, O_CREAT | O_EXCL, 0644, value);
    }
	
    if (new_sem->sem == (sem_t *)SEM_FAILED) {
		s = zrtp_status_fail;
		zrtp_sys_free(new_sem);
    } else {
		sem_unlink(name_buff);
		*sem = new_sem;
	}
	
	return s;
}

zrtp_status_t zrtp_sem_destroy(zrtp_sem_t* sem)
{
	zrtp_status_t s = sem_close(sem->sem);
	zrtp_sys_free(sem);
	if (0 != s) {
		s = zrtp_status_fail;
	}
	
	return s;	
}

zrtp_status_t zrtp_sem_wait(zrtp_sem_t* sem)
{
    return (sem_wait(sem->sem) == 0) ? zrtp_status_ok : zrtp_status_fail;
}

zrtp_status_t zrtp_sem_trtwait(zrtp_sem_t* sem)
{
    return (sem_trywait(sem->sem) == 0) ? zrtp_status_ok : zrtp_status_fail;
}

zrtp_status_t zrtp_sem_post(zrtp_sem_t* sem)
{
	return (sem_post(sem->sem) == 0) ? zrtp_status_ok : zrtp_status_fail;
}

#else

struct zrtp_sem_t
{
	sem_t sem;
};


zrtp_status_t zrtp_sem_init(zrtp_sem_t** sem, uint32_t value, uint32_t limit)
{
	zrtp_sem_t *new_sem = (zrtp_sem_t*)zrtp_sys_alloc(sizeof(zrtp_sem_t));
	if (NULL == new_sem) {
		return zrtp_status_alloc_fail;
	}
	
	if (sem_init(&new_sem->sem, 0, value) != 0) {
		zrtp_sys_free(new_sem);
		return zrtp_status_fail;
	}
	
	*sem = new_sem;
	return zrtp_status_ok;
}

zrtp_status_t zrtp_sem_destroy(zrtp_sem_t* sem)
{
	zrtp_status_t s = sem_destroy(&sem->sem) == 0 ? zrtp_status_ok : zrtp_status_fail;
	zrtp_sys_free(sem);
	return s;
}

zrtp_status_t zrtp_sem_wait(zrtp_sem_t* sem)
{
    return (sem_wait(&sem->sem) == 0) ? zrtp_status_ok : zrtp_status_fail;
}

zrtp_status_t zrtp_sem_trtwait(zrtp_sem_t* sem)
{
    return (sem_trywait(&sem->sem) == 0) ? zrtp_status_ok : zrtp_status_fail;
}

zrtp_status_t zrtp_sem_post(zrtp_sem_t* sem)
{
	return (sem_post(&sem->sem) == 0) ? zrtp_status_ok : zrtp_status_fail;
}


#endif


#endif


/*============================================================================*/
/*   Default realization of general routine									  */
/*============================================================================*/

#if defined ZRTP_HAVE_STRING_H
#	include <string.h> /* for memset() and memcpy() */
#endif

/*----------------------------------------------------------------------------*/
#if   (ZRTP_PLATFORM == ZP_WIN32_KERNEL)

void* zrtp_sys_alloc(unsigned int size)
{
    void *VA;
    return (NDIS_STATUS_SUCCESS != NdisAllocateMemoryWithTag(&VA, size, (ULONG)"zrtp")) ? NULL : VA;
}

void zrtp_sys_free(void* obj)
{
    /* Length is 0 because memory was allocated with TAG */
    NdisFreeMemory(obj, 0, 0);
}

void* zrtp_memcpy(void* dest, const void* src, unsigned int length)
{
	return memcpy(dest,src,length);
}

void *zrtp_memset(void *s, int c, unsigned int n)
{
    return memset(s, c, n);
}

zrtp_time_t zrtp_time_now()
{
	LARGE_INTEGER ft;
	KeQuerySystemTime(&ft);
	
	ft.QuadPart -= 116444736000000000;
	return (zrtp_time_t)(ft.QuadPart) / 10000;
}
#else

/*---------------------------------------------------------------------------*/
#if (ZRTP_PLATFORM == ZP_WIN32) || (ZRTP_PLATFORM == ZP_WIN64) || (ZRTP_PLATFORM == ZP_WINCE)

zrtp_time_t zrtp_time_now()
{
    LONGLONG ft;

#if ZRTP_PLATFORM != ZP_WINCE
	GetSystemTimeAsFileTime((LPFILETIME)&ft);
#else
	SYSTEMTIME SystemTime;
	GetSystemTime(&SystemTime);
	SystemTimeToFileTime(&SystemTime, (LPFILETIME)&ft);
#endif
    
    ft -= 116444736000000000;
	return (zrtp_time_t)(ft) / 10000;
}

/*---------------------------------------------------------------------------*/
#elif (ZRTP_PLATFORM == ZP_LINUX) || (ZRTP_PLATFORM == ZP_DARWIN) || (ZRTP_PLATFORM == ZP_SYMBIAN) || (ZRTP_PLATFORM == ZP_BSD) || (ZRTP_PLATFORM == ZP_ANDROID)

#if defined ZRTP_HAVE_SYS_TIME_H
#	include <sys/time.h>
#endif

zrtp_time_t zrtp_time_now()
{
    struct timeval tv;
    if (0 == gettimeofday(&tv, 0)) {
		return (zrtp_time_t)(tv.tv_sec)*1000 + (zrtp_time_t)(tv.tv_usec)/1000;
    }
	return 0;
}
#endif


void *zrtp_memset(void *s, int c, unsigned int n)
{
    memset(s, c, n);
    return s;
}

void* zrtp_memcpy(void* dest, const void* src, unsigned int length)
{
    memcpy(dest, src, (size_t)length);
    return dest;
}

void* zrtp_sys_alloc(unsigned int size)
{
    return malloc((size_t)size);
}

void zrtp_sys_free(void* obj)
{
    free(obj);
}

#endif /* default platform-dependent components realizations */

#endif /*ZRTP_USE_BUILTIN*/
