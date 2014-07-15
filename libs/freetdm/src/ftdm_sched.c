/*
 * Copyright (c) 2010, Sangoma Technologies
 * Moises Silva <moy@sangoma.com>
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 
 * * Neither the name of the original author; nor the names of any contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 * 
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "private/ftdm_core.h"

#ifdef __WINDOWS__
struct ftdm_timezone {
    int tz_minuteswest;         /* minutes W of Greenwich */
    int tz_dsttime;             /* type of dst correction */
};
int gettimeofday(struct timeval *tv, struct ftdm_timezone *tz)
{
    FILETIME ft;
    unsigned __int64 tmpres = 0;
    static int tzflag;
    if (NULL != tv) {
        GetSystemTimeAsFileTime(&ft);
        tmpres |= ft.dwHighDateTime;
        tmpres <<= 32;
        tmpres |= ft.dwLowDateTime;

        /*converting file time to unix epoch */
        tmpres /= 10;           /*convert into microseconds */
        tmpres -= DELTA_EPOCH_IN_MICROSECS;
        tv->tv_sec = (long) (tmpres / 1000000UL);
        tv->tv_usec = (long) (tmpres % 1000000UL);
    }
    if (NULL != tz) {
        if (!tzflag) {
            _tzset();
            tzflag++;
        }
        tz->tz_minuteswest = _timezone / 60;
        tz->tz_dsttime = _daylight;
    }
    return 0;
}
#endif /* __WINDOWS__ */

typedef struct ftdm_timer ftdm_timer_t;

static struct {
	ftdm_sched_t *freeruns;
	ftdm_mutex_t *mutex;
	ftdm_bool_t running;
} sched_globals;

struct ftdm_sched {
	char name[80];
	ftdm_timer_id_t currid;
	ftdm_mutex_t *mutex;
	ftdm_timer_t *timers;
	int freerun;
	ftdm_sched_t *next;
	ftdm_sched_t *prev;
};

struct ftdm_timer {
	char name[80];
	ftdm_timer_id_t id;
	struct timeval time;
	void *usrdata;
	ftdm_sched_callback_t callback;
	ftdm_timer_t *next;
	ftdm_timer_t *prev;
};

/* FIXME: use ftdm_interrupt_t to wait for new schedules to monitor */
#define SCHED_MAX_SLEEP 100
static void *run_main_schedule(ftdm_thread_t *thread, void *data)
{
	int32_t timeto;
	int32_t sleepms;
	ftdm_status_t status;
	ftdm_sched_t *current = NULL;

	ftdm_unused_arg(data);
	ftdm_unused_arg(thread);

	while (ftdm_running()) {
		
		sleepms = SCHED_MAX_SLEEP;

		ftdm_mutex_lock(sched_globals.mutex);

		if (!sched_globals.freeruns) {
		
			/* there are no free runs, wait a bit and check again (FIXME: use ftdm_interrupt_t for this) */
			ftdm_mutex_unlock(sched_globals.mutex);

			if (ftdm_running()) {
				ftdm_sleep(sleepms);
			}
		}

		for (current = sched_globals.freeruns; current; current = current->next) {
			if (!ftdm_running()) {
				break;
			}

			/* first run the schedule */
			ftdm_sched_run(current);

			/* now find out how much time to sleep until running them again */
			status = ftdm_sched_get_time_to_next_timer(current, &timeto);
			if (status != FTDM_SUCCESS) {
				ftdm_log(FTDM_LOG_WARNING, "Failed to get time to next timer for schedule %s, skipping\n", current->name);	
				continue;
			}

			/* if timeto == -1 we don't want to sleep forever, so keep the last sleepms */
			if (timeto != -1 && sleepms > timeto) {
				sleepms = timeto;
			}
		}

		ftdm_mutex_unlock(sched_globals.mutex);

		if (ftdm_running()) {
			ftdm_sleep(sleepms);
		}
	}
	ftdm_log(FTDM_LOG_NOTICE, "Main scheduling thread going out ...\n");
	sched_globals.running = 0;
	return NULL;
}

FT_DECLARE(ftdm_status_t) ftdm_sched_global_init()
{
	ftdm_log(FTDM_LOG_DEBUG, "Initializing scheduling API\n");
	memset(&sched_globals, 0, sizeof(sched_globals));
	if (ftdm_mutex_create(&sched_globals.mutex) == FTDM_SUCCESS) {
		return FTDM_SUCCESS;
	}
	return FTDM_FAIL;
}

