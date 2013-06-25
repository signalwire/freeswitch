/*
 * Copyright (c) 2009-2012, Anthony Minessale II
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

//#define IODEBUG

#include "private/ftdm_core.h"
#include "lpwrap_pri.h"

static struct lpwrap_pri_event_list LPWRAP_PRI_EVENT_LIST[LPWRAP_PRI_EVENT_MAX] = {
	{0, LPWRAP_PRI_EVENT_ANY, "ANY"},
	{1, LPWRAP_PRI_EVENT_DCHAN_UP, "DCHAN_UP"},
	{2, LPWRAP_PRI_EVENT_DCHAN_DOWN, "DCHAN_DOWN"},
	{3, LPWRAP_PRI_EVENT_RESTART, "RESTART"},
	{4, LPWRAP_PRI_EVENT_CONFIG_ERR, "CONFIG_ERR"},
	{5, LPWRAP_PRI_EVENT_RING, "RING"},
	{6, LPWRAP_PRI_EVENT_HANGUP, "HANGUP"},
	{7, LPWRAP_PRI_EVENT_RINGING, "RINGING"},
	{8, LPWRAP_PRI_EVENT_ANSWER, "ANSWER"},
	{9, LPWRAP_PRI_EVENT_HANGUP_ACK, "HANGUP_ACK"},
	{10, LPWRAP_PRI_EVENT_RESTART_ACK, "RESTART_ACK"},
	{11, LPWRAP_PRI_EVENT_FACILITY, "FACILITY"},
	{12, LPWRAP_PRI_EVENT_INFO_RECEIVED, "INFO_RECEIVED"},
	{13, LPWRAP_PRI_EVENT_PROCEEDING, "PROCEEDING"},
	{14, LPWRAP_PRI_EVENT_SETUP_ACK, "SETUP_ACK"},
	{15, LPWRAP_PRI_EVENT_HANGUP_REQ, "HANGUP_REQ"},
	{16, LPWRAP_PRI_EVENT_NOTIFY, "NOTIFY"},
	{17, LPWRAP_PRI_EVENT_PROGRESS, "PROGRESS"},
	{18, LPWRAP_PRI_EVENT_KEYPAD_DIGIT, "KEYPAD_DIGIT"},
	{19, LPWRAP_PRI_EVENT_IO_FAIL, "IO_FAIL"},
};

const char *lpwrap_pri_event_str(lpwrap_pri_event_t event_id)
{
	if (event_id >= LPWRAP_PRI_EVENT_MAX)
		return "";

	return LPWRAP_PRI_EVENT_LIST[event_id].name;
}

static int __pri_lpwrap_read(struct pri *pri, void *buf, int buflen)
{
	struct lpwrap_pri *spri = (struct lpwrap_pri *) pri_get_userdata(pri);
	ftdm_size_t len = buflen;
	ftdm_status_t zst;
	int res;

	if ((zst = ftdm_channel_read(spri->dchan, buf, &len)) != FTDM_SUCCESS) {
		if (zst == FTDM_FAIL) {
			ftdm_log(FTDM_LOG_CRIT, "span %d D-READ FAIL! [%s]\n", spri->span->span_id, spri->dchan->last_error);
			spri->errs++;
		} else {
			ftdm_log(FTDM_LOG_CRIT, "span %d D-READ TIMEOUT\n", spri->span->span_id);
		}
		/* we cannot return -1, libpri seems to expect values >= 0 */
		return 0;
	}
	spri->errs = 0;
	res = (int)len;

	if (res > 0) {
		memset(&((unsigned char*)buf)[res], 0, 2);
		res += 2;
#ifdef IODEBUG
		{
			char bb[2048] = { 0 };

			print_hex_bytes(buf, res - 2, bb, sizeof(bb));
			ftdm_log(FTDM_LOG_DEBUG, "READ %d\n", res - 2);
		}
#endif
	}
	return res;
}

