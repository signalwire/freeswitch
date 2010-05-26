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

struct ftdm_sched {
	ftdm_mutex_t *mutex;
	ftdm_timer_t *timers;
};

struct ftdm_timer {
	char name[80];
#ifdef __linux__
	struct timeval time;
#endif
	void *usrdata;
	ftdm_sched_callback_t callback;
	ftdm_timer_t *next;
	ftdm_timer_t *prev;
};

FT_DECLARE(ftdm_status_t) ftdm_sched_create(ftdm_sched_t **sched)
{
	ftdm_sched_t *newsched = NULL;

	ftdm_assert_return(sched != NULL, FTDM_EINVAL, "invalid pointer");

	*sched = NULL;

	newsched = ftdm_calloc(1, sizeof(*newsched));
	if (!newsched) {
		return FTDM_MEMERR;
	}

	if (ftdm_mutex_create(&newsched->mutex) != FTDM_SUCCESS) {
		goto failed;
	}

	*sched = newsched;
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
#ifdef __linux__
	struct timeval now;
#else
	ftdm_log(FTDM_LOG_CRIT, "Not implemented in this platform\n");
	return FTDM_NOTIMPL;
#endif
	ftdm_assert_return(sched != NULL, FTDM_EINVAL, "sched is null!\n");

	ftdm_mutex_lock(sched->mutex);

tryagain:

#ifdef __linux__
	rc = gettimeofday(&now, NULL);
#endif
	if (rc == -1) {
		ftdm_log(FTDM_LOG_ERROR, "Failed to retrieve time of day\n");
		goto done;
	}

	timer = sched->timers;
	while (timer) {
		runtimer = timer;
		timer = runtimer->next;

#ifdef __linux__
		ms = ((runtimer->time.tv_sec - now.tv_sec) * 1000) +
		     ((runtimer->time.tv_usec - now.tv_usec) / 1000);
#endif

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

			ftdm_safe_free(runtimer);

			callback(data);
			/* after calling a callback we must start the scanning again since the
			 * callback may have added or cancelled timers to the linked list */
			goto tryagain;
		}
	}

	status = FTDM_SUCCESS;

done:

	ftdm_mutex_unlock(sched->mutex);

	return status;
}

FT_DECLARE(ftdm_status_t) ftdm_sched_timer(ftdm_sched_t *sched, const char *name, 
		int ms, ftdm_sched_callback_t callback, void *data, ftdm_timer_t **timer)
{
#ifdef __linux__
	struct timeval now;
#endif
	int rc = 0;
	ftdm_timer_t *newtimer;
	ftdm_status_t status = FTDM_FAIL;

	ftdm_assert_return(sched != NULL, FTDM_EINVAL, "sched is null!\n");
	ftdm_assert_return(name != NULL, FTDM_EINVAL, "timer name is null!\n");
	ftdm_assert_return(callback != NULL, FTDM_EINVAL, "sched callback is null!\n");
	ftdm_assert_return(ms > 0, FTDM_EINVAL, "milliseconds must be bigger than 0!\n");

	if (timer) {
		*timer = NULL;
	}

#ifdef __linux__
	rc = gettimeofday(&now, NULL);
#else
	ftdm_log(FTDM_LOG_CRIT, "Not implemented in this platform\n");
	return FTDM_NOTIMPL;
#endif
	if (rc == -1) {
		ftdm_log(FTDM_LOG_ERROR, "Failed to retrieve time of day\n");
		return FTDM_FAIL;
	}

	ftdm_mutex_lock(sched->mutex);

	newtimer = ftdm_calloc(1, sizeof(*newtimer));
	if (!newtimer) {
		goto done;
	}

	ftdm_set_string(newtimer->name, name);
	newtimer->callback = callback;
	newtimer->usrdata = data;

#ifdef __linux__
	newtimer->time.tv_sec = now.tv_sec + (ms / 1000);
	newtimer->time.tv_usec = now.tv_usec + (ms % 1000) * 1000;
	if (newtimer->time.tv_usec >= FTDM_MICROSECONDS_PER_SECOND) {
		newtimer->time.tv_sec += 1;
		newtimer->time.tv_usec -= FTDM_MICROSECONDS_PER_SECOND;
	}
#endif

	if (!sched->timers) {
		sched->timers = newtimer;
	}  else {
		newtimer->next = sched->timers;
		sched->timers->prev = newtimer;
		sched->timers = newtimer;
	}

	if (timer) {
		*timer = newtimer;
	}
	status = FTDM_SUCCESS;
done:

	ftdm_mutex_unlock(sched->mutex);
	return status;
}

FT_DECLARE(ftdm_status_t) ftdm_sched_get_time_to_next_timer(const ftdm_sched_t *sched, int32_t *timeto)
{
	ftdm_status_t status = FTDM_FAIL;
	int res = -1;
	int ms = 0;
#ifdef __linux__
	struct timeval currtime;
#endif
	ftdm_timer_t *current = NULL;
	ftdm_timer_t *winner = NULL;

	*timeto = -1;

#ifndef __linux__
	ftdm_log(FTDM_LOG_ERROR, "Implement me!\n");
	return FTDM_NOTIMPL;
#endif

	ftdm_mutex_lock(sched->mutex);

#ifdef __linux__
	res = gettimeofday(&currtime, NULL);
#endif
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

	return status;
}

FT_DECLARE(ftdm_status_t) ftdm_sched_cancel_timer(ftdm_sched_t *sched, ftdm_timer_t **intimer)
{
	ftdm_status_t status = FTDM_FAIL;
	ftdm_timer_t *timer;

	ftdm_assert_return(sched != NULL, FTDM_EINVAL, "sched is null!\n");
	ftdm_assert_return(intimer != NULL, FTDM_EINVAL, "timer is null!\n");
	ftdm_assert_return(*intimer != NULL, FTDM_EINVAL, "timer is null!\n");

	ftdm_mutex_lock(sched->mutex);

	if (*intimer == sched->timers) {
		timer = sched->timers;
		if (timer->next) {
			sched->timers = timer->next;
			sched->timers->prev = NULL;
		} else {
			sched->timers = NULL;
		}
		ftdm_safe_free(timer);
		status = FTDM_SUCCESS;
		*intimer = NULL;
		goto done;
	}

	for (timer = sched->timers; timer; timer = timer->next) {
		if (timer == *intimer) {
			if (timer->prev) {
				timer->prev->next = timer->next;
			}
			if (timer->next) {
				timer->next->prev = timer->prev;
			}
			ftdm_log(FTDM_LOG_DEBUG, "cancelled timer %s\n", timer->name);
			ftdm_safe_free(timer);
			status = FTDM_SUCCESS;
			*intimer = NULL;
			break;
		}
	}
done:
	if (status == FTDM_FAIL) {
		ftdm_log(FTDM_LOG_ERROR, "Could not find timer %s to cancel it\n", (*intimer)->name);
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
	
	ftdm_mutex_lock(sched->mutex);

	timer = sched->timers;
	while (timer) {
		deltimer = timer;
		timer = timer->next;
		ftdm_safe_free(deltimer);
	}

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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */
