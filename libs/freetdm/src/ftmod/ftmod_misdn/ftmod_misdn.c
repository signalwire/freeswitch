/**
 * mISDN HW interface
 *
 * Copyright (c) 2011, Stefan Knoblich <stkn@openisdn.net>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * NOTE: This is intended as a Layer 1 interface only, signaling
 *       is handled by other modules (e.g. ftmod_libpri or ftmod_isdn).
 */

#include <errno.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>

#include <sys/timerfd.h>

/* this is how it should have been...
#ifdef HAVE_FREETDM_FREETDM_H
#include <freetdm/freetdm.h>
#else
#include <freetdm.h>
#endif
*/
/* ... and this is how it is */
#include <private/ftdm_core.h>

#include <mISDN/mISDNif.h>

/*
 * mISDNcompat.h replaces these with references to an extern int,
 * which is exported by libmisdn... unset them and use the official
 * AF ID "34"
 */
#undef PF_ISDN
#undef AF_ISDN
#define AF_ISDN         34
#define PF_ISDN         AF_ISDN

//#define MISDN_DEBUG_EVENTS
//#define MISDN_DEBUG_IO

#ifndef MIN
#define MIN(x,y)	(((x) < (y)) ? (x) : (y))
#endif

typedef enum {
	MISDN_CAPS_NONE = 0,

	/* D-Channel */
	MISDN_CAPS_PRI = (1 << 0),
	MISDN_CAPS_BRI = (1 << 1),
	MISDN_CAPS_UP0 = (1 << 2),
	MISDN_CAPS_NT  = (1 << 3),
	MISDN_CAPS_TE  = (1 << 4),

	/* B-Channel */
	MISDN_CAPS_RAW  = (1 << 10),
	MISDN_CAPS_HDLC = (1 << 11),
} misdn_capability_flags_t;

#define MISDN_IS_BRI(x)	(x & MISDN_CAPS_BRI)
#define MISDN_IS_PRI(x)	(x & MISDN_CAPS_PRI)

#define MISDN_IS_TE(x)	(x & MISDN_CAPS_TE)
#define MISDN_IS_NT(x)	(x & MISDN_CAPS_NT)

#define MISDN_IS_RAW(x)  (x & MISDN_CAPS_RAW)
#define MISDN_IS_HDLC(x) (x & MISDN_CAPS_HDLC)


const static struct {
	const int	id;
	const char	*name;
} misdn_event_types[] = {
	{ PH_DATA_REQ,       "PH_DATA_REQ"       },
	{ PH_DATA_IND,       "PH_DATA_IND"       },
	{ PH_DATA_CNF,       "PH_DATA_CNF"       },
	{ PH_CONTROL_REQ,    "PH_CONTROL_REQ"    },
	{ PH_CONTROL_IND,    "PH_CONTROL_IND"    },
	{ PH_CONTROL_CNF,    "PH_CONTROL_CNF"    },
	{ PH_ACTIVATE_REQ,   "PH_ACTIVATE_REQ"   },
	{ PH_ACTIVATE_IND,   "PH_ACTIVATE_IND"   },
	{ PH_ACTIVATE_CNF,   "PH_ACTIVATE_CNF"   },
	{ PH_DEACTIVATE_REQ, "PH_DEACTIVATE_REQ" },
	{ PH_DEACTIVATE_IND, "PH_DEACTIVATE_IND" },
	{ PH_DEACTIVATE_CNF, "PH_DEACTIVATE_CNF" },
};

static const char *misdn_event2str(const int event)
{
	int x;

	for (x = 0; x < ftdm_array_len(misdn_event_types); x++) {
		if (event == misdn_event_types[x].id)
			return misdn_event_types[x].name;
	}
	return "unknown";
}


const static struct {
	const int	id;
	const char	*name;
} misdn_control_types[] = {
#define MISDN_CONTROL_TYPE(x)	{ x, #x }
	MISDN_CONTROL_TYPE(DTMF_HFC_COEF),
};

#if 0 /* unused for now */
static const char *misdn_control2str(const int ctrl)
{
	int x;

	for (x = 0; x < ftdm_array_len(misdn_control_types); x++) {
		if (ctrl == misdn_control_types[x].id)
			return misdn_control_types[x].name;
	}
	return "unknown";
}
#endif

/***********************************************************************************
 * mISDN <-> FreeTDM data structures
 ***********************************************************************************/

enum {
	MISDN_SPAN_NONE    = 0,
	MISDN_SPAN_RUNNING = (1 << 0),
	MISDN_SPAN_STOPPED = (1 << 1)
};

struct misdn_span_private {
	int flags;

	/* event conditional */
	pthread_mutex_t event_cond_mutex;
	pthread_cond_t  event_cond;
};

#define MISDN_CHAN_STATE_CLOSED 0
#define MISDN_CHAN_STATE_OPEN   1

struct misdn_event_queue;

struct misdn_chan_private {
	/* */
	int state;
	int debugfd;
	int timerfd;

	/* hw addr of channel */
	struct sockaddr_mISDN addr;

	/* counters */
	unsigned long tx_cnt;
	unsigned long tx_ack_cnt;
	unsigned long rx_cnt;
	unsigned long slip_rx_cnt;
	unsigned long slip_tx_cnt;

	struct misdn_event_queue *events;
};

#define ftdm_chan_io_private(x) ((x)->io_data)
#define ftdm_span_io_private(x) ((x)->io_data)

static ftdm_status_t misdn_handle_incoming(ftdm_channel_t *ftdmchan, const char *rbuf, const int size);

/***********************************************************************************
 * mISDN interface functions
 ***********************************************************************************/

/*
 * Event Queue
 */
#define MISDN_EVENTS_MAX 8

struct misdn_event {
	int id;
};

struct misdn_event_queue {
	int read_pos;
	int write_pos;
	pthread_mutex_t mutex;

	struct misdn_event events[MISDN_EVENTS_MAX];
};

/**
 * Initialize event queue
 */
static ftdm_status_t misdn_event_queue_create(struct misdn_event_queue **queue)
{
	struct misdn_event_queue *tmp = NULL;

	if (!queue)
		return FTDM_FAIL;

	tmp = calloc(1, sizeof(*tmp));
	if (!tmp)
		return FTDM_FAIL;

	pthread_mutex_init(&tmp->mutex, NULL);

	*queue = tmp;
	return FTDM_SUCCESS;
}

/**
 * Destroy event queue
 */
static ftdm_status_t misdn_event_queue_destroy(struct misdn_event_queue **queue)
{
	if (!queue || !*queue)
		return FTDM_FAIL;

	pthread_mutex_destroy(&(*queue)->mutex);
	ftdm_safe_free(*queue);
	*queue = NULL;

	return FTDM_SUCCESS;
}

static ftdm_status_t misdn_event_queue_reset(struct misdn_event_queue *queue)
{
	if (!queue)
		return FTDM_FAIL;
	pthread_mutex_lock(&queue->mutex);

	memset(queue->events, 0, sizeof(queue->events));
	queue->read_pos = queue->write_pos = 0;

	pthread_mutex_unlock(&queue->mutex);
	return FTDM_SUCCESS;
}

static ftdm_status_t misdn_event_queue_has_data(const struct misdn_event_queue *queue)
{
	if (!queue)
		return FTDM_FALSE;

	return (queue->read_pos == queue->write_pos) ? FTDM_FALSE : FTDM_TRUE;
}

static struct misdn_event *misdn_event_queue_pop(struct misdn_event_queue *queue)
{
	struct misdn_event *evt = NULL;
	int next_idx = 0;

	if (!queue)
		return NULL;

	pthread_mutex_lock(&queue->mutex);

	next_idx = (queue->read_pos + 1) % MISDN_EVENTS_MAX;

	if (queue->read_pos == queue->write_pos) {
#ifdef MISDN_DEBUG_EVENTS
		ftdm_log(FTDM_LOG_DEBUG, "mISDN queue %p: empty\n", queue);
#endif
		pthread_mutex_unlock(&queue->mutex);
		return NULL;
	}

#ifdef MISDN_DEBUG_EVENTS
	ftdm_log(FTDM_LOG_DEBUG, "mISDN queue %p: read event (read_pos: %d, write_pos: %d, next_write_pos: %d)\n",
		queue, queue->read_pos, queue->write_pos, next_idx);
#endif
	/* update read pos */
	evt = &queue->events[queue->read_pos];
	queue->read_pos = next_idx;

	pthread_mutex_unlock(&queue->mutex);
	return evt;
}