static int __pri_lpwrap_write(struct pri *pri, void *buf, int buflen)
{
	struct lpwrap_pri *spri = (struct lpwrap_pri *) pri_get_userdata(pri);
	ftdm_size_t len = buflen - 2;

	if (ftdm_channel_write(spri->dchan, buf, buflen, &len) != FTDM_SUCCESS) {
		ftdm_log(FTDM_LOG_CRIT, "span %d D-WRITE FAIL! [%s]\n", spri->span->span_id, spri->dchan->last_error);
		/* we cannot return -1, libpri seems to expect values >= 0 */
		return 0;
	}

#ifdef IODEBUG
	{
		char bb[2048] = { 0 };

		print_hex_bytes(buf, buflen - 2, bb, sizeof(bb));
		ftdm_log(FTDM_LOG_DEBUG, "WRITE %d\n", (int)buflen - 2);
	}
#endif
	return (int)buflen;
}


/*
 * Unified init function for BRI + PRI libpri spans
 */
int lpwrap_init_pri(struct lpwrap_pri *spri, ftdm_span_t *span, ftdm_channel_t *dchan, int swtype, int node, int debug)
{
	int ret = -1;

	memset(spri, 0, sizeof(struct lpwrap_pri));
	spri->dchan = dchan;
	spri->span  = span;

	if (!spri->dchan) {
		ftdm_log(FTDM_LOG_ERROR, "No D-Channel available, unable to create BRI/PRI\n");
		return ret;
	}

	if (ftdm_mutex_create(&spri->timer_mutex) != FTDM_SUCCESS) {
		ftdm_log(FTDM_LOG_ERROR, "Failed to create timer list mutex\n");
		return ret;
	}

	switch (ftdm_span_get_trunk_type(span)) {
	case FTDM_TRUNK_E1:
	case FTDM_TRUNK_J1:
	case FTDM_TRUNK_T1:
		spri->pri = pri_new_cb(spri->dchan->sockfd, node, swtype, __pri_lpwrap_read, __pri_lpwrap_write, spri);
		break;
#ifdef HAVE_LIBPRI_BRI
	case FTDM_TRUNK_BRI:
		spri->pri = pri_new_bri_cb(spri->dchan->sockfd, 1, node, swtype, __pri_lpwrap_read, __pri_lpwrap_write, spri);
		break;
	case FTDM_TRUNK_BRI_PTMP:
		spri->pri = pri_new_bri_cb(spri->dchan->sockfd, 0, node, swtype, __pri_lpwrap_read, __pri_lpwrap_write, spri);
		break;
#endif
	default:
		ftdm_log(FTDM_LOG_CRIT, "Invalid/unsupported trunk type '%s'\n",
			ftdm_span_get_trunk_type_str(span));
		ftdm_mutex_destroy(&spri->timer_mutex);
		return ret;
	}

	if (spri->pri) {
		pri_set_debug(spri->pri, debug);
#ifdef HAVE_LIBPRI_BRI
		/* "follow Q.931 Section 5.3.2 call hangup better" */
		pri_hangup_fix_enable(spri->pri, 1);
#endif
#ifdef HAVE_LIBPRI_AOC
		pri_aoc_events_enable(spri->pri, 1);
#endif
		ret = 0;
	} else {
		ftdm_log(FTDM_LOG_CRIT, "Unable to create BRI/PRI\n");
		ftdm_mutex_destroy(&spri->timer_mutex);
	}
	return ret;
}


#define timeval_to_ms(x) \
	(ftdm_time_t)(((x)->tv_sec * 1000) + ((x)->tv_usec / 1000))