FT_DECLARE(ftdm_status_t) ftdm_sched_global_destroy()
{
	ftdm_mutex_destroy(&sched_globals.mutex);
	memset(&sched_globals, 0, sizeof(sched_globals));
	return FTDM_SUCCESS;
}

FT_DECLARE(ftdm_status_t) ftdm_sched_free_run(ftdm_sched_t *sched)
{
	ftdm_status_t status = FTDM_FAIL;
	ftdm_assert_return(sched != NULL, FTDM_EINVAL, "invalid pointer\n");

	ftdm_mutex_lock(sched->mutex);

	ftdm_mutex_lock(sched_globals.mutex);

	if (sched->freerun) {
		ftdm_log(FTDM_LOG_ERROR, "Schedule %s is already running in free run\n", sched->name);
		goto done;
	}
	sched->freerun = 1;

	if (sched_globals.running == FTDM_FALSE) {
		ftdm_log(FTDM_LOG_NOTICE, "Launching main schedule thread\n");
		status = ftdm_thread_create_detached(run_main_schedule, NULL);
		if (status != FTDM_SUCCESS) {
			ftdm_log(FTDM_LOG_CRIT, "Failed to launch main schedule thread\n");
			goto done;
		} 
		sched_globals.running = FTDM_TRUE;
	}

	ftdm_log(FTDM_LOG_DEBUG, "Running schedule %s in the main schedule thread\n", sched->name);
	status = FTDM_SUCCESS;
	
	/* Add the schedule to the global list of free runs */
	if (!sched_globals.freeruns) {
		sched_globals.freeruns = sched;
	}  else {
		sched->next = sched_globals.freeruns;
		sched_globals.freeruns->prev = sched;
		sched_globals.freeruns = sched;
	}

done:
	ftdm_mutex_unlock(sched_globals.mutex);

	ftdm_mutex_unlock(sched->mutex);
	return status;
}

FT_DECLARE(ftdm_bool_t) ftdm_free_sched_running(void)
{
	return sched_globals.running;
}

FT_DECLARE(ftdm_bool_t) ftdm_free_sched_stop(void)
{
	/* currently we really dont stop the thread here, we rely on freetdm being shutdown and ftdm_running() to be false 
	 * so the scheduling thread dies and we just wait for it here */
	uint32_t sanity = 100;
	while (ftdm_free_sched_running() && --sanity) {
		ftdm_log(FTDM_LOG_DEBUG, "Waiting for main schedule thread to finish\n");
		ftdm_sleep(100);
	}

	if (!sanity) {
		ftdm_log(FTDM_LOG_CRIT, "schedule thread did not stop running, we may crash on shutdown\n");
		return FTDM_FALSE;
	}

	return FTDM_TRUE;
}

FT_DECLARE(ftdm_status_t) ftdm_sched_create(ftdm_sched_t **sched, const char *name)
{
	ftdm_sched_t *newsched = NULL;

	ftdm_assert_return(sched != NULL, FTDM_EINVAL, "invalid pointer\n");
	ftdm_assert_return(name != NULL, FTDM_EINVAL, "invalid sched name\n");

	*sched = NULL;

	newsched = ftdm_calloc(1, sizeof(*newsched));
	if (!newsched) {
		return FTDM_MEMERR;
	}

	if (ftdm_mutex_create(&newsched->mutex) != FTDM_SUCCESS) {
		goto failed;
	}

	ftdm_set_string(newsched->name, name);
	newsched->currid = 1;

	*sched = newsched;
	ftdm_log(FTDM_LOG_DEBUG, "Created schedule %s\n", name);
	return FTDM_SUCCESS;

failed:
	ftdm_log(FTDM_LOG_CRIT, "Failed to create schedule\n");

	if (newsched) {
		if (newsched->mutex) {
			ftdm_mutex_destroy(&newsched->mutex);
		}
		ftdm_safe_free(newsched);
	}
	return FTDM_FAIL;
}