static ftdm_status_t misdn_event_queue_push(struct misdn_event_queue *queue, struct misdn_event *evt)
{
	int next_idx = 0;

	if (!queue || !evt)
		return FTDM_FAIL;

	pthread_mutex_lock(&queue->mutex);

	next_idx = (queue->write_pos + 1) % MISDN_EVENTS_MAX;

	if (next_idx == queue->read_pos) {
#ifdef MISDN_DEBUG_EVENTS
		ftdm_log(FTDM_LOG_DEBUG, "mISDN queue %p: full\n", queue);
#endif
		pthread_mutex_unlock(&queue->mutex);
		return FTDM_FAIL;
	}

#ifdef MISDN_DEBUG_EVENTS
	ftdm_log(FTDM_LOG_DEBUG, "mISDN queue %p: wrote event (read_pos: %d, write_pos: %d, next_write_pos: %d)\n",
		queue, queue->read_pos, queue->write_pos, next_idx);
#endif
	memcpy(&queue->events[queue->write_pos], evt, sizeof(*evt));
	queue->write_pos = next_idx;

	pthread_mutex_unlock(&queue->mutex);
	return FTDM_SUCCESS;
}

#if 0 /* unused for now */
static void misdn_event_queue_print_info(const struct misdn_event_queue *queue)
{
	ftdm_log(FTDM_LOG_DEBUG, "Queue %p\n\tread idx: %d\n\twrite idx: %d\n", queue,
			queue->read_pos, queue->write_pos);
}
#endif

/***********************************************************************************
 * mISDN helper functions
 ***********************************************************************************/

#define MISDN_PH_ACTIVATE_TIMEOUT_MS		10000
#define MISDN_MPH_INFORMATION_TIMEOUT_MS	3000

static inline void ts_add_msec(struct timespec *a, int msec)
{
	a->tv_sec  += (msec / 1000);
	a->tv_nsec += (msec % 1000) * 1000000;
	if (a->tv_nsec >= 1000000000) {
		a->tv_sec++;
		a->tv_nsec -= 1000000000;
	}
}

static inline int ts_sub_msec(struct timespec *a, struct timespec *b)
{
	int msec = 0;
	msec += (a->tv_sec  - b->tv_sec)  * 1000;
	msec += (a->tv_nsec - b->tv_nsec) / 1000000;
	return msec;
}

static inline int ts_after(struct timespec *a, struct timespec *b)
{
	if (a->tv_sec > b->tv_sec) return 1;
	if (a->tv_sec == b->tv_sec && a->tv_nsec > b->tv_nsec) return 1;
	return 0;
}

static inline int ts_before(struct timespec *a, struct timespec *b)
{
	if (a->tv_sec < b->tv_sec) return 1;
	if (a->tv_sec == b->tv_sec && a->tv_nsec < b->tv_nsec) return 1;
	return 0;
}

static ftdm_status_t misdn_activate_channel(ftdm_channel_t *chan, int activate)
{
	char buf[MAX_DATA_MEM] = { 0 };
	struct mISDNhead *hh = (struct mISDNhead *) buf;
	struct timespec abstimeout;
	int req = 0, resp = 0, ms_left = MISDN_PH_ACTIVATE_TIMEOUT_MS;
	int retval;

	ftdm_log_chan(chan, FTDM_LOG_DEBUG, "mISDN sending %s request\n",
		(activate) ? "activation" : "deactivation");

	/* prepare + send request primitive */
	req = (activate) ? PH_ACTIVATE_REQ : PH_DEACTIVATE_REQ;
	hh->prim = req;
	hh->id   = MISDN_ID_ANY;

	if ((retval = sendto(chan->sockfd, hh, sizeof(*hh), 0, NULL, 0)) < sizeof(*hh)) {
		ftdm_log_chan(chan, FTDM_LOG_ERROR, "mISDN failed to send activation request: %s\n",
			strerror(errno));
		return FTDM_FAIL;
	}

	clock_gettime(CLOCK_MONOTONIC, &abstimeout);
	ts_add_msec(&abstimeout, ms_left);

	/* wait for answer */
	while (1) {
		struct timespec now;
		struct pollfd pfd;

		pfd.fd = chan->sockfd;
		pfd.events  = POLLIN /* | POLLPRI */;
		pfd.revents = 0;

		switch ((retval = poll(&pfd, 1, ms_left))) {
		case  0:	/* timeout */
			goto out;
		case -1:	/* error */
			if (!(retval == EAGAIN || retval == EINTR)) {
				ftdm_log_chan(chan, FTDM_LOG_ERROR, "mISDN polling for activation confirmation failed: %s\n",
					strerror(errno));
				return FTDM_FAIL;
			}
			break;
		default:	/* read data */
			break;
		}

		if (pfd.revents & (POLLIN | POLLPRI)) {
			/* handle incoming message */
			if ((retval = recvfrom(chan->sockfd, buf, sizeof(buf), 0, NULL, NULL)) <= 0) {
				ftdm_log_chan(chan, FTDM_LOG_ERROR, "mISDN failed to receive possible answer for %s request: %s\n",
					(activate) ? "activation" : "deactivation", strerror(errno));
				return FTDM_FAIL;
			}
//#ifdef MISDN_DEBUG_EVENTS
			ftdm_log_chan(chan, FTDM_LOG_DEBUG, "mISDN got event '%s' while waiting for %s confirmation\n",
				misdn_event2str(hh->prim), (activate) ? "activation" : "deactivation");
//#endif
			switch (hh->prim) {
			case PH_ACTIVATE_IND:	/* success (or not): save last response, */
			case PH_DEACTIVATE_IND:	/* stop looping if it's the one we've been waiting for */
				resp = hh->prim;
				if (hh->prim == (activate) ? PH_ACTIVATE_IND : PH_DEACTIVATE_IND) goto out;
				break;
			case PH_ACTIVATE_CNF:
			case PH_DEACTIVATE_CNF:
				resp = hh->prim;
				if (hh->prim == (activate) ? PH_ACTIVATE_CNF : PH_DEACTIVATE_CNF) goto out;
				break;
			case PH_ACTIVATE_REQ:	/* REQ echo, ignore */
			case PH_DEACTIVATE_REQ:
				ftdm_log_chan(chan, FTDM_LOG_DEBUG, "mISDN got '%s' echo while waiting for %s confirmation (id: %#x)\n",
					misdn_event2str(hh->prim), (activate) ? "activation" : "deactivation", hh->id);
				break;
			default:		/* other messages, ignore */
				ftdm_log_chan(chan, FTDM_LOG_DEBUG, "mISDN ignoring event '%s' while waiting for %s confirmation\n",
					misdn_event2str(hh->prim), (activate) ? "activation" : "deactivation");
				break;
			}
		}

		/* check timeout */
		clock_gettime(CLOCK_MONOTONIC, &now);

		if (ts_after(&now, &abstimeout) || (ms_left = ts_sub_msec(&abstimeout, &now)) <= 0)
			goto out;
	}
out:
	if (resp == 0) {
		ftdm_log_chan(chan, FTDM_LOG_ERROR, "mISDN timeout waiting for %s confirmation\n",
			(activate) ? "activation" : "deactivation");
		return FTDM_TIMEOUT;
	}
	if ((req == PH_ACTIVATE_IND   && !(resp == PH_ACTIVATE_CNF   || resp == PH_ACTIVATE_IND)) ||
	    (req == PH_DEACTIVATE_IND && !(resp == PH_DEACTIVATE_CNF || resp == PH_DEACTIVATE_CNF))) {
		ftdm_log_chan(chan, FTDM_LOG_ERROR, "mISDN received '%s' while waiting for %s\n",
			misdn_event2str(resp), (activate) ? "activation" : "deactivation");
		return FTDM_FAIL;
	}

	ftdm_log_chan(chan, FTDM_LOG_DEBUG, "mISDN received %s confirmation\n",
		(activate) ? "activation" : "deactivation");
	return FTDM_SUCCESS;
}