int lpwrap_start_timer(struct lpwrap_pri *spri, struct lpwrap_timer *timer, const uint32_t timeout_ms, timeout_handler callback)
{
	struct lpwrap_timer **prev, *cur;

	if (!spri || !timer || timer->timeout)
		return -1;

	ftdm_log_chan(spri->dchan, FTDM_LOG_DEBUG, "-- Starting timer %p with timeout %u ms\n",
		timer, timeout_ms);

	timer->timeout  = ftdm_current_time_in_ms() + timeout_ms;
	timer->callback = callback;
	timer->next     = NULL;

	ftdm_mutex_lock(spri->timer_mutex);

	for (prev = &spri->timer_list, cur = spri->timer_list; cur; prev = &(*prev)->next, cur = cur->next) {
		if (cur->timeout > timer->timeout) {
			*prev = timer;
			timer->next = cur;
			break;
		}
	}
	if (!cur) {
		*prev = timer;
	}

	ftdm_mutex_unlock(spri->timer_mutex);
	return 0;
}

int lpwrap_stop_timer(struct lpwrap_pri *spri, struct lpwrap_timer *timer)
{
	struct lpwrap_timer **prev, *cur;

	if (!spri || !timer)
		return -1;

	if (!timer->timeout)
		return 0;

	ftdm_log_chan(spri->dchan, FTDM_LOG_DEBUG, "-- Stopping timer %p\n", timer);

	ftdm_mutex_lock(spri->timer_mutex);

	for (prev = &spri->timer_list, cur = spri->timer_list; cur; prev = &(*prev)->next, cur = cur->next) {
		if (cur == timer) {
			*prev = cur->next;
			break;
		}
	}

	ftdm_mutex_unlock(spri->timer_mutex);

	if (!cur) {
		ftdm_log_chan(spri->dchan, FTDM_LOG_WARNING, "-- Timer %p not found in list\n", timer);
	}

	timer->next     = NULL;
	timer->timeout  = 0;
	timer->callback = NULL;
	return 0;
}

static struct lpwrap_timer *lpwrap_timer_next(struct lpwrap_pri *spri)
{
	return spri ? spri->timer_list : NULL;
}

static int lpwrap_run_expired(struct lpwrap_pri *spri, ftdm_time_t now_ms)
{
	struct lpwrap_timer *expired_list = NULL;
	struct lpwrap_timer **prev, *cur;

	if (!spri || !spri->timer_list)
		return 0;

	ftdm_mutex_lock(spri->timer_mutex);

	/* Move all timers to expired list */
	expired_list = spri->timer_list;

	for (prev = &expired_list, cur = expired_list; cur; prev = &(*prev)->next, cur = cur->next) {
		if (cur->timeout > now_ms) {
			*prev = NULL;
			break;
		}
	}
	/* Move non-expired timer to front of timer_list (or clear list if there are none) */
	spri->timer_list = cur;

	ftdm_mutex_unlock(spri->timer_mutex);

	/* fire callbacks */
	while ((cur = expired_list)) {
		timeout_handler handler = cur->callback;
		expired_list = cur->next;

		/* Stop timer */
		cur->next     = NULL;
		cur->timeout  = 0;
		cur->callback = NULL;

		if (handler)
			handler(spri, cur);
	}
	return 0;
}


#define LPWRAP_MAX_TIMEOUT_MS	100
#define LPWRAP_MAX_ERRORS	2

