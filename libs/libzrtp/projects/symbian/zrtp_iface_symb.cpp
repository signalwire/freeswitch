/*
 * Copyright (c) 2006-2009 Philip R. Zimmermann. All rights reserved.
 * Contact: http://philzimmermann.com
 * For licensing and other legal details, see the file zrtp_legal.c.
 */

#include <charconv.h>
#include <stdarg.h>
#include <sys/time.h>

#include <e32msgqueue.h>

#include <UNISTD.H>
#include <e32base.h>
#include <e32math.h>

#include <zrtp.h>

extern "C"
{
/**
 * @brief Get kernel-generated random number
 * @bug		seems not work
 * @return 32 random bits
 */
uint32_t zrtp_symbian_kernel_random();

/**
 * @brief Pseudo random number: sum of pid's shifted and xored by number of precceses
 * @return
 */
uint32_t zrtp_sum_of_pid_and_number_of_poccesses();

/**
 * @brief Number of milisecond past from particular date and time
 * @return
 */
uint64_t zrtp_get_system_time_crazy();

/**
 * @brief Current procces PID
 * @return PID
 */
unsigned int zrtp_get_pid();

/**
 * @brief Availible memory
 * @return memory availible on heap
 */
uint32_t zrtp_get_availible_heap();

}



//-----------------------------------------------------------------------------
zrtp_status_t zrtp_mutex_init(zrtp_mutex_t **mutex) {
	RMutex *rmutex = new RMutex();
	//rmutex->CreateLocal(); was before
	rmutex->CreateGlobal(KNullDesC);
	*mutex = (zrtp_mutex_t*) rmutex;
	return zrtp_status_ok;
}

zrtp_status_t zrtp_mutex_lock(zrtp_mutex_t* mutex) {
	RMutex *rmutex = (RMutex *) mutex;
	rmutex->Wait();
	return zrtp_status_ok;
}

zrtp_status_t zrtp_mutex_unlock(zrtp_mutex_t* mutex) {
	RMutex *rmutex = (RMutex *) mutex;
	rmutex->Signal();
	return zrtp_status_ok;
}

zrtp_status_t zrtp_mutex_destroy(zrtp_mutex_t* mutex) {
	RMutex *rmutex = (RMutex *) mutex;
	if (rmutex) {
		rmutex->Close();
		delete rmutex;
	}
	return zrtp_status_ok;
}

//-----------------------------------------------------------------------------
zrtp_status_t zrtp_sem_init(zrtp_sem_t** sem, uint32_t value, uint32_t limit) {
	RSemaphore *rsem = new RSemaphore();
	//rsem->CreateLocal(value);
	rsem->CreateGlobal(KNullDesC,value);
	*sem = (zrtp_sem_t*) rsem;
	return zrtp_status_ok;
}

zrtp_status_t zrtp_sem_destroy(zrtp_sem_t* sem) {
	RSemaphore *rsem = (RSemaphore *) sem;
	if (rsem) {
		rsem->Close();
		delete rsem;
	}
	return zrtp_status_ok;
}

zrtp_status_t zrtp_sem_wait(zrtp_sem_t* sem) {
	RSemaphore *rsem = (RSemaphore *) sem;
	rsem->Wait();
	return zrtp_status_ok;
}

zrtp_status_t zrtp_sem_trtwait(zrtp_sem_t* sem) {
	RSemaphore *rsem = (RSemaphore *) sem;
	rsem->Wait(1000);
	return zrtp_status_ok;
}

zrtp_status_t zrtp_sem_post(zrtp_sem_t* sem) {
	RSemaphore *rsem = (RSemaphore *) sem;
	rsem->Signal();
	return zrtp_status_ok;
}

//-----------------------------------------------------------------------------
int zrtp_sleep(unsigned int msec) {
	TTimeIntervalMicroSeconds32 i(msec *1000);
	User::After(i);
	return 0;
}

int zrtp_thread_create(zrtp_thread_routine_t start_routine, void *arg) {
	RThread h;
	TBuf<64> thName=_L("zrtp_thread");

	h.Create(thName, start_routine, KDefaultStackSize*2, NULL, arg) ;
	h.Resume();
	h.Close();

   return NULL;
}
//-----------------------------------------------------------------------------
//			For Scheduler
#if (defined(ZRTP_USE_BUILTIN_SCEHDULER) && (ZRTP_USE_BUILTIN_SCEHDULER ==1))

#include "DelayRuner.h"
#include "zrtp_error.h"
mlist_t		tasks_head_s;
static uint8_t	inited = 0 ;
static uint8_t	is_running = 0;

typedef struct {
	zrtp_stream_t   *ctx;		/** ZRTP stream context associated with the task */
	zrtp_retry_task_t	*ztask;		/** ZRTP stream associated with the task */
	mlist_t				_mlist;
	CDelayRuner*			ao;		// Active object
} zrtp_sched_task_s_t;

zrtp_status_t zrtp_def_scheduler_init(zrtp_global_t* zrtp)
{
	zrtp_status_t status = zrtp_status_ok;
	ZRTP_LOG(3,("symbian","Init start"));
	if (inited) {
		return zrtp_status_ok;
	}

	do {
		init_mlist(&tasks_head_s);
		is_running = 1;
		inited  = 1;
	} while (0);

	ZRTP_LOG(3,("symbian","Init end"));
	return status;
}