#if 0 /* unused for now */
static ftdm_status_t misdn_get_ph_info(ftdm_channel_t *chan, struct ph_info *info)
{
	char buf[MAX_DATA_MEM] = { 0 };
	struct mISDNhead *hh;
	struct timespec abstimeout;
	int req = 0, resp = 0, ms_left = MISDN_MPH_INFORMATION_TIMEOUT_MS;
	int retval;

	/* prepare + send request primitive */
	req = MPH_INFORMATION_REQ;
	hh = (struct mISDNhead *)buf;
	hh->prim = req;
	hh->id   = MISDN_ID_ANY;

	ftdm_log_chan(chan, FTDM_LOG_DEBUG, "mISDN sending %s request\n",
		misdn_event2str(req));

	if ((retval = sendto(chan->sockfd, &hh, sizeof(hh), 0, NULL, 0)) < sizeof(hh)) {
		ftdm_log_chan(chan, FTDM_LOG_ERROR, "mISDN failed to send %s request: %s\n",
			misdn_event2str(req), strerror(errno));
		return FTDM_FAIL;
	}

	clock_gettime(CLOCK_MONOTONIC, &abstimeout);
	ts_add_msec(&abstimeout, ms_left);

	/* wait for answer */
	while (1) {
		struct timespec now;
		struct pollfd pfd;

		pfd.fd = chan->sockfd;
		pfd.events  = POLLIN /* | POLLPRI */;
		pfd.revents = 0;

		switch ((retval = poll(&pfd, 1, ms_left))) {
		case  0:	/* timeout */
			goto out;
		case -1:	/* error */
			if (!(retval == EAGAIN || retval == EINTR)) {
				ftdm_log_chan(chan, FTDM_LOG_ERROR, "mISDN polling for %s answer failed: %s\n",
					misdn_event2str(req), strerror(errno));
				return FTDM_FAIL;
			}
			break;
		default:	/* read data */
			break;
		}

		if (pfd.revents & (POLLIN | POLLPRI)) {
			/* handle incoming message */
			if ((retval = recvfrom(chan->sockfd, buf, sizeof(buf), 0, NULL, NULL)) <= 0) {
				ftdm_log_chan(chan, FTDM_LOG_ERROR, "mISDN failed to receive possible answer for %s request: %s\n",
					misdn_event2str(req), strerror(errno));
				return FTDM_FAIL;
			}
//#ifdef MISDN_DEBUG_EVENTS
			ftdm_log_chan(chan, FTDM_LOG_DEBUG, "mISDN got event '%s' while waiting for %s answer\n",
				misdn_event2str(hh->prim), misdn_event2str(req));
//#endif
			switch (hh->prim) {
			case MPH_INFORMATION_IND:	/* success */
				if (retval < MISDN_HEADER_LEN + sizeof(*info)) {
					ftdm_log_chan(chan, FTDM_LOG_ERROR, "mISDN answer for %s is too short\n",
						misdn_event2str(req));
					return FTDM_FAIL;
				}
				resp = hh->prim;
				/* TODO */
				goto out;
			case MPH_INFORMATION_REQ:	/* REQ echo, ignore */
				ftdm_log_chan(chan, FTDM_LOG_DEBUG, "mISDN got '%s' echo while waiting for %s answer\n",
					misdn_event2str(hh->prim), misdn_event2str(req));
				break;
			default:		/* other messages, ignore */
				ftdm_log_chan(chan, FTDM_LOG_DEBUG, "mISDN ignoring event '%s' while waiting for %s answer\n",
					misdn_event2str(hh->prim), misdn_event2str(req));
				break;
			}
		}

		/* check timeout */
		clock_gettime(CLOCK_MONOTONIC, &now);

		if (ts_after(&now, &abstimeout) || (ms_left = ts_sub_msec(&abstimeout, &now)) <= 0)
			goto out;
	}
out:
	if (resp == 0) {
		ftdm_log_chan(chan, FTDM_LOG_ERROR, "mISDN timeout waiting for %s answer\n",
			misdn_event2str(req));
		return FTDM_TIMEOUT;
	}

	return FTDM_SUCCESS;
}
#endif

static int misdn_handle_ph_control_ind(ftdm_channel_t *chan, const struct mISDNhead *hh, const void *data, const int data_len)
{
#ifdef MISDN_DEBUG_EVENTS
	ftdm_log_chan(chan, FTDM_LOG_DEBUG,
		"PH_CONTROL_IND:\n"
		"\tMessage:\t%s\n"
		"\tPayload:\t%d\n",
		misdn_control2str(hh->id), data_len);
#endif

	switch (hh->id) {
	case DTMF_HFC_COEF:
		break;
	default:
		break;
	}

	return FTDM_SUCCESS;
}

static int misdn_handle_mph_information_ind(ftdm_channel_t *chan, const struct mISDNhead *hh, const void *data, const int data_len)
{
	struct misdn_chan_private *priv = ftdm_chan_io_private(chan);
	int alarm_flags, value;

	if (data_len < sizeof(value)) {
		ftdm_log_chan_msg(chan, FTDM_LOG_ERROR, "mISDN MPH_INFORMATION_IND message is too short\n");
		return FTDM_FAIL;
	}
	value = *(int *)data;
	alarm_flags = chan->alarm_flags;

	switch (value) {
	case L1_SIGNAL_LOS_ON:
		alarm_flags |= FTDM_ALARM_RED;
		break;
	case L1_SIGNAL_LOS_OFF:
		alarm_flags &= ~FTDM_ALARM_RED;
		break;
	case L1_SIGNAL_AIS_ON:
		alarm_flags |= FTDM_ALARM_AIS;
		break;
	case L1_SIGNAL_AIS_OFF:
		alarm_flags &= ~FTDM_ALARM_AIS;
		break;
	case L1_SIGNAL_RDI_ON:
		alarm_flags |= FTDM_ALARM_YELLOW;
		break;
	case L1_SIGNAL_RDI_OFF:
		alarm_flags &= ~FTDM_ALARM_YELLOW;
		break;
	case L1_SIGNAL_SLIP_RX:
		priv->slip_rx_cnt++;
		break;
	case L1_SIGNAL_SLIP_TX:
		priv->slip_tx_cnt++;
		break;
	default:
		ftdm_log_chan(chan, FTDM_LOG_ERROR, "mISDN unknown MPH_INFORMATION_IND message: %d\n",
			value);
		return FTDM_FAIL;
	}
	if ((value = (alarm_flags ^ chan->alarm_flags))) {
		ftdm_log_chan(chan, FTDM_LOG_DEBUG, "mISDN alarm flags have changed %#x -> %#x\n",
			chan->alarm_flags, alarm_flags);
		chan->alarm_flags ^= value;
	}
	return FTDM_SUCCESS;
}

/***********************************************************************************
 * mISDN <-> FreeTDM interface functions
 ***********************************************************************************/

struct misdn_globals {
	int sockfd;
} globals;

/**
 * \brief	Open channel
 * \param	ftdmchan	FreeTDM channel to open
 */
static FIO_OPEN_FUNCTION(misdn_open)
{
	struct misdn_chan_private *chan_priv = ftdm_chan_io_private(ftdmchan);
	ftdm_span_t *span = ftdm_channel_get_span(ftdmchan);
	struct misdn_span_private *span_priv = ftdm_span_io_private(span);
	ftdm_status_t ret = 0;

	assert(chan_priv);
	assert(span_priv);

	if (chan_priv->state == MISDN_CHAN_STATE_OPEN) {
		ftdm_log_chan_msg(ftdmchan, FTDM_LOG_INFO, "mISDN channel is already open, skipping activation\n");
		return FTDM_SUCCESS;
	}

	/* flush all events */
	misdn_event_queue_reset(chan_priv->events);

	/*
	 * Send activation request
	 */
	ret = misdn_activate_channel(ftdmchan, 1);
	if (ret != FTDM_SUCCESS) {
		ftdm_log_chan(ftdmchan, FTDM_LOG_ERROR, "Failed to activate channel (socket: %d)\n",
			ftdmchan->sockfd);
		return FTDM_FAIL;
	}

	ftdm_log_chan_msg(ftdmchan, FTDM_LOG_DEBUG, "mISDN channel activation request sent\n");

	switch (ftdmchan->type) {
	case FTDM_CHAN_TYPE_B: {
			struct itimerspec its = {
				.it_interval = { 0, 0 },
				.it_value    = { 0, 0 },
			};

			its.it_interval.tv_nsec = (ftdmchan->effective_interval * 1000000);
			its.it_value.tv_nsec    = (ftdmchan->effective_interval * 1000000);

			/* create tx timerfd */
			chan_priv->timerfd = timerfd_create(CLOCK_MONOTONIC, O_NONBLOCK);
			if (chan_priv->timerfd < 0) {
				ftdm_log_chan(ftdmchan, FTDM_LOG_ERROR, "mISDN failed to create b-channel tx interval timer: %s\n",
					strerror(errno));
				return FTDM_FAIL;
			}

			/* start tx timerfd */
			ret = timerfd_settime(chan_priv->timerfd, 0, &its, NULL);
			if (ret < 0) {
				ftdm_log_chan(ftdmchan, FTDM_LOG_ERROR, "mISDN failed to start b-channel tx interval timer: %s\n",
					strerror(errno));
				return FTDM_FAIL;
			}

			ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "mISDN created tx interval (%d ms) timer\n",
				ftdmchan->effective_interval);
		}
	case FTDM_CHAN_TYPE_DQ921:
		chan_priv->state = MISDN_CHAN_STATE_OPEN;
		break;
	default:
		ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "mISDN invalid channel type '%s'\n",
			ftdm_channel_get_type(ftdmchan));
		break;
	}
	return FTDM_SUCCESS;
}