FT_DECLARE(ftdm_status_t) ftdm_sched_run(ftdm_sched_t *sched)
{
	ftdm_status_t status = FTDM_FAIL;
	ftdm_timer_t *runtimer;
	ftdm_timer_t *timer;
	ftdm_sched_callback_t callback;
	int ms = 0;
	int rc = -1;
	void *data;
	struct timeval now;

	ftdm_assert_return(sched != NULL, FTDM_EINVAL, "sched is null!\n");

tryagain:

	ftdm_mutex_lock(sched->mutex);

	rc = gettimeofday(&now, NULL);
	if (rc == -1) {
		ftdm_log(FTDM_LOG_ERROR, "Failed to retrieve time of day\n");
		goto done;
	}

	timer = sched->timers;
	while (timer) {
		runtimer = timer;
		timer = runtimer->next;

		ms = ((runtimer->time.tv_sec - now.tv_sec) * 1000) +
		     ((runtimer->time.tv_usec - now.tv_usec) / 1000);

		if (ms <= 0) {

			if (runtimer == sched->timers) {
				sched->timers = runtimer->next;
				if (sched->timers) {
					sched->timers->prev = NULL;
				}
			}

			callback = runtimer->callback;
			data = runtimer->usrdata;
			if (runtimer->next) {
				runtimer->next->prev = runtimer->prev;
			}
			if (runtimer->prev) {
				runtimer->prev->next = runtimer->next;
			}

			runtimer->id = 0;
			ftdm_safe_free(runtimer);

			/* avoid deadlocks by releasing the sched lock before triggering callbacks */
			ftdm_mutex_unlock(sched->mutex);

			callback(data);
			/* after calling a callback we must start the scanning again since the
			 * callback or some other thread may have added or cancelled timers to 
			 * the linked list */
			goto tryagain;
		}
	}

	status = FTDM_SUCCESS;
done:
	ftdm_mutex_unlock(sched->mutex);
	ftdm_unused_arg(sched);
	return status;
}

FT_DECLARE(ftdm_status_t) ftdm_sched_timer(ftdm_sched_t *sched, const char *name, 
		int ms, ftdm_sched_callback_t callback, void *data, ftdm_timer_id_t *timerid)
{
	ftdm_status_t status = FTDM_FAIL;
	struct timeval now;
	int rc = 0;
	ftdm_timer_t *newtimer;

	ftdm_assert_return(sched != NULL, FTDM_EINVAL, "sched is null!\n");
	ftdm_assert_return(name != NULL, FTDM_EINVAL, "timer name is null!\n");
	ftdm_assert_return(callback != NULL, FTDM_EINVAL, "sched callback is null!\n");
	ftdm_assert_return(ms > 0, FTDM_EINVAL, "milliseconds must be bigger than 0!\n");

	if (timerid) {
		*timerid = 0;
	}

	rc = gettimeofday(&now, NULL);
	if (rc == -1) {
		ftdm_log(FTDM_LOG_ERROR, "Failed to retrieve time of day\n");
		return FTDM_FAIL;
	}

	ftdm_mutex_lock(sched->mutex);

	newtimer = ftdm_calloc(1, sizeof(*newtimer));
	if (!newtimer) {
		goto done;
	}
	newtimer->id = sched->currid;
	sched->currid++;
	if (!sched->currid) {
		ftdm_log(FTDM_LOG_NOTICE, "Timer id wrap around for sched %s\n", sched->name);
		/* we do not want currid to be zero since is an invalid id 
		 * TODO: check that currid does not exists already in the context, it'd be insane
		 * though, having a timer to live all that time */
		sched->currid++;
	}

	ftdm_set_string(newtimer->name, name);
	newtimer->callback = callback;
	newtimer->usrdata = data;

	newtimer->time.tv_sec = now.tv_sec + (ms / 1000);
	newtimer->time.tv_usec = now.tv_usec + (ms % 1000) * 1000;
	if (newtimer->time.tv_usec >= FTDM_MICROSECONDS_PER_SECOND) {
		newtimer->time.tv_sec += 1;
		newtimer->time.tv_usec -= FTDM_MICROSECONDS_PER_SECOND;
	}

	if (!sched->timers) {
		sched->timers = newtimer;
	}  else {
		newtimer->next = sched->timers;
		sched->timers->prev = newtimer;
		sched->timers = newtimer;
	}

	if (timerid) {
		*timerid = newtimer->id;
	}

	status = FTDM_SUCCESS;
done:
	ftdm_mutex_unlock(sched->mutex);
	ftdm_unused_arg(sched);
	ftdm_unused_arg(name);
	ftdm_unused_arg(ms);
	ftdm_unused_arg(callback);
	ftdm_unused_arg(data);
	ftdm_unused_arg(timerid);
	return status;
}