void zrtp_def_scheduler_down()
{
	ZRTP_LOG(3,("symbian","Down start"));
	mlist_t *node = 0, *tmp = 0;

	if (!inited) {
		return;
	}

	/* Stop main thread */
	is_running = 0;
//	zrtp_sem_post(count);

	/* Then destroy tasks queue and realease all other resources */
	//zrtp_mutex_lock(protector);

	mlist_for_each_safe(node, tmp, &tasks_head_s) {
		zrtp_sched_task_s_t* task = mlist_get_struct(zrtp_sched_task_s_t, _mlist, node);
		if (task->ao!=NULL)
			{
			delete task->ao;
			}
		zrtp_sys_free(task);
	}
	init_mlist(&tasks_head_s);

//	zrtp_mutex_unlock(protector);

//	zrtp_mutex_destroy(protector);
//	zrtp_sem_destroy(count);

	ZRTP_LOG(3,("symbian","Down end"));
	inited  = 0;
}


void zrtp_def_scheduler_call_later(zrtp_stream_t *ctx, zrtp_retry_task_t* ztask)
{
//	ZRTP_LOG(3,("symbian","CallLater start"));
	//mlist_t *node=0, *tmp=0;
	mlist_t* last = &tasks_head_s;

	//zrtp_mutex_lock(protector);

	if (!ztask->_is_enabled) {
		//zrtp_mutex_unlock(protector);
		return;
	}

	do {
		zrtp_sched_task_s_t* new_task = (zrtp_sched_task_s_t*)zrtp_sys_alloc(sizeof(zrtp_sched_task_s_t));
		if (!new_task) {
			break;
		}

		new_task->ctx			= ctx;
		new_task->ztask			= ztask;
		new_task->ao			= CDelayRuner::NewL();

		mlist_insert(last, &new_task->_mlist);

		new_task->ao->StartL(ctx,ztask);
		//zrtp_sem_post(count);
	} while (0);

	//ZRTP_LOG(3,("symbian","CallLater end"));
	//zrtp_mutex_unlock(protector);
}

void zrtp_def_scheduler_cancel_call_later(zrtp_stream_t* ctx, zrtp_retry_task_t* ztask)
{
	mlist_t *node=0, *tmp=0;
	ZRTP_LOG(3,("symbian","CancelcallLater start"));
//	zrtp_mutex_lock(protector);

	mlist_for_each_safe(node, tmp, &tasks_head_s) {
		zrtp_sched_task_s_t* task = mlist_get_struct(zrtp_sched_task_s_t, _mlist, node);
		if ((task->ctx == ctx) && ((task->ztask == ztask) || !ztask)) {
			task->ao->Cancel();
			delete task->ao; // Cancel and delete task
			mlist_del(&task->_mlist);
			zrtp_sys_free(task);
			//zrtp_sem_trtwait(count);
			if (ztask) {
				break;
			}
		}
	}
	ZRTP_LOG(3,("symbian","CancelCallLater done"));
//	zrtp_mutex_unlock(protector);
}

void zrtp_internal_delete_task_from_list(zrtp_stream_t* ctx, zrtp_retry_task_t* ztask)
	{
	mlist_t *node=0, *tmp=0;
	ZRTP_LOG(3,("symbian","DelTask begin"));
	mlist_for_each_safe(node, tmp, &tasks_head_s)
		{
		zrtp_sched_task_s_t* task = mlist_get_struct(zrtp_sched_task_s_t, _mlist, node);
		if ((task->ctx == ctx) && ((task->ztask == ztask) || !ztask))
			{
			delete task->ao; // Cancel and delete task
			mlist_del(&task->_mlist);
			zrtp_sys_free(task);
			ZRTP_LOG(3,("symbian","DelTask Del"));
			//zrtp_sem_trtwait(count);
			if (ztask)
				{
				break;
				}
			}
		}
	ZRTP_LOG(3,("symbian","DelTask end"));
	}

void zrtp_def_scheduler_wait_call_later(zrtp_stream_t* ctx)
{
}
#endif // ZRTP_USE_BUILTIN_SCEHDULER
//-----------------------------------------------------------------------------

unsigned int zrtp_get_pid()
	{
	return getpid();
	}

uint64_t zrtp_get_system_time_crazy()
	{
	TTime time;

	return time.MicroSecondsFrom(TTime(TDateTime (491,EAugust,7,3,37,17,347))).Int64();
	}

uint32_t zrtp_sum_of_pid_and_number_of_poccesses()
	{
	TFindProcess fp;
	RProcess procces;
	TFullName proccesName;
	uint_32t idsum=1;
	uint_32t proccesCount=0;
	fp.Find(KNullDesC);
	while (fp.Next(proccesName)==KErrNone)
		{
		 if (procces.Open(proccesName,EOwnerProcess)==KErrNone)
			 {
			 idsum+=procces.Id();
			 proccesCount++;
			 procces.Close();
			 }
		}
	idsum = (idsum << 3) xor proccesCount;
	return idsum;
	}

uint32_t zrtp_get_availible_heap()
	{
	return User::Heap().MaxLength();
	}

uint32_t zrtp_symbian_kernel_random()
	{
	return Math::Random();
	}