/**
 * \brief	Close channel
 * \param	ftdmchan	FreeTDM channel to close
 */
static FIO_CLOSE_FUNCTION(misdn_close)
{
	struct misdn_chan_private *chan_priv = ftdm_chan_io_private(ftdmchan);
	ftdm_status_t ret = 0;

	assert(chan_priv);

	/* deactivate b-channels on close */
	if (ftdm_channel_get_type(ftdmchan) == FTDM_CHAN_TYPE_B) {
		/*
		 * Stop tx timerfd
		 */
		if (chan_priv->timerfd >= 0) {
			close(chan_priv->timerfd);
			chan_priv->timerfd = -1;
		}

		/*
		 * Send deactivation request (don't wait for answer)
		 */
		ret = misdn_activate_channel(ftdmchan, 0);
		if (ret != FTDM_SUCCESS) {
			ftdm_log_chan_msg(ftdmchan, FTDM_LOG_ERROR, "Failed to deactivate channel\n");
			return FTDM_FAIL;
		}

		ftdm_log_chan_msg(ftdmchan, FTDM_LOG_INFO, "mISDN channel deactivated\n");
		chan_priv->state = MISDN_CHAN_STATE_CLOSED;
	}

	return FTDM_SUCCESS;
}

/**
 * \brief	Execute command
 * \param	ftdmchan	FreeTDM channel
 * \param	command	Command to execute
 * \param	obj	Additional command data
 */
static FIO_COMMAND_FUNCTION(misdn_command)
{
	switch (command) {
	case FTDM_COMMAND_NOOP:
		break;
	case FTDM_COMMAND_SET_INTERVAL:
//	case FTDM_COMMAND_GET_INTERVAL:
	case FTDM_COMMAND_SET_CODEC:
	case FTDM_COMMAND_GET_CODEC:
	case FTDM_COMMAND_SET_NATIVE_CODEC:
	case FTDM_COMMAND_GET_NATIVE_CODEC:
	case FTDM_COMMAND_ENABLE_DTMF_DETECT:
	case FTDM_COMMAND_DISABLE_DTMF_DETECT:
	case FTDM_COMMAND_SEND_DTMF:
	case FTDM_COMMAND_SET_DTMF_ON_PERIOD:
	case FTDM_COMMAND_GET_DTMF_ON_PERIOD:
	case FTDM_COMMAND_SET_DTMF_OFF_PERIOD:
	case FTDM_COMMAND_GET_DTMF_OFF_PERIOD:
	case FTDM_COMMAND_SET_RX_GAIN:	/* DSP_VOL_CHANGE_RX / HFC_VOL_CHANGE_RX */
	case FTDM_COMMAND_GET_RX_GAIN:
	case FTDM_COMMAND_SET_TX_GAIN:	/* DSP_VOL_CHANGE_TX / HFC_VOL_CHANGE_TX */
	case FTDM_COMMAND_GET_TX_GAIN:
	case FTDM_COMMAND_FLUSH_TX_BUFFERS:
	case FTDM_COMMAND_FLUSH_RX_BUFFERS:
	case FTDM_COMMAND_FLUSH_BUFFERS:
	case FTDM_COMMAND_FLUSH_IOSTATS:
	case FTDM_COMMAND_SET_PRE_BUFFER_SIZE:
	case FTDM_COMMAND_SET_LINK_STATUS:
	case FTDM_COMMAND_GET_LINK_STATUS:
	case FTDM_COMMAND_SET_RX_QUEUE_SIZE:
	case FTDM_COMMAND_SET_TX_QUEUE_SIZE:
	case FTDM_COMMAND_START_MF_PLAYBACK:
	case FTDM_COMMAND_STOP_MF_PLAYBACK:
	case FTDM_COMMAND_GET_IOSTATS:
	case FTDM_COMMAND_SWITCH_IOSTATS:
	/* Supported by mISDN */
	case FTDM_COMMAND_ENABLE_ECHOCANCEL:	/* DSP_ECHO_ON */
	case FTDM_COMMAND_DISABLE_ECHOCANCEL:	/* DSP_ECHO_OFF */
	case FTDM_COMMAND_ENABLE_LOOP:
	case FTDM_COMMAND_DISABLE_LOOP:
		ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Received unimplemented command: %d\n",
			command);
		break;

	case FTDM_COMMAND_GET_INTERVAL:
		FTDM_COMMAND_OBJ_INT = ftdm_channel_get_io_interval(ftdmchan);
		ftdm_log(FTDM_LOG_NOTICE, "Interval %d ms [%d:%d]\n",
			ftdm_channel_get_io_interval(ftdmchan), ftdm_channel_get_span_id(ftdmchan), ftdm_channel_get_id(ftdmchan));
		break;

	default:
		ftdm_log(FTDM_LOG_ERROR, "Unknown command %d\n", command);
	}
	return FTDM_SUCCESS;
}


/**
 * \brief	Wait for new data
 * \param	ftdmchan	FreeTDM channel to wait on
 * \param	flags	Wait flags
 * \param	to	Timeout
 */
static FIO_WAIT_FUNCTION(misdn_wait)
{
	struct misdn_chan_private *chan_priv = ftdm_chan_io_private(ftdmchan);
	struct pollfd pfds[2];
	int nr_fds = 0;
	int retval;

	memset(pfds, 0, sizeof(pfds));

	switch (ftdm_channel_get_type(ftdmchan)) {
	case FTDM_CHAN_TYPE_B:
		if (*flags & FTDM_WRITE) {
			pfds[nr_fds].fd = chan_priv->timerfd;
			pfds[nr_fds].events = POLLIN;
			nr_fds++;
		}
		if (*flags & (FTDM_READ | FTDM_EVENTS)) {
			pfds[nr_fds].fd = ftdmchan->sockfd;
			pfds[nr_fds].events |= (*flags & FTDM_READ)   ? POLLIN  : 0;
			pfds[nr_fds].events |= (*flags & FTDM_EVENTS) ? POLLPRI : 0;
			nr_fds++;
		}
		break;
	default:
		if (*flags & FTDM_READ)
			pfds[0].events |= POLLIN;
		if (*flags & FTDM_WRITE)
			pfds[0].events |= POLLOUT;
		if (*flags & FTDM_EVENTS)
			pfds[0].events |= POLLPRI;
		pfds[0].fd = ftdmchan->sockfd;
		nr_fds++;
		break;
	}

	*flags = FTDM_NO_FLAGS;

	if (!(pfds[0].events || pfds[1].events))
		return FTDM_SUCCESS;
	if ((retval = poll(pfds, nr_fds, to)) < 0) {
		ftdm_log_chan(ftdmchan, FTDM_LOG_ERROR, "mISDN poll() failed: %s\n",
			strerror(errno));
		return FTDM_FAIL;
	}
	if (retval == 0)
		return FTDM_TIMEOUT;

	switch (ftdm_channel_get_type(ftdmchan)) {
	case FTDM_CHAN_TYPE_B:
		if (pfds[0].fd == chan_priv->timerfd) {
			if (pfds[0].revents & POLLIN) {
				uint64_t tmp = 0;	/* clear pending events on timerfd */
				retval = read(pfds[0].fd, &tmp, sizeof(tmp));
				*flags |= FTDM_WRITE;
			}
			if (pfds[1].revents & POLLIN)
				*flags |= FTDM_READ;
			if (pfds[1].revents & POLLPRI)
				*flags |= FTDM_EVENTS;
			break;
		}
	default:
		if (pfds[0].revents & POLLIN)
			*flags |= FTDM_READ;
		if (pfds[0].revents & POLLOUT)
			*flags |= FTDM_WRITE;
		if (pfds[0].revents & POLLPRI)
			*flags |= FTDM_EVENTS;
		break;
	}
	return FTDM_SUCCESS;
}

/**
 * \brief	Read data
 * \param	ftdmchan	FreeTDM channel
 * \param	data	Buffer for data
 * \param	datalen	Number of bytes to read (contains bytes read after return)
 */