FT_DECLARE(ftdm_status_t) ftdm_sched_get_time_to_next_timer(const ftdm_sched_t *sched, int32_t *timeto)
{
	ftdm_status_t status = FTDM_FAIL;
	int res = -1;
	int ms = 0;
	struct timeval currtime;
	ftdm_timer_t *current = NULL;
	ftdm_timer_t *winner = NULL;
	
	/* forever by default */
	*timeto = -1;

	ftdm_mutex_lock(sched->mutex);

	res = gettimeofday(&currtime, NULL);
	if (-1 == res) {
		ftdm_log(FTDM_LOG_ERROR, "Failed to get next event time\n");
		goto done;
	}
	status = FTDM_SUCCESS;
	current = sched->timers;
	while (current) {
		/* if no winner, set this as winner */
		if (!winner) {
			winner = current;
		}
		current = current->next;
		/* if no more timers, return the winner */
		if (!current) {
			ms = (((winner->time.tv_sec - currtime.tv_sec) * 1000) + 
			     ((winner->time.tv_usec - currtime.tv_usec) / 1000));

			/* if the timer is expired already, return 0 to attend immediately */
			if (ms < 0) {
				*timeto = 0;
				break;
			}
			*timeto = ms;
			break;
		}

		/* if the winner timer is after the current timer, then we have a new winner */
		if (winner->time.tv_sec > current->time.tv_sec
		    || (winner->time.tv_sec == current->time.tv_sec &&
		       winner->time.tv_usec > current->time.tv_usec)) {
			winner = current;
		}
	}

done:
	ftdm_mutex_unlock(sched->mutex);
	ftdm_unused_arg(timeto);
	ftdm_unused_arg(sched);
	return status;
}

FT_DECLARE(ftdm_status_t) ftdm_sched_cancel_timer(ftdm_sched_t *sched, ftdm_timer_id_t timerid)
{
	ftdm_status_t status = FTDM_FAIL;
	ftdm_timer_t *timer;

	ftdm_assert_return(sched != NULL, FTDM_EINVAL, "sched is null!\n");

	if (!timerid) {
		return FTDM_SUCCESS;
	}

	ftdm_mutex_lock(sched->mutex);

	/* look for the timer and destroy it */
	for (timer = sched->timers; timer; timer = timer->next) {
		if (timer->id == timerid) {
			if (timer == sched->timers) {
				/* it's the head timer, put a new head */
				sched->timers = timer->next;
			}
			if (timer->prev) {
				timer->prev->next = timer->next;
			}
			if (timer->next) {
				timer->next->prev = timer->prev;
			}
			ftdm_safe_free(timer);
			status = FTDM_SUCCESS;
			break;
		}
	}

	ftdm_mutex_unlock(sched->mutex);

	return status;
}

FT_DECLARE(ftdm_status_t) ftdm_sched_destroy(ftdm_sched_t **insched)
{
	ftdm_sched_t *sched = NULL;
	ftdm_timer_t *timer;
	ftdm_timer_t *deltimer;
	ftdm_assert_return(insched != NULL, FTDM_EINVAL, "sched is null!\n");
	ftdm_assert_return(*insched != NULL, FTDM_EINVAL, "sched is null!\n");

	sched = *insched;

	/* since destroying a sched may affect the global list, we gotta check */	
	ftdm_mutex_lock(sched_globals.mutex);

	/* if we're head, replace head with our next (whatever our next is, even null will do) */
	if (sched == sched_globals.freeruns) {
		sched_globals.freeruns = sched->next;
	}
	/* if we have a previous member (then we were not head) set our previous next to our next */
	if (sched->prev) {
		sched->prev->next = sched->next;
	}
	/* if we have a next then set their prev to our prev (if we were head prev will be null and sched->next is already the new head) */
	if (sched->next) {
		sched->next->prev = sched->prev;
	}

	ftdm_mutex_unlock(sched_globals.mutex);

	/* now grab the sched mutex */
	ftdm_mutex_lock(sched->mutex);

	timer = sched->timers;
	while (timer) {
		deltimer = timer;
		timer = timer->next;
		ftdm_safe_free(deltimer);
	}

	ftdm_log(FTDM_LOG_DEBUG, "Destroying schedule %s\n", sched->name);

	ftdm_mutex_unlock(sched->mutex);

	ftdm_mutex_destroy(&sched->mutex);

	ftdm_safe_free(sched);

	*insched = NULL;
	return FTDM_SUCCESS;
}

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