int lpwrap_run_pri_once(struct lpwrap_pri *spri)
{
	struct timeval *next = NULL;
	struct lpwrap_timer *timer = NULL;
	pri_event *event = NULL;
	ftdm_wait_flag_t flags;
	ftdm_time_t now_ms, next_ms, timeout_ms, tmp_ms;
	int ret;

	if (spri->on_loop) {
		if ((ret = spri->on_loop(spri)) < 0)
			return FTDM_FAIL;
	}

	/* Default timeout when no scheduled events are pending */
	timeout_ms = LPWRAP_MAX_TIMEOUT_MS;
	next_ms    = 0;
	now_ms     = ftdm_current_time_in_ms();

	/*
	 * Get the next scheduled timer from libpri to set the maximum timeout,
	 * but limit it to MAX_TIMEOUT_MS (100ms).
	 */
	if ((next = pri_schedule_next(spri->pri))) {
		next_ms = timeval_to_ms(next);
		if (now_ms >= next_ms) {
			/* Already late, handle timeout */
			timeout_ms = 0;
		} else {
			/* Calculate new timeout and limit it to MAX_TIMEOUT_MS miliseconds */
			tmp_ms     = ftdm_min(next_ms - now_ms, LPWRAP_MAX_TIMEOUT_MS);
			timeout_ms = ftdm_min(timeout_ms, tmp_ms);
		}
	}

	/*
	 * Next lpwrap_timer timeout
	 */
	if ((timer = lpwrap_timer_next(spri))) {
		if (now_ms >= timer->timeout) {
			/* Already late, handle timeout */
			timeout_ms = 0;
		} else {
			/* Calculate new timeout and limit it to MAX_TIMEOUT_MS miliseconds */
			tmp_ms     = ftdm_min(timer->timeout - now_ms, LPWRAP_MAX_TIMEOUT_MS);
			timeout_ms = ftdm_min(timeout_ms, tmp_ms);
		}
	}

	/* */
	if (timeout_ms > 0) {
		flags = FTDM_READ | FTDM_EVENTS;
		ret = ftdm_channel_wait(spri->dchan, &flags, timeout_ms);

		if (spri->flags & LPWRAP_PRI_ABORT)
			return FTDM_SUCCESS;

		if (ret == FTDM_TIMEOUT) {
			now_ms = ftdm_current_time_in_ms();

			if (next) {
				if (next_ms < now_ms) {
					ftdm_log_chan(spri->dchan, FTDM_LOG_DEBUG, "pri timer %d ms late\n",
						(int)(now_ms - next_ms));
				}
				event = pri_schedule_run(spri->pri);
			}
			if (timer) {
				if (timer->timeout < now_ms) {
					ftdm_log_chan(spri->dchan, FTDM_LOG_DEBUG, "lpwrap timer %d ms late\n",
						(int)(now_ms - timer->timeout));
				}
				lpwrap_run_expired(spri, now_ms);
			}
		} else if (flags & (FTDM_READ | FTDM_EVENTS)) {
			event = pri_check_event(spri->pri);
		}
	} else {
		/*
		 * Scheduled event has already expired, handle it immediately
		 */
		if (next) {
			event = pri_schedule_run(spri->pri);
		}
		if (timer) {
			lpwrap_run_expired(spri, now_ms);
		}
	}

	if (spri->flags & LPWRAP_PRI_ABORT)
		return FTDM_SUCCESS;

	if (event) {
		event_handler handler;

		/* 0 is catchall event handler */
		if (event->e < 0 || event->e >= LPWRAP_PRI_EVENT_MAX) {
			handler = spri->eventmap[0];
		} else if (spri->eventmap[event->e]) {
			handler = spri->eventmap[event->e];
		} else {
			handler = spri->eventmap[0];
		}

		if (handler) {
			handler(spri, event->e, event);
		} else {
			ftdm_log(FTDM_LOG_CRIT, "No event handler found for event %d.\n", event->e);
		}
	}

	return FTDM_SUCCESS;
}

int lpwrap_run_pri(struct lpwrap_pri *spri)
{
	int ret = 0;

	while (!(spri->flags & LPWRAP_PRI_ABORT)) {
		ret = lpwrap_run_pri_once(spri);
		if (ret) {
			ftdm_log(FTDM_LOG_ERROR, "Error = %d, [%s]\n",
				ret, strerror(errno));
			spri->errs++;
		} else {
			spri->errs = 0;
		}
		if (!ftdm_running())
			break;
		if (spri->errs >= LPWRAP_MAX_ERRORS) {
			ftdm_log(FTDM_LOG_CRIT, "Too many errors on span, restarting\n");
			spri->errs = 0;
			break;
		}
	}
	return ret;
}

int lpwrap_stop_pri(struct lpwrap_pri *spri)
{
	spri->flags |= LPWRAP_PRI_ABORT;
	return FTDM_SUCCESS;
}

int lpwrap_destroy_pri(struct lpwrap_pri *spri)
{
	if (spri->timer_mutex)
		ftdm_mutex_destroy(&spri->timer_mutex);
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