static FIO_READ_FUNCTION(misdn_read)
{
	struct misdn_chan_private *priv = ftdm_chan_io_private(ftdmchan);
	char rbuf[MAX_DATA_MEM] = { 0 };
	struct mISDNhead *hh = (struct mISDNhead *)rbuf;
	int bytes = *datalen;
	int retval;

	if (priv->state == MISDN_CHAN_STATE_CLOSED) {
		/* ignore */
		*datalen = 0;
		return FTDM_SUCCESS;
	}

	/*
	 * try to read all messages, as long as we haven't received a PH_DATA_IND one
	 * we'll get a lot of "mISDN_send: error -12" message in dmesg otherwise
	 * (= b-channel receive queue overflowing)
	 */
	while (1) {
		if ((retval = recvfrom(ftdmchan->sockfd, rbuf, sizeof(rbuf), 0, NULL, NULL)) < 0) {
			if (errno == EWOULDBLOCK) break;
			if (errno == EAGAIN) continue;
			ftdm_log_chan(ftdmchan, FTDM_LOG_ERROR, "mISDN failed to receive incoming message: %s\n",
				strerror(errno));
			return FTDM_FAIL;
		}

		if (retval < MISDN_HEADER_LEN) {
			ftdm_log_chan_msg(ftdmchan, FTDM_LOG_ERROR, "mISDN received message too small\n");
			return FTDM_FAIL;
		}

		if (hh->prim == PH_DATA_IND) {
			*datalen = MIN(bytes, retval - MISDN_HEADER_LEN);
			memcpy(data, rbuf + MISDN_HEADER_LEN, *datalen);
#ifdef MISDN_DEBUG_IO
			if (*datalen > 0) {
				char hbuf[MAX_DATA_MEM] = { 0 };
				print_hex_bytes(data, *datalen, hbuf, sizeof(hbuf));
				ftdm_log(FTDM_LOG_DEBUG, "mISDN read data: %s\n", hbuf);
			}
#endif
			return FTDM_SUCCESS;
		} else {
			*datalen = 0;
			/* event */
			misdn_handle_incoming(ftdmchan, rbuf, retval);
		}
	}
	return FTDM_SUCCESS;
}

/**
 * \brief	Write data
 * \param	ftdmchan	FreeTDM channel
 * \param	data	Buffer for data
 * \param	datalen	Number of bytes to write (contains bytes written after return)
 */
static FIO_WRITE_FUNCTION(misdn_write)
{
	struct misdn_chan_private *priv = ftdm_chan_io_private(ftdmchan);
	char wbuf[MAX_DATA_MEM];
	struct mISDNhead *hh = (struct mISDNhead *)wbuf;
	int size = *datalen;
	int retval = 0;

	assert(priv);

	/* ignore empty writes */
	if (*datalen <= 0)
		return FTDM_SUCCESS;

#ifdef MISDN_DEBUG_IO
	{
		char hbuf[MAX_DATA_MEM] = { 0 };
		print_hex_bytes(data, *datalen, hbuf, sizeof(hbuf));
		ftdm_log(FTDM_LOG_DEBUG, "mISDN write data: %s\n", hbuf);
	}
#endif
	hh->prim = PH_DATA_REQ;
	hh->id   = MISDN_ID_ANY;

	/* avoid buffer overflow */
	size = MIN(size, MAX_DATA_MEM);

	memcpy(wbuf + MISDN_HEADER_LEN, data, size);
	size += MISDN_HEADER_LEN;

#ifdef MISDN_DEBUG_IO
	ftdm_log(FTDM_LOG_DEBUG, "mISDN writing %d bytes to channel %d:%d socket %d\n",
		size, ftdm_channel_get_span_id(ftdmchan), ftdm_channel_get_id(ftdmchan), ftdmchan->sockfd);
#endif
	if ((retval = sendto(ftdmchan->sockfd, wbuf, size, 0, NULL, 0)) != size) {
		ftdm_log(FTDM_LOG_ERROR, "mISDN channel %d:%d socket write error: %s\n",
			ftdm_channel_get_span_id(ftdmchan), ftdm_channel_get_id(ftdmchan),
			strerror(errno));
		return FTDM_FAIL;
	}
	*datalen = retval;

//	if (priv->debugfd >= 0) {
//		write(priv->debugfd, wbuf + MISDN_HEADER_LEN, size  - MISDN_HEADER_LEN);
//	}

	priv->tx_cnt++;
	return FTDM_SUCCESS;
}


static ftdm_status_t misdn_open_range(ftdm_span_t *span, ftdm_chan_type_t type, struct mISDN_devinfo *devinfo, int start, int end)
{
	int num_configured = 0;
	int d_protocol, d_channel;
	int x;

	ftdm_log(FTDM_LOG_DEBUG, "mISDN configuring card:range %d:%d->%d\n",
		devinfo->id, start, end);

	switch (ftdm_span_get_trunk_type(span)) {
	case FTDM_TRUNK_E1:
		d_protocol = ISDN_P_TE_E1;
		d_channel  = 16;
		break;
	case FTDM_TRUNK_BRI:
	case FTDM_TRUNK_BRI_PTMP:
		d_protocol = ISDN_P_TE_S0;
		d_channel  = 0;
		break;
	default:
		ftdm_log(FTDM_LOG_ERROR, "Unsupported span type %s\n",
			ftdm_span_get_trunk_type_str(span));
		return FTDM_FAIL;
	}

	for (x = start; x <= end; x++) {
		struct misdn_chan_private *priv;
		struct sockaddr_mISDN addr;
		ftdm_channel_t *ftdmchan = NULL;
		ftdm_socket_t sockfd = -1;

		ftdm_log(FTDM_LOG_DEBUG, "mISDN configuring card:channel => %d:%d\n",
			devinfo->id, x);

		memset(&addr, 0, sizeof(addr));
		addr.family = AF_ISDN;
		addr.dev    = devinfo->id;

		switch (type) {
		case FTDM_CHAN_TYPE_DQ931:	/* unsupported */
			ftdm_log(FTDM_LOG_ERROR, "Unsupported channel type '%s'\n",
				ftdm_chan_type2str(type));
			return FTDM_FAIL;

		case FTDM_CHAN_TYPE_DQ921:
			/* No NT-mode support, since we have no idea which mode we should run in at this point */
			sockfd = socket(PF_ISDN, SOCK_DGRAM, d_protocol);
			addr.channel = d_channel;	/* 0 for S0 and 16 for E1 */
			break;

		case FTDM_CHAN_TYPE_B:
			if (!test_channelmap(x, devinfo->channelmap)) {
				ftdm_log(FTDM_LOG_ERROR, "Invalid B-Channel specified: %d\n", x);
				return FTDM_FAIL;
			}
			sockfd = socket(PF_ISDN, SOCK_DGRAM, ISDN_P_B_RAW);
			addr.channel = x;
			break;

		default:
			ftdm_log(FTDM_LOG_ERROR, "Invalid/unsupported channel type '%s' (%d)\n",
				ftdm_chan_type2str(type), type);
			return FTDM_FAIL;
		}

		if (sockfd < 0) {
			ftdm_log(FTDM_LOG_ERROR, "Failed to open socket: %s\n", strerror(errno));
			return FTDM_FAIL;
		}

		ftdm_log(FTDM_LOG_DEBUG, "mISDN opened socket (on chan:dev => %d:%d): %d\n",
			addr.dev, addr.channel, sockfd);

		/* set non-blocking */
		if (fcntl(sockfd, F_SETFL, O_NONBLOCK) < 0) {
			ftdm_log(FTDM_LOG_ERROR, "mISDN Failed to set socket fd to non-blocking: %s\n", strerror(errno));
			return FTDM_FAIL;
		}

		/*
		 * Bind socket to card:channel
		 */
		if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
			ftdm_log(FTDM_LOG_ERROR, "Failed to bind mISDN socket [%d:%d]: %s\n",
				addr.dev, x, strerror(errno));
			close(sockfd);
			return FTDM_FAIL;
		}

		/*
		 * Add channel to span
		 */
		if (ftdm_span_add_channel(span, sockfd, type, &ftdmchan) != FTDM_SUCCESS) {
			ftdm_log(FTDM_LOG_ERROR, "Failed to add mISDN ftdmchan to span\n");
			close(sockfd);
			return FTDM_FAIL;
		}

		priv = calloc(1, sizeof(*priv));
		if (!priv) {
			ftdm_log(FTDM_LOG_ERROR, "mISDN failed to allocate channel private data\n");
			close(sockfd);
			return FTDM_FAIL;
		}
		ftdm_chan_io_private(ftdmchan) = priv;

		priv->addr    = addr;
		priv->debugfd = -1;
		priv->timerfd = -1;

		/*
		 * Create event queue
		 */
		misdn_event_queue_create(&priv->events);

		ftdmchan->rate = 8000;
		ftdmchan->physical_span_id = devinfo->id;
		ftdmchan->physical_chan_id = x;

		if (ftdmchan->type == FTDM_CHAN_TYPE_B) {
			ftdmchan->packet_len         = 10 /* ms */ * (ftdmchan->rate / 1000);
			ftdmchan->effective_interval = ftdmchan->native_interval = ftdmchan->packet_len / 8;
			ftdmchan->native_codec       = ftdmchan->effective_codec = FTDM_CODEC_ALAW;

			ftdm_channel_set_feature(ftdmchan, FTDM_CHANNEL_FEATURE_INTERVAL);
		} else {
			ftdmchan->native_codec = ftdmchan->effective_codec = FTDM_CODEC_NONE;
		}
		num_configured++;
	}

	return num_configured;
}

