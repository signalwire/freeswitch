/*
 * libZRTP SDK library, implements the ZRTP secure VoIP protocol.
 * Copyright (c) 2006-2009 Philip R. Zimmermann.  All rights reserved.
 * Contact: http://philzimmermann.com
 * For licensing and other legal details, see the file zrtp_legal.c.
 * 
 * Viktor Krykun <v.krikun at zfoneproject.com> 
 */


#ifndef __ZRTP_IFACE_SCHEDULER_H__ 
#define __ZRTP_IFACE_SCHEDULER_H__

#include "zrtp_config.h"
#include "zrtp_base.h"
#include "zrtp_string.h"
#include "zrtp_error.h"
#include "zrtp_iface.h"

#if defined(ZRTP_USE_BUILTIN_SCEHDULER) && (ZRTP_USE_BUILTIN_SCEHDULER == 1)

#if defined(__cplusplus)
extern "C"
{
#endif	


/**
 * In order to use default secheduler libzrtp one should define tow extra interfaces:
 * sleep and threads riutine. 
 */

/**
 * \brief Suspend thread execution for an interval measured in miliseconds
 * \param msec - number of miliseconds
 * \return: 0 if successful and -1 in case of errors.
 */
	
#if ZRTP_PLATFORM != ZP_WIN32_KERNEL
	
#if (ZRTP_PLATFORM == ZP_WIN32) || (ZRTP_PLATFORM == ZP_WINCE)
#include <windows.h>
	typedef LPTHREAD_START_ROUTINE zrtp_thread_routine_t;
#elif (ZRTP_PLATFORM == ZP_LINUX) || (ZRTP_PLATFORM == ZP_DARWIN) || (ZRTP_PLATFORM == ZP_BSD) || (ZRTP_PLATFORM == ZP_ANDROID)
	typedef	void *(*zrtp_thread_routine_t)(void*);
#elif (ZRTP_PLATFORM == ZP_SYMBIAN)
	typedef int(*zrtp_thread_routine_t)(void*);
#endif
	
/**
 * \brief Function is used to create a new thread, within a process.
 *
 * Thread should be created with default attributes. Upon its creation, the thread executes
 * \c start_routine, with \c arg as its sole argument.
 * \param start_routine - thread start routine.
 * \param arg - start routine arguments.
 * \return 0 if successful and -1 in case of errors.
 */


extern int zrtp_thread_create(zrtp_thread_routine_t start_routine, void *arg);
extern int zrtp_sleep(unsigned int msec);
	
#endif	
	
void zrtp_def_scheduler_down();

zrtp_status_t zrtp_def_scheduler_init(zrtp_global_t* zrtp);

void zrtp_def_scheduler_call_later(zrtp_stream_t *ctx, zrtp_retry_task_t* ztask);

void zrtp_def_scheduler_cancel_call_later(zrtp_stream_t* ctx, zrtp_retry_task_t* ztask);

void zrtp_def_scheduler_wait_call_later(zrtp_stream_t* ctx);


zrtp_status_t zrtp_sem_init(zrtp_sem_t** sem, uint32_t value, uint32_t limit);
zrtp_status_t zrtp_sem_destroy(zrtp_sem_t* sem);
zrtp_status_t zrtp_sem_wait(zrtp_sem_t* sem);
zrtp_status_t zrtp_sem_trtwait(zrtp_sem_t* sem);
zrtp_status_t zrtp_sem_post(zrtp_sem_t* sem);
	
#if defined(__cplusplus)
}
#endif

#endif /* ZRTP_USE_BUILTIN_SCEHDULER */

#endif /*__ZRTP_IFACE_SCHEDULER_H__*/