static int misdn_find_device(const char *name, int nr_devices, struct mISDN_devinfo *info)
{
	struct mISDN_devinfo devinfo;
	char *endp = NULL;
	int port_id = -1;
	int i;

	port_id = strtoul(name, &endp, 10);
	if (endp == name || errno == EINVAL)
		port_id = -1;
	if (port_id < 0 || port_id >= nr_devices)
		port_id = -1;

	for (i = 0; i < nr_devices; i++) {
		memset(&devinfo, 0, sizeof(devinfo));
		devinfo.id = i;

		if (ioctl(globals.sockfd, IMGETDEVINFO, &devinfo) < 0) {
			ftdm_log(FTDM_LOG_ERROR, "mISDN unable to get device %d info: %s\n",
				devinfo.id, strerror(errno));
			return FTDM_FAIL;
		}
		if (devinfo.id == port_id)
			break;
		if (strlen(devinfo.name) <= 0)
			continue;
		if (!strcasecmp(devinfo.name, name))
			break;
	}
	if (i == nr_devices)
		return FTDM_FAIL;

	if (info) *info = devinfo;
	return FTDM_SUCCESS;
}

#define MISDN_PH_TE_PROTOCOLS(x)	\
	((x) & ((1 << ISDN_P_TE_S0) | (1 << ISDN_P_TE_E1) | (1 << ISDN_P_TE_UP0)))
#define MISDN_PH_NT_PROTOCOLS(x)	\
	((x) & ((1 << ISDN_P_NT_S0) | (1 << ISDN_P_NT_E1) | (1 << ISDN_P_NT_UP0)))

/**
 * \brief	Configure/open span ftmod_misdn settings
 */
static FIO_CONFIGURE_SPAN_FUNCTION(misdn_configure_span)
{
	struct misdn_span_private *span_priv = ftdm_span_io_private(span);
	struct mISDN_devinfo devinfo;
	int range_start = 0, range_end = 0;
	int nr_ports = 0, nr_items = 0;
	int res = 0, i;
	char *chan_str, *ptr;
	char *data = strdup(str);
	char *item_list[10];

	/* only these are supported */
	switch (ftdm_span_get_trunk_type(span)) {
	case FTDM_TRUNK_E1:
	case FTDM_TRUNK_BRI:
	case FTDM_TRUNK_BRI_PTMP:
		break;
	default:
		ftdm_log(FTDM_LOG_ERROR, "Unsupported span type %s\n",
			ftdm_span_get_trunk_type_str(span));
		return FTDM_FAIL;
	}

	/* get port count */
	if (ioctl(globals.sockfd, IMGETCOUNT, &nr_ports) < 0) {
		ftdm_log(FTDM_LOG_ERROR, "mISDN unable to get port count: %s\n",
			strerror(errno));
		goto error;
	}
	if (nr_ports <= 0) {
		ftdm_log(FTDM_LOG_ERROR, "No mISDN devices found\n");
		goto error;
	}

	/* split configuration string into port ID and channel list */
	if (!(chan_str = strchr(data, ':'))) {
		ftdm_log(FTDM_LOG_ERROR, "Invalid configuration string: %s\nExpected format <card_id>:<channel_1>[-<channel_N>]\n", str);
		goto error;
	}
	*chan_str++ = '\0';

	/* lookup port id, by number first, then by name */
	if (misdn_find_device(data, nr_ports, &devinfo) != FTDM_SUCCESS) {
		ftdm_log(FTDM_LOG_ERROR, "No such mISDN device/port: %s\n",
			data);
		goto error;
	}
	if (devinfo.nrbchan == 0 || devinfo.channelmap == 0) {
		ftdm_log(FTDM_LOG_ERROR, "mISDN device '%s' has no b-channels\n",
			data);
		goto error;
	}
	if (!MISDN_PH_TE_PROTOCOLS(devinfo.Dprotocols)) {
		ftdm_log(FTDM_LOG_ERROR, "mISDN device '%s' does not support any ISDN TE modes\n",
			data);
		goto error;
	}

	/* allocate span private */
	if (!span_priv) {
		/*
		 * Not perfect, there should be something like span_create too
		 */
		span_priv = calloc(1, sizeof(*span_priv));
		if (!span_priv) {
			ftdm_log(FTDM_LOG_ERROR, "mISDN failed to allocate span private data\n");
			return FTDM_FAIL;
		}
		ftdm_span_io_private(span) = span_priv;

		/* init event condition */
		pthread_cond_init(&span_priv->event_cond, NULL);
		pthread_mutex_init(&span_priv->event_cond_mutex, NULL);
	}

	/* split channel list by ',' */
	nr_items = ftdm_separate_string(chan_str, ',', item_list, ftdm_array_len(item_list));

	for (i = 0; i < nr_items; i++) {
		/* */
		if (!(ptr = strchr(item_list[i], '-'))) {
			/* single channel */
			range_start = atoi(item_list[i]);
			range_end   = range_start;
		} else {
			*ptr++ = '\0';
			/* channel range */
			range_start = atoi(item_list[i]);
			range_end   = atoi(ptr);
		}

		/* check if channel range/id is valid */
		if (range_start <= 0 || range_end <= 0 || range_end < range_start) {
			ftdm_log(FTDM_LOG_ERROR, "Invalid configuration string: %s\n",
				item_list[i]);
			goto error;
		}

		/* add range to span */
		res = misdn_open_range(span, type, &devinfo, range_start, range_end);
		if (res <= 0) {
			ftdm_log(FTDM_LOG_ERROR, "mISDN failed to configure channel(s)\n");
			goto error;
		}
	}

	ftdm_safe_free(data);
	return res;
error:
	ftdm_span_io_private(span) = NULL;
	ftdm_safe_free(span_priv);
	ftdm_safe_free(data);
	return res;
}

/**
 * \brief	Configure global ftmod_misdn settings
 */
static FIO_CONFIGURE_FUNCTION(misdn_configure)
{
	return FTDM_SUCCESS;
}

/**
 * \brief	Retrieve alarm event information (if any)
 * \param	ftdmchan	FreeTDM channel
 */
static FIO_GET_ALARMS_FUNCTION(misdn_get_alarms)
{
#if 0
/*
	Nope, this won't work...

	There's no way to create a separate "control" socket for a device
	that can be used to send / receive MPH_INFORMATION_REQ/_IND without
	having to care about PH_* messages in between...

	... well, unless we use our own event loop (= thread) and
	add event queues and data fifos, so we can sift all the
	messages we get to forward them to the right receiver
*/
	ftdm_span_t *span = ftdm_channel_get_span(ftdmchan);
	struct misdn_span_private *span_priv = ftdm_span_io_private(span);
	char buf[MAX_DATA_MEM] = { 0 };
	struct sockaddr_mISDN addr;
	struct mISDNhead *hh;
	struct ph_info *phi = NULL;
	struct pollfd pfd;
	socklen_t addrlen = sizeof(addr);
	int retval;

	/* use the global socket to query alarms */
	ftdm_log(FTDM_LOG_DEBUG, "mISDN getting alarms for channel %d:%d [%d:%d]\n",
		ftdm_channel_get_span_id(ftdmchan), ftdm_channel_get_id(ftdmchan),
		ftdm_channel_get_ph_span_id(ftdmchan), ftdm_channel_get_ph_id(ftdmchan));

	memset(&addr, 0, sizeof(addr));
	addr.family  = AF_ISDN;
	addr.dev     = ftdm_channel_get_ph_span_id(ftdmchan) - 1;
	addr.channel = ftdm_channel_get_ph_id(ftdmchan)      - 1;

	hh = (struct mISDNhead *)buf;
	hh->prim = MPH_INFORMATION_REQ;
	hh->id   = MISDN_ID_ANY;

	/* */
	if ((retval = sendto(span_priv->ctrlsock, hh, sizeof(*hh), 0, (struct sockaddr *)&addr, addrlen)) <= 0) {
		ftdm_log(FTDM_LOG_ERROR, "mISDN failed to send '%s' to channel %d:%d: %s\n",
			misdn_event2str(hh->prim), ftdm_channel_get_span_id(ftdmchan),
			ftdm_channel_get_id(ftdmchan), strerror(errno));
		return FTDM_FAIL;
	}

	pfd.fd = span_priv->ctrlsock;
	pfd.events  = POLLIN /*| POLLPRI*/;
	pfd.revents = 0;

	if ((retval = poll(&pfd, 1, -1)) <= 0) {
		ftdm_log(FTDM_LOG_ERROR, "mISDN failed to poll for '%s' answer on channel %d:%d: %s\n",
			misdn_event2str(hh->prim), ftdm_channel_get_span_id(ftdmchan),
			ftdm_channel_get_id(ftdmchan), strerror(errno));
		return FTDM_FAIL;
	}

	if (!(pfd.revents & (POLLIN | POLLPRI))) {
		ftdm_log(FTDM_LOG_ERROR, "mISDN failed to poll for '%s' answer on channel %d:%d: %s\n",
			misdn_event2str(hh->prim), ftdm_channel_get_span_id(ftdmchan),
			ftdm_channel_get_id(ftdmchan), "No read/pri flag");
		return FTDM_FAIL;
	}

	if ((retval = recvfrom(span_priv->ctrlsock, buf, sizeof(buf), 0, (struct sockaddr *)&addr, &addrlen)) < 0) {
		ftdm_log(FTDM_LOG_ERROR, "mISDN failed to receive answer for '%s' on channel %d:%d: %s\n",
			misdn_event2str(hh->prim), ftdm_channel_get_span_id(ftdmchan),
			ftdm_channel_get_id(ftdmchan), strerror(errno));
		return FTDM_FAIL;
	}
	if (retval < MISDN_HEADER_LEN) {
		ftdm_log(FTDM_LOG_ERROR, "mISDN short read on channel %d:%d\n",
			ftdm_channel_get_span_id(ftdmchan), ftdm_channel_get_id(ftdmchan));
		return FTDM_FAIL;
	}

	switch (hh->prim) {
	case MPH_INFORMATION_IND:
		ftdm_log(FTDM_LOG_DEBUG, "mISDN received '%s' on channel %d:%d, size %d bytes\n",
			misdn_event2str(hh->prim), ftdm_channel_get_span_id(ftdmchan),
			ftdm_channel_get_id(ftdmchan), retval);
		break;
	default:
		ftdm_log(FTDM_LOG_ERROR, "mISDN received unexpected answer '%s' on channel %d:%d: %s\n",
			misdn_event2str(hh->prim), ftdm_channel_get_span_id(ftdmchan),
			ftdm_channel_get_id(ftdmchan), strerror(errno));
		return FTDM_FAIL;
	}
#endif
	return FTDM_SUCCESS;
}


/**
 * \brief	Poll for new events
 * \param	span	FreeTDM span
 * \param	ms	Timeout (in ms)
 */
static FIO_SPAN_POLL_EVENT_FUNCTION(misdn_poll_event)
{
	struct misdn_span_private *span_priv = ftdm_span_io_private(span);
	struct timespec ts;
	int retval = 0, nr_events = 0;
	int i;

	clock_gettime(CLOCK_REALTIME, &ts);
	ts_add_msec(&ts, ms);

	for (i = 1; i <= ftdm_span_get_chan_count(span); i++) {
		ftdm_channel_t *chan = ftdm_span_get_channel(span, i);
		struct misdn_chan_private *chan_priv = ftdm_chan_io_private(chan);

		if (misdn_event_queue_has_data(chan_priv->events)) {
#ifdef MISDN_DEBUG_EVENTS
			ftdm_log(FTDM_LOG_DEBUG, "mISDN channel %d:%d has event(s)\n",
				ftdm_channel_get_span_id(chan), ftdm_channel_get_id(chan));
#endif
			ftdm_set_flag(chan, FTDM_CHANNEL_IO_EVENT);
			chan->last_event_time = ftdm_current_time_in_ms();
			nr_events++;
		}
	}
	if (nr_events)
		return FTDM_SUCCESS;

	if ((retval = pthread_cond_timedwait(&span_priv->event_cond, &span_priv->event_cond_mutex, &ts))) {
		switch (retval) {
		case ETIMEDOUT:
//			ftdm_log(FTDM_LOG_DEBUG, "mISDN span %d: No events within %d ms\n",
//				ftdm_span_get_id(span), ms);
			return FTDM_TIMEOUT;
		default:
			ftdm_log(FTDM_LOG_DEBUG, "mISDN failed to poll for events on span %d: %s\n",
				ftdm_span_get_id(span), strerror(retval));
			return FTDM_FAIL;
		}
	}

	for (i = 1; i <= ftdm_span_get_chan_count(span); i++) {
		ftdm_channel_t *chan = ftdm_span_get_channel(span, i);
		struct misdn_chan_private *chan_priv = ftdm_chan_io_private(chan);

		if (misdn_event_queue_has_data(chan_priv->events)) {
			ftdm_set_flag(chan, FTDM_CHANNEL_IO_EVENT);
			chan->last_event_time = ftdm_current_time_in_ms();
			nr_events++;
		}
	}
	return (nr_events) ? FTDM_SUCCESS : FTDM_TIMEOUT;	/* no events? => timeout */
}

/**
 * \brief	Retrieve event
 * \param	span	FreeTDM span
 * \param	event	FreeTDM event
 */
static FIO_SPAN_NEXT_EVENT_FUNCTION(misdn_next_event)
{
	int32_t event_id = FTDM_OOB_INVALID;
	int i;

	ftdm_log(FTDM_LOG_DEBUG, "Reading next event from span %d\n",
		ftdm_span_get_id(span));

	for (i = 1; i <= ftdm_span_get_chan_count(span); i++) {
		ftdm_channel_t *chan = ftdm_span_get_channel(span, i);
		struct misdn_chan_private *chan_priv = ftdm_chan_io_private(chan);
		struct misdn_event *evt = NULL;

		if (!(evt = misdn_event_queue_pop(chan_priv->events))) {
#ifdef MISDN_DEBUG_EVENTS
			ftdm_log_chan_msg(chan, FTDM_LOG_DEBUG, "mISDN channel event queue has no events\n");
#endif
                        ftdm_clear_io_flag(chan, FTDM_CHANNEL_IO_EVENT);
			continue;
		}

#ifdef MISDN_DEBUG_EVENTS
		ftdm_log_chan(chan, FTDM_LOG_DEBUG, "Got event '%s' from channel event queue\n",
			misdn_event2str(evt->id));
#endif
		switch (evt->id) {
		case PH_DEACTIVATE_IND:
			event_id = FTDM_OOB_ALARM_TRAP;
			chan->alarm_flags |= FTDM_ALARM_RED;
			break;
		case PH_ACTIVATE_IND:
			event_id = FTDM_OOB_ALARM_CLEAR;
			chan->alarm_flags &= ~FTDM_ALARM_RED;
			break;
		default:
			ftdm_log(FTDM_LOG_ERROR, "Unhandled event id %d (0x%x) %s\n",
				evt->id, evt->id, misdn_event2str(evt->id));
			continue;
		}

		chan->last_event_time = 0;
		span->event_header.e_type = FTDM_EVENT_OOB;
		span->event_header.enum_id = event_id;
		span->event_header.channel = chan;
		*event = &span->event_header;
		return FTDM_SUCCESS;
	}
	return FTDM_FAIL;
}

/**
 * \brief	Shutdown ftmod_misdn channel
 * \param	ftdmchan	FreeTDM channel
 */
static FIO_CHANNEL_DESTROY_FUNCTION(misdn_channel_destroy)
{
	struct misdn_chan_private *chan_priv = ftdm_chan_io_private(ftdmchan);
	assert(chan_priv);

	ftdm_log(FTDM_LOG_DEBUG, "Destroying channel %d:%d\n",
		ftdm_channel_get_span_id(ftdmchan),
		ftdm_channel_get_id(ftdmchan));

	if (ftdmchan->sockfd >= 0) {
		close(ftdmchan->sockfd);
		ftdmchan->sockfd = -1;
	}

	/*
	 * Destroy fifo + event queue
	 */
	if (chan_priv->events)
		misdn_event_queue_destroy(&chan_priv->events);

	ftdm_chan_io_private(ftdmchan) = NULL;
	ftdm_safe_free(chan_priv);

	ftdm_log(FTDM_LOG_DEBUG, "mISDN channel %d:%d destroyed\n",
		ftdm_channel_get_span_id(ftdmchan), ftdm_channel_get_id(ftdmchan));
	return FTDM_SUCCESS;
}

/**
 * \brief	Shutdown ftmod_misdn span
 * \param	span	FreeTDM span
 */
static FIO_SPAN_DESTROY_FUNCTION(misdn_span_destroy)
{
	struct misdn_span_private *span_priv = ftdm_span_io_private(span);

	ftdm_span_io_private(span) = NULL;
	ftdm_safe_free(span_priv);

	ftdm_log(FTDM_LOG_DEBUG, "mISDN span %d (%s) destroyed\n",
		ftdm_span_get_id(span), ftdm_span_get_name(span));
	return FTDM_SUCCESS;
}


static ftdm_status_t misdn_handle_incoming(ftdm_channel_t *ftdmchan, const char *rbuf, const int size)
{
	struct mISDNhead *hh = (struct mISDNhead *)rbuf;
	struct misdn_chan_private *priv = ftdm_chan_io_private(ftdmchan);
	const char *data = rbuf + sizeof(*hh);
	int data_len = size - sizeof(*hh);

	assert(priv);

#ifdef MISDN_DEBUG_EVENTS
	ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "mISDN channel received '%s' message (additional data: %d bytes)\n",
		misdn_event2str(hh->prim), data_len);
#endif

	switch (hh->prim) {
	/* data events */
	case PH_DATA_CNF:	/* TX ack */
		priv->tx_ack_cnt++;
		break;
	case PH_DATA_REQ:	/* request echo? */
		break;
	case PH_DATA_E_IND:	/* TX/RX ERR(?) */
		break;

	/* control events */
	case PH_ACTIVATE_REQ:
	case PH_DEACTIVATE_REQ:
		/*
		 * Echoed(?) L2->L1 requests, ignore...
		 * (something broken in mISDN or the way we setup the channel?)
		 */
		break;
	case PH_CONTROL_IND:
		return misdn_handle_ph_control_ind(ftdmchan, hh, data, data_len);
	case PH_CONTROL_REQ:
	case PH_CONTROL_CNF:
		break;

	case MPH_INFORMATION_IND:
		return misdn_handle_mph_information_ind(ftdmchan, hh, data, data_len);

	case PH_ACTIVATE_IND:
	case PH_DEACTIVATE_IND:
		{
			/* other events, enqueue and let misdn_event_next handle it */
			struct misdn_span_private *span_priv = ftdm_span_io_private(ftdmchan->span);
			struct misdn_event evt = { 0 };
			evt.id = hh->prim;

			misdn_event_queue_push(priv->events, &evt);

			/* wake possible readers */
			pthread_cond_signal(&span_priv->event_cond);
		}
		break;

	default:	/* error? */
		ftdm_log(FTDM_LOG_DEBUG, "mISDN channel %d:%d received unknown event %d\n",
			ftdm_channel_get_span_id(ftdmchan), ftdm_channel_get_id(ftdmchan), hh->prim);
		break;
	}
	return FTDM_SUCCESS;
}


/**
 * \brief	ftmod_misdn interface
 */
//static const ftdm_io_interface_t misdn_interface = {
static const ftdm_io_interface_t misdn_interface = {
	.name       = "misdn",

	.open       = misdn_open,
	.close      = misdn_close,
	.wait       = misdn_wait,
	.read       = misdn_read,
	.write      = misdn_write,

	.poll_event = misdn_poll_event,
	.next_event = misdn_next_event,

	.command         = misdn_command,
	.get_alarms      = misdn_get_alarms,
	.configure       = misdn_configure,		/* configure global parameters */
	.configure_span  = misdn_configure_span,	/* assign channels to span */
	.channel_destroy = misdn_channel_destroy,	/* clean up channel */
	.span_destroy    = misdn_span_destroy,		/* clean up span */
};


/**
 * \brief	ftmod_misdn module init function
 */
static FIO_IO_LOAD_FUNCTION(misdn_load)
{
	struct mISDNversion ver;
	struct mISDN_devinfo devinfo;
	int devcnt, usecnt;
	int i;

	/* */
	globals.sockfd = socket(PF_ISDN, SOCK_RAW, ISDN_P_BASE);
	if (globals.sockfd < 0) {
		ftdm_log(FTDM_LOG_CRIT, "Unable to create mISDN base socket (are you sure this kernel has mISDN support?)\n");
		return FTDM_FAIL;
	}

	if (ioctl(globals.sockfd, IMGETVERSION, &ver) < 0) {
		ftdm_log(FTDM_LOG_CRIT, "Unable to retrieve mISDN version\n");
		goto error;
	}

	ftdm_log(FTDM_LOG_INFO, "mISDN Interface version %hhd.%hhd.%hd\n", ver.major, ver.minor, ver.release);

	devcnt = 0;
	if (ioctl(globals.sockfd, IMGETCOUNT, &devcnt) < 0) {
		ftdm_log(FTDM_LOG_CRIT, "Unable to retrieve number of mISDN devices\n");
		goto error;

	}

	if (!devcnt) {
		ftdm_log(FTDM_LOG_CRIT, "No mISDN devices found\n");
		goto error;
	}
	usecnt = devcnt;

	ftdm_log(FTDM_LOG_INFO, "Found %d mISDN devices:\n", devcnt);

	/* Output most important device information */
	for (i = 0; i < devcnt; i++) {
		int caps = MISDN_CAPS_NONE;

		devinfo.id = i;
		if (ioctl(globals.sockfd, IMGETDEVINFO, &devinfo) < 0) {
			ftdm_log(FTDM_LOG_ERROR, "Failed to retrieve information for device %d\n", i);
			continue;
		}

		/* print */
		ftdm_log(FTDM_LOG_INFO, "<%d> Name: %s, B-Channels: %d\n",
					devinfo.id,
					ftdm_strlen_zero_buf(devinfo.name) ? "Unknown" : devinfo.name,
					devinfo.nrbchan);

		/* D-Channels capabilities */
		if (devinfo.Dprotocols & (1 << ISDN_P_TE_E1))
			caps |= MISDN_CAPS_TE | MISDN_CAPS_PRI;
		if (devinfo.Dprotocols & (1 << ISDN_P_NT_E1))
			caps |= MISDN_CAPS_NT | MISDN_CAPS_PRI;
		if (devinfo.Dprotocols & (1 << ISDN_P_TE_S0))
			caps |= MISDN_CAPS_TE | MISDN_CAPS_BRI;
		if (devinfo.Dprotocols & (1 << ISDN_P_NT_S0))
			caps |= MISDN_CAPS_NT | MISDN_CAPS_BRI;
#ifdef ISDN_P_TE_UP0
		if (devinfo.Dprotocols & (1 << ISDN_P_TE_UP0))
			caps |= MISDN_CAPS_TE | MISDN_CAPS_UP0 | MISDN_CAPS_BRI;
#endif
#ifdef ISDN_P_NT_UP0
		if (devinfo.Dprotocols & (1 << ISDN_P_NT_UP0))
			caps |= MISDN_CAPS_NT | MISDN_CAPS_UP0 | MISDN_CAPS_BRI;
#endif
		/* B-Channel capabilities */
		if (devinfo.Bprotocols & (1 << (ISDN_P_B_RAW & ISDN_P_B_MASK)))
			caps |= MISDN_CAPS_RAW;
		if (devinfo.Bprotocols & (1 << (ISDN_P_B_HDLC & ISDN_P_B_MASK)))
			caps |= MISDN_CAPS_HDLC;

		ftdm_log(FTDM_LOG_INFO, "    Type: %s, Modes: %s %s\n",
					MISDN_IS_PRI(caps) ? "PRI" : "BRI",
					MISDN_IS_NT(caps)  ? "NT" : "",
					MISDN_IS_TE(caps)  ? "TE" : "");

		ftdm_log(FTDM_LOG_INFO, "    B-Channel modes: %s %s\n",
					MISDN_IS_RAW(caps) ? "RAW" : "",
					MISDN_IS_HDLC(caps) ? "HDLC" : "");

		if (!(MISDN_IS_NT(caps) || MISDN_IS_TE(caps)) && !MISDN_IS_RAW(caps)) {
			ftdm_log(FTDM_LOG_ERROR, "   This device is unusable!\n");
			usecnt--;
		}
	}
	if (!usecnt) {
		ftdm_log(FTDM_LOG_CRIT, "No useable devices found!\n");
		goto error;
	}

	ftdm_log(FTDM_LOG_INFO, "Found %d useable mISDN devices\n", usecnt);

	/* assign interface struct */
	*fio = (ftdm_io_interface_t *)&misdn_interface;
	return FTDM_SUCCESS;
error:
	if (globals.sockfd >= 0)
		close(globals.sockfd);
	return FTDM_FAIL;
}

/**
 * \brief	ftmod_misdn module shutdown
 */
static FIO_IO_UNLOAD_FUNCTION(misdn_unload)
{
	if (globals.sockfd >= 0)
		close(globals.sockfd);
	return FTDM_SUCCESS;
}

/**
 * \brief	ftmod_misdn module
 */
ftdm_module_t ftdm_module = {
	.name      = "misdn",
	.io_load   = misdn_load,
	.io_unload = misdn_unload
};
