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
/*
 * TODO:
 *	- Use a fifo and PH_DATA_CNF for b-channel write polling (drop timerfd)
 *
 *	- Disable L1 idle deactivation on BRI PTMP with IMGL1HOLD ioctl(? optional)
 *
 *	- Add hfcsusb specific state + flag defines and try to do something useful with
 *	  it in misdn_handle_mph_information_ind().
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
#include <sys/epoll.h>

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

#define ACCESS_ONCE(x)	(*(volatile typeof(x) *)&(x))

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

#define MISDN_MSG_DATA(x) ((void *)((unsigned char *)(x) + MISDN_HEADER_LEN))

const static struct {
	const int	id;
	const char	*name;
} misdn_event_types[] = {
#define MISDN_EVENT_TYPE(x)	{ x, #x }
	MISDN_EVENT_TYPE(PH_DATA_REQ),
	MISDN_EVENT_TYPE(PH_DATA_IND),
	MISDN_EVENT_TYPE(PH_DATA_CNF),
	MISDN_EVENT_TYPE(PH_DATA_E_IND),
	MISDN_EVENT_TYPE(PH_CONTROL_REQ),
	MISDN_EVENT_TYPE(PH_CONTROL_IND),
	MISDN_EVENT_TYPE(PH_CONTROL_CNF),
	MISDN_EVENT_TYPE(PH_ACTIVATE_REQ),
	MISDN_EVENT_TYPE(PH_ACTIVATE_IND),
	MISDN_EVENT_TYPE(PH_ACTIVATE_CNF),
	MISDN_EVENT_TYPE(PH_DEACTIVATE_REQ),
	MISDN_EVENT_TYPE(PH_DEACTIVATE_IND),
	MISDN_EVENT_TYPE(PH_DEACTIVATE_CNF),
	MISDN_EVENT_TYPE(MPH_ACTIVATE_REQ),
	MISDN_EVENT_TYPE(MPH_ACTIVATE_IND),
	MISDN_EVENT_TYPE(MPH_DEACTIVATE_REQ),
	MISDN_EVENT_TYPE(MPH_DEACTIVATE_IND),
	MISDN_EVENT_TYPE(MPH_INFORMATION_REQ),
	MISDN_EVENT_TYPE(MPH_INFORMATION_IND),
#undef MISDN_EVENT_TYPE
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
	MISDN_CONTROL_TYPE(DSP_CONF_JOIN),
	MISDN_CONTROL_TYPE(DSP_CONF_SPLIT),
	MISDN_CONTROL_TYPE(DSP_RECEIVE_OFF),
	MISDN_CONTROL_TYPE(DSP_RECEIVE_ON),
	MISDN_CONTROL_TYPE(DSP_ECHO_ON),
	MISDN_CONTROL_TYPE(DSP_ECHO_OFF),
	MISDN_CONTROL_TYPE(DSP_MIX_ON),
	MISDN_CONTROL_TYPE(DSP_MIX_OFF),
	MISDN_CONTROL_TYPE(DSP_DELAY),
	MISDN_CONTROL_TYPE(DSP_JITTER),
	MISDN_CONTROL_TYPE(DSP_TXDATA_ON),
	MISDN_CONTROL_TYPE(DSP_TXDATA_OFF),
	MISDN_CONTROL_TYPE(DSP_TX_DEJITTER),
	MISDN_CONTROL_TYPE(DSP_TX_DEJ_OFF),
	MISDN_CONTROL_TYPE(DSP_TONE_PATT_ON),
	MISDN_CONTROL_TYPE(DSP_TONE_PATT_OFF),
	MISDN_CONTROL_TYPE(DSP_VOL_CHANGE_TX),
	MISDN_CONTROL_TYPE(DSP_VOL_CHANGE_RX),
	MISDN_CONTROL_TYPE(DSP_BF_ENABLE_KEY),
	MISDN_CONTROL_TYPE(DSP_BF_DISABLE),
	MISDN_CONTROL_TYPE(DSP_BF_ACCEPT),
	MISDN_CONTROL_TYPE(DSP_BF_REJECT),
	MISDN_CONTROL_TYPE(DSP_PIPELINE_CFG),
	MISDN_CONTROL_TYPE(HFC_VOL_CHANGE_TX),
	MISDN_CONTROL_TYPE(HFC_VOL_CHANGE_RX),
	MISDN_CONTROL_TYPE(HFC_SPL_LOOP_ON),
	MISDN_CONTROL_TYPE(HFC_SPL_LOOP_OFF),
#undef MISDN_CONTROL_TYPE
};

static const char *misdn_control2str(const int ctrl)
{
	int x;

	for (x = 0; x < ftdm_array_len(misdn_control_types); x++) {
		if (ctrl == misdn_control_types[x].id)
			return misdn_control_types[x].name;
	}
	return "unknown";
}


/***********************************************************************************
 * mISDN <-> FreeTDM audio conversion
 ***********************************************************************************/

/*
 * Code used to generate table values taken from
 * Linux Call Router (LCR) http://www.linux-call-router.de/
 *
 * chan_lcr.c:3488 ff., load_module()
 */
static const unsigned char conv_audio_tbl[256] = {
	0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0,
	0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0,
	0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8,
	0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8,
	0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0xe4,
	0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4,
	0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec,
	0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc,
	0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2,
	0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2,
	0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea,
	0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa,
	0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6,
	0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6,
	0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee,
	0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe,
	0x01, 0x81, 0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1,
	0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1,
	0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9,
	0x19, 0x99, 0x59, 0xd9, 0x39, 0xb9, 0x79, 0xf9,
	0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5,
	0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5,
	0x0d, 0x8d, 0x4d, 0xcd, 0x2d, 0xad, 0x6d, 0xed,
	0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd,
	0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3,
	0x13, 0x93, 0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3,
	0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb,
	0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb,
	0x07, 0x87, 0x47, 0xc7, 0x27, 0xa7, 0x67, 0xe7,
	0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7,
	0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef,
	0x1f, 0x9f, 0x5f, 0xdf, 0x3f, 0xbf, 0x7f, 0xff,
};

/* Convert ISDN_P_B_RAW audio data to/from a-/u-law */
static inline void misdn_convert_audio_bits(char *buf, int buflen)
{
	int i;

	for (i = 0; i < buflen; i++) {
		 buf[i] = conv_audio_tbl[(unsigned char)buf[i]];
	}
}


/***********************************************************************************
 * mISDN <-> FreeTDM data structures
 ***********************************************************************************/

typedef enum {
	MISDN_CMD_NONE = 0,
	MISDN_CMD_STOP,		/*!< Stop the I/O thread */
} misdn_cmd_t;

struct misdn_command {
	misdn_cmd_t type;
/*	union { } cmd; */	/*!< Command-specific parameters */
};

enum {
	MISDN_SPAN_NONE    = 0,
	MISDN_SPAN_RUNNING = (1 << 0),
	MISDN_SPAN_STOPPED = (1 << 1)
};

struct misdn_span_private {
	int flags;
	int running;

	int event_pipe_in;
	int event_pipe_out;

	/* event conditional */
	pthread_mutex_t event_cond_mutex;
	pthread_cond_t  event_cond;

	/* start / stop feedback */
	pthread_mutex_t ctrl_cond_mutex;
	pthread_cond_t  ctrl_cond;
};

struct misdn_event_queue;

struct misdn_chan_private {
	/* */
	int debugfd;
	int active;

	/* hw addr of channel */
	struct sockaddr_mISDN addr;

	/* audio tx pipe (= socketpair ends) */
	int tx_audio_pipe_in;
	int tx_audio_pipe_out;
	int rx_audio_pipe_in;
	int rx_audio_pipe_out;

	/* counters */
	unsigned long tx_cnt;
	unsigned long tx_ack_cnt;
	unsigned long rx_cnt;
	unsigned long slip_rx_cnt;
	unsigned long slip_tx_cnt;

	unsigned long tx_pipe_wr_bytes;		/*!< Number of bytes written into tx audio pipe */
	unsigned long tx_pipe_rd_bytes;		/*!< Number of bytes read from tx audio pipe */
	unsigned long tx_miss_bytes;		/*!< Number of bytes missing in short reads from tx audio pipe */
	unsigned long tx_lost_bytes;		/*!< Number of bytes lost in short writes to the mISDN B-Channel */
	unsigned long tx_sent_bytes;		/*!< Number of bytes successfully sent to the mISDN B-Channel */
	unsigned long tx_pipe_under_cnt;	/*!< Number of tx audio pipe underflows */
	unsigned long tx_pipe_over_cnt;		/*!< Number of tx audio pipe overflows  */

	struct misdn_event_queue *events;
};

#define ftdm_chan_io_private(x) ((x)->io_data)
#define ftdm_span_io_private(x) ((x)->io_data)

static ftdm_status_t misdn_handle_incoming(ftdm_channel_t *ftdmchan, const char *rbuf, const int size);
static int misdn_handle_mph_information_ind(ftdm_channel_t *chan, const struct mISDNhead *hh, const void *data, const int data_len);

static const char *ftdm_channel_get_type_str(const ftdm_channel_t *chan)
{
	return ftdm_chan_type2str(ftdm_channel_get_type(chan));
}

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

/*
 * Asynchronous channel (de-)activation
 */
static ftdm_status_t _misdn_toggle_channel_nowait(ftdm_channel_t *chan, int activate)
{
	struct misdn_chan_private *priv = ftdm_chan_io_private(chan);
	char buf[MAX_DATA_MEM] = { 0 };
	struct mISDNhead *hh = (struct mISDNhead *) buf;
	int retval;

	/* NOTE: sending PH_DEACTIVATE_REQ to closed b-channels kills the d-channel (hfcsusb)... */
	if ((activate && priv->active) || (!activate && !priv->active))
		return FTDM_SUCCESS;

	ftdm_log_chan(chan, FTDM_LOG_DEBUG, "mISDN sending %s request\n",
		(activate) ? "activation" : "deactivation");

	/* prepare + send request primitive */
	hh->prim = (activate) ? PH_ACTIVATE_REQ : PH_DEACTIVATE_REQ;
	hh->id   = MISDN_ID_ANY;

	if ((retval = sendto(chan->sockfd, hh, sizeof(*hh), 0, NULL, 0)) < sizeof(*hh)) {
		ftdm_log_chan(chan, FTDM_LOG_ERROR, "mISDN failed to send activation request: %s\n",
			strerror(errno));
		return FTDM_FAIL;
	}

	return FTDM_SUCCESS;
}

static ftdm_status_t misdn_activate_channel_nowait(ftdm_channel_t *chan)
{
	return _misdn_toggle_channel_nowait(chan, 1);
}

static ftdm_status_t misdn_deactivate_channel_nowait(ftdm_channel_t *chan)
{
	return _misdn_toggle_channel_nowait(chan, 0);
}

/*
 * Synchronous channel (de-)activation
 */
static ftdm_status_t _misdn_toggle_channel(ftdm_channel_t *chan, int activate)
{
	struct misdn_chan_private *priv = ftdm_chan_io_private(chan);
	char buf[MAX_DATA_MEM] = { 0 };
	struct mISDNhead *hh = (struct mISDNhead *) buf;
	struct timespec abstimeout;
	int req = 0, resp = 0, ms_left = MISDN_PH_ACTIVATE_TIMEOUT_MS;
	int retval;

	/* NOTE: sending PH_DEACTIVATE_REQ to closed b-channels kills the d-channel (hfcsusb)... */
	if ((activate && priv->active) || (!activate && !priv->active))
		return FTDM_SUCCESS;

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
		pfd.events  = POLLIN | POLLPRI;
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
#ifdef MISDN_DEBUG_EVENTS
			ftdm_log_chan(chan, FTDM_LOG_DEBUG, "mISDN got event '%s (%#x)', id %#x, while waiting for %s confirmation on %c-channel\n",
				misdn_event2str(hh->prim), hh->prim, hh->id, (activate) ? "activation" : "deactivation",
				ftdm_channel_get_type(chan) == FTDM_CHAN_TYPE_B ? 'B' : 'D');
#endif
			switch (hh->prim) {
			case PH_ACTIVATE_IND:
			case PH_ACTIVATE_CNF:
				resp = hh->prim;
				priv->active = 1;
				if (activate) goto out;
				break;
			case PH_DEACTIVATE_CNF:
			case PH_DEACTIVATE_IND:
				resp = hh->prim;
				priv->active = 0;
				if (!activate) goto out;
				break;
			case PH_ACTIVATE_REQ:	/* REQ echo, ignore */
			case PH_DEACTIVATE_REQ:
				ftdm_log_chan(chan, FTDM_LOG_DEBUG, "mISDN got '%s' echo while waiting for %s confirmation (id: %#x)\n",
					misdn_event2str(hh->prim), (activate) ? "activation" : "deactivation", hh->id);
				break;
			case MPH_INFORMATION_IND:
				misdn_handle_mph_information_ind(chan, hh, MISDN_MSG_DATA(buf), retval - MISDN_HEADER_LEN);
				break;
			case PH_DATA_IND:	/* ignore */
			case PH_DATA_CNF:	/* ignore */
				break;
			default:		/* other messages, ignore */
				ftdm_log_chan(chan, FTDM_LOG_DEBUG, "mISDN ignoring event '%s (%#x)', id %#x, while waiting for %s confirmation\n",
					misdn_event2str(hh->prim), hh->prim, hh->id, (activate) ? "activation" : "deactivation");
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
	if ((req == PH_ACTIVATE_REQ   && !(resp == PH_ACTIVATE_CNF   || resp == PH_ACTIVATE_IND)) ||
	    (req == PH_DEACTIVATE_REQ && !(resp == PH_DEACTIVATE_CNF || resp == PH_DEACTIVATE_IND))) {
		ftdm_log_chan(chan, FTDM_LOG_ERROR, "mISDN received '%s' while waiting for %s\n",
			misdn_event2str(resp), (activate) ? "activation" : "deactivation");
		return FTDM_FAIL;
	}

	ftdm_log_chan(chan, FTDM_LOG_DEBUG, "mISDN received %s confirmation\n",
		(activate) ? "activation" : "deactivation");
	return FTDM_SUCCESS;
}

static ftdm_status_t misdn_activate_channel(ftdm_channel_t *chan)
{
	return _misdn_toggle_channel(chan, 1);
}

static ftdm_status_t misdn_deactivate_channel(ftdm_channel_t *chan)
{
	return _misdn_toggle_channel(chan, 0);
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
#ifdef MISDN_DEBUG_EVENTS
			ftdm_log_chan(chan, FTDM_LOG_DEBUG, "mISDN got event '%s' while waiting for %s answer\n",
				misdn_event2str(hh->prim), misdn_event2str(req));
#endif
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
	ftdm_log_chan(chan, FTDM_LOG_DEBUG,
		"PH_CONTROL_IND:\n"
		"\tMessage:\t%s (%#x)\n"
		"\tPayload:\t%d\n",
		misdn_control2str(hh->id), hh->id, data_len);

	switch (hh->id) {
	case DTMF_HFC_COEF:
		break;
	default:
		break;
	}

	return FTDM_SUCCESS;
}

/*
 * TE/NT state names
 * taken from linux-3.2.1/drivers/isdn/hardware/mISDN/hfcsusb.h
 */
static const char *misdn_layer1_te_states[] = {
        "TE F0 - Reset",
        "TE F1 - Reset",
        "TE F2 - Sensing",
        "TE F3 - Deactivated",
        "TE F4 - Awaiting signal",
        "TE F5 - Identifying input",
        "TE F6 - Synchronized",
        "TE F7 - Activated",
        "TE F8 - Lost framing",
};

static const char *misdn_layer1_nt_states[] = {
        "NT G0 - Reset",
        "NT G1 - Deactive",
        "NT G2 - Pending activation",
        "NT G3 - Active",
        "NT G4 - Pending deactivation",
};

static const char *misdn_hw_state_name(const int proto, const int id)
{
	if (IS_ISDN_P_TE(proto)) {
		if (id < 0 || id >= ftdm_array_len(misdn_layer1_te_states))
			return NULL;
		return misdn_layer1_te_states[id];
	}
	else if (IS_ISDN_P_NT(proto)) {
		if (id < 0 || id >= ftdm_array_len(misdn_layer1_nt_states))
			return NULL;
		return misdn_layer1_nt_states[id];
	}
	return NULL;
}


static const struct misdn_hw_flag {
	const unsigned int flag;
	const char *name;
} misdn_hw_flags[] = {
#define MISDN_HW_FLAG(v,n)	{ v, #n }
	MISDN_HW_FLAG(0, FLG_TX_BUSY),
	MISDN_HW_FLAG(1, FLG_TX_NEXT),
	MISDN_HW_FLAG(2, FLG_L1_BUSY),
	MISDN_HW_FLAG(3, FLG_L2_ACTIVATED),
	MISDN_HW_FLAG(5, FLG_OPEN),
	MISDN_HW_FLAG(6, FLG_ACTIVE),
	MISDN_HW_FLAG(7, FLG_BUSY_TIMER),
	MISDN_HW_FLAG(8, FLG_DCHANNEL),
	MISDN_HW_FLAG(9, FLG_BCHANNEL),
	MISDN_HW_FLAG(10, FLG_ECHANNEL),
	MISDN_HW_FLAG(12, FLG_TRANSPARENT),
	MISDN_HW_FLAG(13, FLG_HDLC),
	MISDN_HW_FLAG(14, FLG_L2DATA),
	MISDN_HW_FLAG(15, FLG_ORIGIN),
	MISDN_HW_FLAG(16, FLG_FILLEMPTY),
	MISDN_HW_FLAG(17, FLG_ARCOFI_TIMER),
	MISDN_HW_FLAG(18, FLG_ARCOFI_ERROR),
	MISDN_HW_FLAG(17, FLG_INITIALIZED),
	MISDN_HW_FLAG(18, FLG_DLEETX),
	MISDN_HW_FLAG(19, FLG_LASTDLE),
	MISDN_HW_FLAG(20, FLG_FIRST),
	MISDN_HW_FLAG(21, FLG_LASTDATA),
	MISDN_HW_FLAG(22, FLG_NMD_DATA),
	MISDN_HW_FLAG(23, FLG_FTI_RUN),
	MISDN_HW_FLAG(24, FLG_LL_OK),
	MISDN_HW_FLAG(25, FLG_LL_CONN),
	MISDN_HW_FLAG(26, FLG_DTMFSEND),
	MISDN_HW_FLAG(30, FLG_RECVQUEUE),
	MISDN_HW_FLAG(31, FLG_PHCHANGE),
#undef MISDN_HW_FLAG
};

static const char *misdn_hw_print_flags(unsigned int flags, char *buf, int buflen)
{
	int i;

	buf[0] = '\0';
	for (i = 0; i < ftdm_array_len(misdn_hw_flags); i++) {
		if ((1 << misdn_hw_flags[i].flag) & flags) {
			strncat(buf, misdn_hw_flags[i].name, buflen);
			flags &= ~(1 << misdn_hw_flags[i].flag);
			if (!flags) break;
			strncat(buf, ",", buflen);
		}
	}
	return buf;
}

static int misdn_handle_mph_information_ind(ftdm_channel_t *chan, const struct mISDNhead *hh, const void *data, const int data_len)
{
	struct misdn_chan_private *priv = ftdm_chan_io_private(chan);

	/*
	 * mISDN has some inconsistency issues here.
	 *
	 * There are only two drivers that emit MPH_INFORMATION_IND messages,
	 * hfcsusb and hfcmulti. The former sends a set of ph_info and ph_info_ch structures,
	 * while the latter just sends an int containing the current L1_SIGNAL_* event id.
	 *
	 * The flags and state information in the ph_info and ph_info_ch structures
	 * are defined in kernel internal hw-specific headers (mISDNhw.h).
	 *
	 * Use the payload size to guess the type of message.
	 */
	if (data_len >= sizeof(struct ph_info)) {
		/* complete port status, hfcsusb sends this */
		struct ph_info *info = (struct ph_info *)data;
		struct ph_info_ch *bch_info = NULL;
		char tmp[1024] = { 0 };

		if (data_len < (sizeof(*info) + info->dch.num_bch * sizeof(*bch_info))) {
			ftdm_log_chan_msg(chan, FTDM_LOG_ERROR, "mISDN MPH_INFORMATION_IND message is too short\n");
			return FTDM_FAIL;
		}
		bch_info = &info->bch[0];

		ftdm_log_chan(chan, FTDM_LOG_DEBUG, "mISDN port state:\n\tD-Chan proto:\t%hu\n\tD-Chan state:\t%s (%hu)\n\tD-Chan flags:\t%#"FTDM_XINT64_FMT"\n\t\t\t%-70s\n",
			info->dch.ch.protocol,
			misdn_hw_state_name(info->dch.ch.protocol, info->dch.state), info->dch.state,
			info->dch.ch.Flags,
			misdn_hw_print_flags(info->dch.ch.Flags, tmp, sizeof(tmp) - 1));

		/* TODO: try to translate this to a usable set of alarm flags */

	} else if (data_len == sizeof(int)) {
		/* alarm info, sent by hfcmulti */
		int value = *(int *)data;
		int alarm_flags = chan->alarm_flags;

		if (data_len < sizeof(value)) {
			ftdm_log_chan_msg(chan, FTDM_LOG_ERROR, "mISDN MPH_INFORMATION_IND message is too short\n");
			return FTDM_FAIL;
		}

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
			ftdm_log_chan(chan, FTDM_LOG_ERROR, "mISDN unknown MPH_INFORMATION_IND signal: %#04x\n",
				value);
			return FTDM_FAIL;
		}

		/* check whether alarm status has changed, update channel flags if it has */
		if ((value = (alarm_flags ^ chan->alarm_flags))) {
			ftdm_log_chan(chan, FTDM_LOG_DEBUG, "mISDN alarm flags have changed %#x -> %#x\n",
				chan->alarm_flags, alarm_flags);
			chan->alarm_flags ^= value;
		}
	} else {
		ftdm_log_chan(chan, FTDM_LOG_ERROR, "mISDN sent MPH_INFORMATION_IND message with unknown size %d\n",
			data_len);
		return FTDM_FAIL;
	}

	return FTDM_SUCCESS;
}

/***********************************************************************************
 * mISDN <-> FreeTDM interface functions
 ***********************************************************************************/

static struct misdn_globals {
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

	if (chan_priv->active) {
		ftdm_log_chan_msg(ftdmchan, FTDM_LOG_INFO, "mISDN channel is already open, skipping activation\n");
		return FTDM_SUCCESS;
	}

	/* flush all events */
	misdn_event_queue_reset(chan_priv->events);

	/*
	 * Send activation request
	 */
	ret = misdn_activate_channel(ftdmchan);
	if (ret != FTDM_SUCCESS) {
		ftdm_log_chan(ftdmchan, FTDM_LOG_ERROR, "Failed to activate channel (socket: %d)\n",
			ftdmchan->sockfd);
		/*
		 * Ignore error, ftdm_channel_open() does not correctly handle return FTDM_FAIL cases.
		 * We will try to activate the channel later.
		 */
	} else {
		ftdm_log_chan_msg(ftdmchan, FTDM_LOG_DEBUG, "mISDN channel activation request sent\n");

		switch (ftdmchan->type) {
		case FTDM_CHAN_TYPE_B:
		case FTDM_CHAN_TYPE_DQ921:
			chan_priv->active = 1;
			break;
		default:
			ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "mISDN invalid channel type '%s'\n",
				ftdm_channel_get_type_str(ftdmchan));
			break;
		}
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

	ftdm_log_chan(ftdmchan, FTDM_LOG_NOTICE, "mISDN trying to close %c-channel\n",
		ftdm_channel_get_type(ftdmchan) == FTDM_CHAN_TYPE_B ? 'B' : 'D');

	if (chan_priv->active) {

		if (ftdm_channel_get_type(ftdmchan) == FTDM_CHAN_TYPE_B) {
			ret = misdn_deactivate_channel(ftdmchan);
		} else {
			/* Don't wait for D-Channel deactivation */
			ret = misdn_deactivate_channel_nowait(ftdmchan);
		}

		if (ret != FTDM_SUCCESS) {
			ftdm_log_chan(ftdmchan, FTDM_LOG_ERROR, "Failed to deactivate %c-channel\n",
				ftdm_channel_get_type(ftdmchan) == FTDM_CHAN_TYPE_B ? 'B' : 'D');
			/* Ignore error, channel might be closed already */
		} else {
			ftdm_log_chan(ftdmchan, FTDM_LOG_INFO, "mISDN %c-channel deactivated\n",
				ftdm_channel_get_type(ftdmchan) == FTDM_CHAN_TYPE_B ? 'B' : 'D');
		}

		ftdm_log_chan(ftdmchan, FTDM_LOG_NOTICE, "mISDN tx stats: wr: %lu, rd: %lu, tx: %lu, tx-lost: %lu, tx-miss: %lu, tx-under#: %lu, tx-over#: %lu\n",
			chan_priv->tx_pipe_wr_bytes, chan_priv->tx_pipe_rd_bytes,
			chan_priv->tx_sent_bytes, chan_priv->tx_lost_bytes, chan_priv->tx_miss_bytes,
			chan_priv->tx_pipe_over_cnt, chan_priv->tx_pipe_under_cnt);

		chan_priv->active = 0;
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
		ftdm_log_chan(ftdmchan, FTDM_LOG_NOTICE, "Interval %d ms\n",
			ftdm_channel_get_io_interval(ftdmchan));
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
			pfds[nr_fds].fd = chan_priv->tx_audio_pipe_in;
			pfds[nr_fds].events = POLLOUT;
			nr_fds++;
		}
		if (*flags & FTDM_READ) {
			pfds[nr_fds].fd = chan_priv->rx_audio_pipe_out;
			pfds[nr_fds].events = POLLIN;
			nr_fds++;
		}
/*		if (*flags & (FTDM_READ | FTDM_EVENTS)) {
			pfds[nr_fds].fd = ftdmchan->sockfd;
			pfds[nr_fds].events |= (*flags & FTDM_READ)   ? POLLIN  : 0;
			pfds[nr_fds].events |= (*flags & FTDM_EVENTS) ? POLLPRI : 0;
			nr_fds++;
		}
*/
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

	if (!(pfds[0].events || pfds[1].events)) {
		ftdm_log_chan_msg(ftdmchan, FTDM_LOG_NOTICE, "mISDN poll(): no flags set!\n");
		return FTDM_SUCCESS;
	}

	if ((retval = poll(pfds, nr_fds, to)) < 0) {
		ftdm_log_chan(ftdmchan, FTDM_LOG_ERROR, "mISDN poll() failed: %s\n",
			strerror(errno));
		return FTDM_FAIL;
	}
	if (retval == 0)
		return FTDM_TIMEOUT;

	switch (ftdm_channel_get_type(ftdmchan)) {
	case FTDM_CHAN_TYPE_B:
		if ((pfds[0].revents & POLLOUT) || (pfds[1].revents & POLLOUT))
			*flags |= FTDM_WRITE;
		if ((pfds[0].revents & POLLIN)  || (pfds[1].revents & POLLIN))
			*flags |= FTDM_READ;
		if ((pfds[0].revents & POLLPRI) || (pfds[1].revents & POLLPRI))
			*flags |= FTDM_EVENTS;
		break;
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
 * Handle incoming mISDN message on d-channel
 * \param[in]	ftdmchan
 * \param[in]	msg_buf
 * \param[in]	msg_len
 * \internal
 */
static ftdm_status_t misdn_handle_incoming(ftdm_channel_t *ftdmchan, const char *msg_buf, const int msg_len)
{
	struct misdn_chan_private *priv = ftdm_chan_io_private(ftdmchan);
	struct mISDNhead *hh = (struct mISDNhead *)msg_buf;
	const char *data = msg_buf + sizeof(*hh);
	int data_len = msg_len - sizeof(*hh);

	assert(msg_buf);
	assert(priv);

	if (msg_len < sizeof(*hh)) {
		ftdm_log_chan(ftdmchan, FTDM_LOG_ERROR, "mISDN message to small (%d < %"FTDM_SIZE_FMT" bytes)\n",
			msg_len, sizeof(*hh));
		return FTDM_FAIL;
	}

#ifdef MISDN_DEBUG_EVENTS
	ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "mISDN %c-channel received '%s' message (id: 0x%x, additional data: %d bytes)\n",
		ftdm_channel_get_type(ftdmchan) == FTDM_CHAN_TYPE_B ? 'B' : 'D', misdn_event2str(hh->prim), hh->id, data_len);
#endif

	switch (hh->prim) {
	/* data events */
	case PH_DATA_CNF:	/* TX: ack */
		priv->tx_ack_cnt++;
		break;
	case PH_DATA_REQ:	/* TX: request echo (ignore) */
		break;
	case PH_DATA_E_IND:	/* RX: e-channel data received (monitoring?) */
		break;

	/* control requests */
	case PH_CONTROL_IND:
		return misdn_handle_ph_control_ind(ftdmchan, hh, data, data_len);
	case PH_CONTROL_REQ:
	case PH_CONTROL_CNF:
		break;

	/* information */
	case MPH_INFORMATION_IND:
		return misdn_handle_mph_information_ind(ftdmchan, hh, data, data_len);

	/* channel de-/activation */
	case PH_ACTIVATE_REQ:	/* Echoed requests, ignore */
	case PH_DEACTIVATE_REQ:
		break;
	case PH_ACTIVATE_IND:
	case PH_DEACTIVATE_IND: {
		/* other events, enqueue and let misdn_event_next handle it */
		struct misdn_span_private *span_priv = ftdm_span_io_private(ftdmchan->span);
		struct misdn_event evt = { 0 };
		evt.id = hh->prim;

		misdn_event_queue_push(priv->events, &evt);

		/* wake possible readers */
		pthread_cond_signal(&span_priv->event_cond);
		break;
	}
	default:	/* error? */
		ftdm_log(FTDM_LOG_DEBUG, "mISDN channel %d:%d received unknown event %d\n",
			ftdm_channel_get_span_id(ftdmchan), ftdm_channel_get_id(ftdmchan), hh->prim);
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
	int maxretry = 10;

	/* nothing read yet */
	*datalen = 0;

	/*
	 * try to read all messages, as long as we haven't received a PH_DATA_IND one
	 * we'll get a lot of "mISDN_send: error -12" message in dmesg otherwise
	 * (= b-channel receive queue overflowing)
	 */
	switch (ftdm_channel_get_type(ftdmchan)) {
	case FTDM_CHAN_TYPE_DQ921: {
		while (maxretry--) {
			struct sockaddr_mISDN addr;
			socklen_t addrlen = sizeof(addr);

			if ((retval = recvfrom(ftdmchan->sockfd, rbuf, sizeof(rbuf), 0, (struct sockaddr *)&addr, &addrlen)) < 0) {
				if (errno == EWOULDBLOCK || errno == EAGAIN) break;
				ftdm_log_chan(ftdmchan, FTDM_LOG_ERROR, "mISDN failed to receive incoming message: %s\n",
					strerror(errno));
				return FTDM_FAIL;
			}

			if (retval < MISDN_HEADER_LEN) {
				ftdm_log_chan_msg(ftdmchan, FTDM_LOG_ERROR, "mISDN received message too small\n");
				return FTDM_FAIL;
			}

			if (hh->prim == PH_DATA_IND) {
				*datalen = ftdm_clamp(retval - MISDN_HEADER_LEN, 0, bytes);

				if (*datalen <= 0)
					continue;

				/*
				 * Copy data into ouput buffer (excluding the mISDN message header)
				 * NOTE: audio data needs to be converted to a-law / u-law!
				 */
				memcpy(data, rbuf + MISDN_HEADER_LEN, *datalen);
				return FTDM_SUCCESS;
			} else {
				*datalen = 0;
				/* event */
				misdn_handle_incoming(ftdmchan, rbuf, retval);
			}
		}
		break;
	}
	case FTDM_CHAN_TYPE_B: {
		if (!priv->active) {
			ftdm_log_chan_msg(ftdmchan, FTDM_LOG_DEBUG, "mISDN ignoring read on closed b-channel\n");
			return FTDM_SUCCESS;
		}

		if ((retval = read(priv->rx_audio_pipe_out, data, bytes)) < 0) {
			ftdm_log_chan(ftdmchan, FTDM_LOG_ERROR, "mISDN failed to read audio data from rx pipe: %s\n",
				strerror(errno));
			return FTDM_FAIL;
		}
		*datalen = retval;
		break;
	}
	default:
		break;
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
	ftdm_wait_flag_t wflags;

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
	*datalen = 0;

	switch (ftdm_channel_get_type(ftdmchan)) {
	case FTDM_CHAN_TYPE_B:
		/*
		 * Check state, send activation request (async) if channel is not open
		 */
		if (!priv->active) {
			misdn_activate_channel_nowait(ftdmchan);
			return FTDM_SUCCESS;	/* eat data */
		}
		/*
		 * Write to audio pipe, misdn_read() will pull
		 * from there as needed and send it to the b-channel
		 *
		 * NOTE: can't use blocking I/O here since both parts are serviced
		 *       from the same thread
		 */
		if ((retval = write(priv->tx_audio_pipe_in, data, size)) < 0) {
			ftdm_log_chan(ftdmchan, FTDM_LOG_ERROR, "mISDN channel audio pipe write error [wr: %lu, rd: %lu: tx: %lu, tx-under#: %lu, tx-over#: %lu]: %s\n",
				priv->tx_pipe_wr_bytes, priv->tx_pipe_rd_bytes, priv->tx_sent_bytes,
				priv->tx_pipe_under_cnt, priv->tx_pipe_over_cnt, strerror(errno));
			return FTDM_FAIL;
		} else if (retval < size) {
			priv->tx_pipe_over_cnt++;
			ftdm_log_chan(ftdmchan, FTDM_LOG_WARNING, "mISDN channel audio pipe short write [wr: %lu, rd: %lu: tx: %lu, tx-under#: %lu, tx-over#: %lu], expected: %d, written: %d\n",
				priv->tx_pipe_wr_bytes, priv->tx_pipe_rd_bytes, priv->tx_sent_bytes,
				priv->tx_pipe_under_cnt, priv->tx_pipe_over_cnt, size, retval);
		}
		ACCESS_ONCE(priv->tx_pipe_wr_bytes) += retval;
		*datalen = retval;
		break;
	default:
		hh->prim = PH_DATA_REQ;
		hh->id   = MISDN_ID_ANY;

		/* Avoid buffer overflow */
		size = ftdm_min(size, MAX_DATA_MEM - MISDN_HEADER_LEN);

		memcpy(wbuf + MISDN_HEADER_LEN, data, size);
		size += MISDN_HEADER_LEN;

		/*
		 * Check state, send activation request (sync) if channel is not open
		 */
		if (!priv->active) {
			retval = misdn_activate_channel(ftdmchan);
			if (retval) {
				*datalen = 0;
				return FTDM_FAIL;
			}
		}

		/* wait for channel to get ready */
		wflags = FTDM_WRITE;
		retval = misdn_wait(ftdmchan, &wflags, 20);
		if (retval) {
			/* timeout, io error */
			*datalen = 0;
			return FTDM_FAIL;
		}

#ifdef MISDN_DEBUG_IO
		ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "mISDN writing %d bytes to channel socket %d [dev.ch: %d.%d]\n",
			size, ftdmchan->sockfd, priv->addr.dev, priv->addr.channel);
#endif

		if ((retval = sendto(ftdmchan->sockfd, wbuf, size, 0, NULL, 0)) < size) {
			ftdm_log_chan(ftdmchan, FTDM_LOG_ERROR, "mISDN channel socket write error: %s\n",
				strerror(errno));
			return FTDM_FAIL;
		}
		*datalen = retval;
		break;
	}

	priv->tx_cnt++;
	return FTDM_SUCCESS;
}

/**
 * Carefully choosen size for socket send/recv buffers
 * larger values will add more latency, while lower values will cause deadlocks
 * (see misdn_span_run() comments below for an explanation)
 */
#define SOCKETPAIR_BUFFER_SIZE	3072

static ftdm_status_t misdn_open_range(ftdm_span_t *span, ftdm_chan_type_t type, struct mISDN_devinfo *devinfo, int start, int end)
{
	int num_configured = 0;
	int d_protocol, d_channel;
	int x;

	ftdm_log(FTDM_LOG_DEBUG, "mISDN configuring card:range %d:%d->%d\n",
		devinfo->id, start, end);

	switch (ftdm_span_get_trunk_type(span)) {
	case FTDM_TRUNK_E1:
		d_protocol = ftdm_span_get_trunk_mode(span) == FTDM_TRUNK_MODE_NET ? ISDN_P_NT_E1 : ISDN_P_TE_E1;
		d_channel  = 16;
		break;
	case FTDM_TRUNK_BRI:
	case FTDM_TRUNK_BRI_PTMP:
		d_protocol = ftdm_span_get_trunk_mode(span) == FTDM_TRUNK_MODE_NET ? ISDN_P_NT_S0 : ISDN_P_TE_S0;
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

		/* Set mISDN channel socket non-blocking */
		if (fcntl(sockfd, F_SETFL, O_NONBLOCK) < 0) {
			ftdm_log(FTDM_LOG_ERROR, "mISDN Failed to set socket fd to non-blocking: %s\n",
				strerror(errno));
			close(sockfd);
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

		/*
		 * Create event queue
		 */
		misdn_event_queue_create(&priv->events);

		ftdmchan->rate = 8000;
		ftdmchan->physical_span_id = devinfo->id;
		ftdmchan->physical_chan_id = x;

		if (ftdmchan->type == FTDM_CHAN_TYPE_B) {
			int pipefd[2] = { -1, -1 };

			ftdmchan->packet_len         = 30 /* ms */ * (ftdmchan->rate / 1000);
			ftdmchan->effective_interval = ftdmchan->native_interval = ftdmchan->packet_len / 8;
			ftdmchan->native_codec       = ftdmchan->effective_codec = FTDM_CODEC_ALAW;

//			ftdm_channel_set_feature(ftdmchan, FTDM_CHANNEL_FEATURE_INTERVAL);

#ifdef USE_PIPE
			/*
			 * Create audio tx pipe, use non-blocking I/O to avoid deadlock since both ends
			 * are used from the same thread
			 */
			if (pipe2(pipefd, 0 | O_NONBLOCK) < 0) {
				ftdm_log(FTDM_LOG_ERROR, "Failed to create mISDN audio tx pipe [%d:%d]: %s\n",
					addr.dev, x, strerror(errno));
				close(sockfd);
				return FTDM_FAIL;
			}
			priv->tx_audio_pipe_in  = pipefd[1];
			priv->tx_audio_pipe_out = pipefd[0];

#if 1 || defined(HAVE_F_SETPIPE_SZ)
			if (fcntl(priv->tx_audio_pipe_in, F_SETPIPE_SZ, 4096) < 0) {
				ftdm_log(FTDM_LOG_WARNING, "Failed to set mISDN audio tx pipe size [%d:%d]: %s\n",
					addr.dev, x, strerror(errno));
			}
#endif
			/*
			 * Create audio rx pipe, use non-blocking I/O to avoid deadlock since both ends
			 * are used from the same thread
			 */
			if (pipe2(pipefd, 0 | O_NONBLOCK) < 0) {
				ftdm_log(FTDM_LOG_ERROR, "Failed to create mISDN audio rx pipe [%d:%d]: %s\n",
					addr.dev, x, strerror(errno));
				close(sockfd);
				return FTDM_FAIL;
			}
			priv->rx_audio_pipe_in  = pipefd[1];
			priv->rx_audio_pipe_out = pipefd[0];

#if 1 || defined(HAVE_F_SETPIPE_SZ)
			if (fcntl(priv->rx_audio_pipe_in, F_SETPIPE_SZ, 4096) < 0) {
				ftdm_log(FTDM_LOG_WARNING, "Failed to set mISDN audio rx pipe size [%d:%d]: %s\n",
					addr.dev, x, strerror(errno));
			}
#endif
#else	/* !USE_PIPE */
			/*
			 * Use a socket pair for audio rx/tx, allows for more fine-grained control
			 * of latency (= amounts of data in buffers)
			 */
			if (socketpair(AF_UNIX, SOCK_STREAM, 0, pipefd) < 0) {
				ftdm_log(FTDM_LOG_ERROR, "Failed to create mISDN audio socket pair [%d:%d]: %s\n",
					addr.dev, x, strerror(errno));
				close(sockfd);
				return FTDM_FAIL;
			} else {
				int opt = SOCKETPAIR_BUFFER_SIZE;
				socklen_t optlen = sizeof(opt);

				if (fcntl(pipefd[0], F_SETFL, O_NONBLOCK) < 0) {
					ftdm_log(FTDM_LOG_ERROR, "mISDN Failed to set socket pair fd[0] to non-blocking: %s\n",
						strerror(errno));
					close(sockfd);
					close(pipefd[0]);
					close(pipefd[1]);
					return FTDM_FAIL;
				}
				if (fcntl(pipefd[1], F_SETFL, O_NONBLOCK) < 0) {
					ftdm_log(FTDM_LOG_ERROR, "mISDN Failed to set socket pair fd[1] to non-blocking: %s\n",
					strerror(errno));
					close(sockfd);
					close(pipefd[0]);
					close(pipefd[1]);
					return FTDM_FAIL;
				}

				/*
				 * Set RX/TX buffer sizes on each end of the socket pair
				 */
				if (setsockopt(pipefd[0], SOL_SOCKET, SO_RCVBUF, &opt, optlen) < 0) {
					ftdm_log(FTDM_LOG_WARNING, "mISDN Failed to set socket pair fd[0] RCVBUF: %s\n",
						strerror(errno));
				}
				if (setsockopt(pipefd[0], SOL_SOCKET, SO_SNDBUF, &opt, optlen) < 0) {
					ftdm_log(FTDM_LOG_WARNING, "mISDN Failed to set socket pair fd[0] SNDBUF: %s\n",
						strerror(errno));
				}
				if (setsockopt(pipefd[1], SOL_SOCKET, SO_RCVBUF, &opt, optlen) < 0) {
					ftdm_log(FTDM_LOG_WARNING, "mISDN Failed to set socket pair fd[1] RCVBUF: %s\n",
						strerror(errno));
				}
				if (setsockopt(pipefd[1], SOL_SOCKET, SO_SNDBUF, &opt, optlen) < 0) {
					ftdm_log(FTDM_LOG_WARNING, "mISDN Failed to set socket pair fd[1] SNDBUF: %s\n",
						strerror(errno));
				}

				priv->rx_audio_pipe_in  = pipefd[1];
				priv->rx_audio_pipe_out = pipefd[0];

				priv->tx_audio_pipe_in  = pipefd[0];
				priv->tx_audio_pipe_out = pipefd[1];
			}
#endif
		} else {
			/* early activate D-Channel */
			misdn_activate_channel(ftdmchan);
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
		int pipe[2] = { -1, -1 };

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

		/* init control condition */
		pthread_cond_init(&span_priv->ctrl_cond, NULL);
		pthread_mutex_init(&span_priv->ctrl_cond_mutex, NULL);

		/* create event pipe */
		if (pipe2(pipe, O_CLOEXEC) < 0) {
			ftdm_log(FTDM_LOG_ERROR, "mISDN failed to create event pipe: %s\n",
				strerror(errno));
			return FTDM_FAIL;
		}
		span_priv->event_pipe_in  = pipe[0];
		span_priv->event_pipe_out = pipe[1];
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

	for (i = 1; i <= ftdm_span_get_chan_count(span); i++) {
		ftdm_channel_t *chan = ftdm_span_get_channel(span, i);
		struct misdn_chan_private *chan_priv = ftdm_chan_io_private(chan);

		/* Skip channels that have event processing pending (Avoids event storms) */
		if (ftdm_test_io_flag(chan, FTDM_CHANNEL_IO_EVENT))
			continue;

		if (misdn_event_queue_has_data(chan_priv->events)) {
#ifdef MISDN_DEBUG_EVENTS
			ftdm_log(FTDM_LOG_DEBUG, "mISDN channel %d:%d has event(s)\n",
				ftdm_channel_get_span_id(chan), ftdm_channel_get_id(chan));
#endif
			ftdm_set_io_flag(chan, FTDM_CHANNEL_IO_EVENT);
			chan->last_event_time = ftdm_current_time_in_ms();
			nr_events++;
		}
	}
	if (nr_events) {
#ifdef MISDN_DEBUG_EVENTS
		ftdm_log(FTDM_LOG_DEBUG, "mISDN span %d has %d new events pending (pre poll)\n",
			ftdm_span_get_id(span), nr_events);
#endif
		return FTDM_SUCCESS;
	}


#ifdef MISDN_DEBUG_EVENTS
	ftdm_log(FTDM_LOG_DEBUG, "mISDN span %d has no events pending, polling for new events with %d ms timeout\n",
		ftdm_span_get_id(span), ms);
#endif
	/* Wait at least 1 ms, max 1 s */
	ms = ftdm_clamp(ms, 1, 1000);

	clock_gettime(CLOCK_REALTIME, &ts);
	ts_add_msec(&ts, ms);

	if ((retval = pthread_cond_timedwait(&span_priv->event_cond, &span_priv->event_cond_mutex, &ts))) {
		switch (retval) {
		case ETIMEDOUT:
#ifdef MISDN_DEBUG_EVENTS
			ftdm_log(FTDM_LOG_DEBUG, "mISDN span %d: No events within %d ms\n",
				ftdm_span_get_id(span), ms);
#endif
			return FTDM_TIMEOUT;
		default:
			ftdm_log(FTDM_LOG_DEBUG, "mISDN failed to poll for events on span %d: %s\n",
				ftdm_span_get_id(span), strerror(retval));
			return FTDM_FAIL;
		}
	}

#ifdef MISDN_DEBUG_EVENTS
	ftdm_log(FTDM_LOG_DEBUG, "mISDN span %d received new event notification, checking channel event queues\n",
		ftdm_span_get_id(span));
#endif
	for (i = 1; i <= ftdm_span_get_chan_count(span); i++) {
		ftdm_channel_t *chan = ftdm_span_get_channel(span, i);
		struct misdn_chan_private *chan_priv = ftdm_chan_io_private(chan);

		/* Skip channels that have event processing pending (Avoids event storms) */
		if (ftdm_test_io_flag(chan, FTDM_CHANNEL_IO_EVENT))
			continue;

		if (misdn_event_queue_has_data(chan_priv->events)) {
			ftdm_set_io_flag(chan, FTDM_CHANNEL_IO_EVENT);
			chan->last_event_time = ftdm_current_time_in_ms();
			nr_events++;
		}
	}
	return (nr_events) ? FTDM_SUCCESS : FTDM_TIMEOUT;	/* no events? => timeout */
}

/**
 * Retrieve event from channel
 * \param	ftdmchan	Channel to retrieve event from
 * \param	event		FreeTDM event to return
 * \return Success or failure
 */
static FIO_CHANNEL_NEXT_EVENT_FUNCTION(misdn_channel_next_event)
{
	struct misdn_chan_private *chan_priv = ftdm_chan_io_private(ftdmchan);
	struct misdn_event *evt = NULL;
	ftdm_span_t *span = ftdm_channel_get_span(ftdmchan);
	uint32_t event_id = FTDM_OOB_INVALID;

	ftdm_assert(span, "span == NULL");

	ftdm_clear_io_flag(ftdmchan, FTDM_CHANNEL_IO_EVENT);

	if (!(evt = misdn_event_queue_pop(chan_priv->events))) {
#ifdef MISDN_DEBUG_EVENTS
		ftdm_log_chan_msg(ftdmchan, FTDM_LOG_DEBUG, "mISDN channel event queue has no events\n");
#endif
		return FTDM_FAIL;
	}

#ifdef MISDN_DEBUG_EVENTS
	ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Got event '%s' from channel event queue\n",
		misdn_event2str(evt->id));
#endif
	/* Convert from misdn event to ftdm */
	switch (evt->id) {
	case PH_DEACTIVATE_IND:
		event_id = FTDM_OOB_ALARM_TRAP;
		ftdmchan->alarm_flags |= FTDM_ALARM_RED;
		break;
	case PH_ACTIVATE_IND:
		event_id = FTDM_OOB_ALARM_CLEAR;
		ftdmchan->alarm_flags &= ~FTDM_ALARM_RED;
		break;
	default:
		ftdm_log_chan(ftdmchan, FTDM_LOG_ERROR, "Unhandled event id %d (0x%x) %s\n",
			evt->id, evt->id, misdn_event2str(evt->id));
	}

	ftdmchan->last_event_time = 0;
	span->event_header.e_type = FTDM_EVENT_OOB;
	span->event_header.enum_id = event_id;
	span->event_header.channel = ftdmchan;
	*event = &span->event_header;
	return FTDM_SUCCESS;
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

		ftdm_clear_io_flag(chan, FTDM_CHANNEL_IO_EVENT);

		if (!(evt = misdn_event_queue_pop(chan_priv->events))) {
#ifdef MISDN_DEBUG_EVENTS
			ftdm_log_chan_msg(chan, FTDM_LOG_DEBUG, "mISDN channel event queue has no events\n");
#endif
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

	/* free resources */
	ftdm_span_io_private(span) = NULL;
	ftdm_safe_free(span_priv);

	ftdm_log(FTDM_LOG_DEBUG, "mISDN span %d (%s) destroyed\n",
		ftdm_span_get_id(span), ftdm_span_get_name(span));
	return FTDM_SUCCESS;
}


/**
 * Called by misdn_span_run() to handle incoming b-channel events
 * \param[in]	chan	FreeTDM channel object
 * \return	FTDM_SUCCESS on success, FTDM_* on error
 */
static ftdm_status_t handle_b_channel_event(ftdm_channel_t *chan)
{
	struct misdn_chan_private *priv = ftdm_chan_io_private(chan);
	char buf[MAX_DATA_MEM] = { 0 };
	struct mISDNhead *mh = (void *)buf;
	int retval;

	if ((retval = recvfrom(chan->sockfd, buf, sizeof(buf), 0, NULL, NULL)) <= 0) {
		ftdm_log_chan(chan, FTDM_LOG_ERROR, "mISDN failed to receive message: %s\n",
			strerror(errno));
		return FTDM_FAIL;
	}

	if (retval < MISDN_HEADER_LEN) {
		ftdm_log_chan(chan, FTDM_LOG_ERROR, "mISDN message too short, min.: %d, read: %d\n",
			(int)MISDN_HEADER_LEN, retval);
		return FTDM_FAIL;
	}

	switch (mh->prim) {
	case PH_DATA_IND: {
		int datalen = retval - MISDN_HEADER_LEN;
		char *data  = buf    + MISDN_HEADER_LEN;

		/* Discard incoming audio if not active */
		if (!priv->active) {
			/* Convert audio data */
			misdn_convert_audio_bits(data, datalen);

			/* Write audio into receive pipe */
			if ((retval = write(priv->rx_audio_pipe_in, data, datalen)) < 0) {
				ftdm_log_chan(chan, FTDM_LOG_ERROR, "mISDN failed to write audio data into rx pipe: %s\n",
					strerror(errno));
				return FTDM_FAIL;
			} else if (retval < datalen) {
				ftdm_log_chan(chan, FTDM_LOG_ERROR, "mISDN short write into rx pipe, written: %d, expected: %d\n",
					retval, datalen);
				return FTDM_FAIL;
			}
		}

		/* Get receive buffer usage */
		if (ioctl(priv->tx_audio_pipe_out, FIONREAD, &retval) < 0) {
			ftdm_log_chan(chan, FTDM_LOG_ERROR, "mISDN failed to get tx audio buffer usage: %s\n",
				strerror(errno));
			return FTDM_FAIL;
		} else if (retval < datalen) {
//			 ftdm_log_chan(chan, FTDM_LOG_DEBUG, "mISDN has not enough bytes in tx audio pipe, available: %d, requested: %d\n",
//				retval, datalen);
			priv->tx_pipe_under_cnt++;
			return FTDM_SUCCESS;
		}

#ifdef MISDN_DEBUG_IO
		ftdm_log_chan(chan, FTDM_LOG_INFO, "mISDN tx audio buffer usage: %d\n",
			retval);
#endif

		/* Get audio from tx pipe */
		if ((retval = read(priv->tx_audio_pipe_out, data, datalen)) < 0) {
			ftdm_log_chan(chan, FTDM_LOG_ERROR, "mISDN failed to read audio data from tx pipe: %s\n",
				strerror(errno));
			return FTDM_FAIL;
		} else if (retval == 0) {
			ftdm_log_chan_msg(chan, FTDM_LOG_NOTICE, "mISDN tx pipe is empty\n");
			priv->tx_pipe_under_cnt++;
			return FTDM_SUCCESS;
		} else if (retval < datalen) {
			ftdm_log_chan(chan, FTDM_LOG_NOTICE, "mISDN short read from tx pipe, read: %d, expected: %d\n",
				retval, datalen);
			priv->tx_pipe_under_cnt++;
			priv->tx_miss_bytes += ftdm_max(0, datalen - retval);
			datalen = retval;
		}
		priv->tx_pipe_rd_bytes += retval;

		if (!priv->active) {
			/* discard */
			return FTDM_SUCCESS;
		}

		/* Convert audio data */
		misdn_convert_audio_bits(data, datalen);

		/* Write to channel */
		mh->prim = PH_DATA_REQ;
		mh->id   = 0;
		datalen += MISDN_HEADER_LEN;

		if ((retval = write(chan->sockfd, buf, datalen)) < 0) {
			ftdm_log_chan(chan, FTDM_LOG_ERROR, "mISDN failed to write audio data into b-channel: %s\n",
				strerror(errno));
			return FTDM_FAIL;
		} else if (retval < datalen) {
			ftdm_log_chan(chan, FTDM_LOG_WARNING, "mISDN short write into b-channel, written: %d, expected: %d\n",
				retval, datalen);
			priv->tx_lost_bytes += ftdm_max(0, datalen - retval - MISDN_HEADER_LEN);
		}
		priv->tx_sent_bytes += ftdm_max(0, retval - MISDN_HEADER_LEN);
		break;
	}
	case PH_DATA_CNF:
		priv->tx_ack_cnt++;
		break;
	case PH_DEACTIVATE_IND:
		priv->active = 0;
		break;
	case PH_ACTIVATE_IND:
		priv->active = 1;
		break;
	case PH_CONTROL_IND: {
		int datalen = retval - MISDN_HEADER_LEN;
		char *data  = buf    + MISDN_HEADER_LEN;

		misdn_handle_ph_control_ind(chan, mh, data, datalen);
		break;
	}
	default:
		ftdm_log_chan(chan, FTDM_LOG_ERROR, "mISDN received unknown/unhandled event primitive: (%d) %s\n",
			mh->prim, misdn_event2str(mh->prim));
		break;
	}
	return FTDM_SUCCESS;
}


/**
 * Timeout (miliseconds) for epoll_wait()
 */
#define MISDN_EPOLL_WAIT_MAX_MSEC	1000

/**
 * mISDN I/O thread
 * This thread handles all of the B-Channel I/O, this avoids all of the hazzles with
 * intermixed data + control frames on mISDN sockets and the missing write poll support on B-Channels.
 *
 * Each channel uses a unix stream socketpair as a two-way, pipe replacement for incoming and outgoing
 * data. Socketpairs allow a more fine grained tuning of the buffer sizes (pipe are restricted to multiples of
 * the native page size (with the smallest possible size (4k) being already 500ms worth of audio).
 *
 * The socketpair buffer sizes and the send algorithm have been carefully tuned to:
 *
 *    - Minimize the risk of sending too much data and making the mISDN drivers unhappy, by
 *      sending PH_DATA_REQ only when there is as much data available as we have received in
 *      the PH_DATA_IND.
 *
 *    - Avoid deadlocks between ftdm_write() trying to fill an almust full socket buffer and
 *      the I/O thread not having enough data to send a PH_DATA_REQ message.
 *      (The write() call will return EAGAIN since there is not ehough space free to send all audio data.)
 *
 * \param	thread	FreeTDM thread handle
 * \param	data	Private data pointer passed to ftdm_thread_create_detached() (the span object)
 * \return	Always returns NULL (unused)
 *
 * \note
 *	ftdm_span_start/_stop() locks the span mutex,
 *	use direct access to span members to avoid deadlocking
 *
 * \todo
 *	Move D-Channel handling into the I/O thread too.
 * 	Use custom ring buffer structures instead of socketpairs
 *	(for even more fine grained size control).
 */
static void *misdn_span_run(ftdm_thread_t *thread, void *data)
{
	ftdm_span_t *span = data;
	struct misdn_span_private *priv = ftdm_span_io_private(span);
	struct epoll_event evh;
	int epfd = -1;
	int ret;
	int i;

	ftdm_log(FTDM_LOG_NOTICE, "mISDN[%d:%s] span thread initializing\n",
		ftdm_span_get_id(span), ftdm_span_get_name(span));

	/* Use epoll for event handling */
	epfd = epoll_create(1);
	if (epfd < 0) {
		ftdm_log(FTDM_LOG_ERROR, "mISDN[%d:%s] failed to create epoll context: %s\n",
			ftdm_span_get_id(span), ftdm_span_get_name(span), strerror(errno));
		goto error;
	}

	ftdm_log(FTDM_LOG_DEBUG, "mISDN[%d:%s] adding event pipe to epoll context\n",
		ftdm_span_get_id(span), ftdm_span_get_name(span));

	/* Add event pipe */
	evh.events   = EPOLLIN | EPOLLPRI | EPOLLERR;
	evh.data.fd  = priv->event_pipe_out;

	ret = epoll_ctl(epfd, EPOLL_CTL_ADD, priv->event_pipe_out, &evh);
	if (ret < 0) {
		ftdm_log(FTDM_LOG_ERROR, "mISDN[%d:%s] failed to add event pipe to epoll context: %s\n",
			ftdm_span_get_id(span), ftdm_span_get_name(span), strerror(errno));
		goto error;
	}

	ftdm_log(FTDM_LOG_DEBUG, "mISDN[%d:%s] adding b-channels to epoll context\n",
		ftdm_span_get_id(span), ftdm_span_get_name(span));

	/* Add b-channels */
	for (i = 1; i <= span->chan_count; i++) {
		ftdm_channel_t *chan = span->channels[i];
		ftdm_assert(chan, "channel == NULL");

		if (ftdm_channel_get_type(chan) != FTDM_CHAN_TYPE_B)
			continue;

		ftdm_log(FTDM_LOG_DEBUG, "mISDN[%d:%s] adding b-channel [%d:%d] to epoll context\n",
			ftdm_span_get_id(span), ftdm_span_get_name(span),
			ftdm_channel_get_id(chan), ftdm_channel_get_ph_id(chan));

		evh.events   = EPOLLIN | EPOLLPRI | EPOLLERR;
		evh.data.ptr = chan;

		ret = epoll_ctl(epfd, EPOLL_CTL_ADD, chan->sockfd, &evh);
		if (ret < 0) {
			ftdm_log(FTDM_LOG_ERROR, "mISDN[%d:%s] failed to add b-channel [%d] socket to epoll context: %s\n",
				ftdm_span_get_id(span), ftdm_span_get_name(span), ftdm_channel_get_id(chan), strerror(errno));
			goto error;
		}
	}

	ftdm_log(FTDM_LOG_NOTICE, "mISDN[%d:%s] span thread started\n",
		ftdm_span_get_id(span), ftdm_span_get_name(span));

	/* Notify world we're running */
	priv->running = 1;
	pthread_cond_signal(&priv->ctrl_cond);

	while (priv->running > 0) {
		struct epoll_event ev[10];
		int timeout_ms = MISDN_EPOLL_WAIT_MAX_MSEC;

		ret = epoll_wait(epfd, ev, ftdm_array_len(ev), timeout_ms);
		if (ret < 0) {
			switch (errno) {
			case EAGAIN:
			case EINTR:
				continue;
			default:
				ftdm_log(FTDM_LOG_ERROR, "mISDN[%d:%s] epoll_wait() failed: %s\n",
					ftdm_span_get_id(span), ftdm_span_get_name(span), strerror(errno));
				goto error;
			}
		}
		/* Check events */
		for (i = 0; i < ret; i++) {
			/* */
			if (ev[i].data.fd == priv->event_pipe_out) {
				struct misdn_command cmd;
				/* event pipe */
				ftdm_log(FTDM_LOG_DEBUG, "mISDN[%d:%s] event pipe notification\n",
					ftdm_span_get_id(span), ftdm_span_get_name(span));
				ret = read(priv->event_pipe_out, &cmd, sizeof(cmd));
				if (ret < sizeof(cmd)) {
					ftdm_log(FTDM_LOG_ERROR, "mISDN[%d:%s] failed to read span thread command\n",
						ftdm_span_get_id(span), ftdm_span_get_name(span));
					continue;
				}

				switch (cmd.type) {
				case MISDN_CMD_STOP:
					ftdm_log(FTDM_LOG_ERROR, "mISDN[%d:%s] got STOP command\n",
						ftdm_span_get_id(span), ftdm_span_get_name(span));
					priv->running = -1;
					break;
				default:
					ftdm_log(FTDM_LOG_ERROR, "mISDN[%d:%s] got unknown command: %d\n",
						ftdm_span_get_id(span), ftdm_span_get_name(span), cmd.type);
				}

			} else {
				ftdm_channel_t *chan = ev[i].data.ptr;
				handle_b_channel_event(chan);
			}
		}
	}
error:
	ftdm_log(FTDM_LOG_NOTICE, "mISDN[%d:%s] span thread stopped\n",
		ftdm_span_get_id(span), ftdm_span_get_name(span));

	/* Remove epoll event sources */
	for (i = 1; i <= span->chan_count; i++) {
		ftdm_channel_t *chan = span->channels[i];
		ftdm_assert(chan, "channel == NULL");

		if (ftdm_channel_get_type(chan) != FTDM_CHAN_TYPE_B)
			continue;

		ret = epoll_ctl(epfd, EPOLL_CTL_DEL, chan->sockfd, NULL);
		if (ret < 0) {
			ftdm_log(FTDM_LOG_ERROR, "mISDN[%d:%s] failed to remove b-channel [%d] socket from epoll context: %s\n",
				ftdm_span_get_id(span), ftdm_span_get_name(span), ftdm_channel_get_id(chan), strerror(errno));
		}
	}

	/* Close epoll context */
	if (epfd >= 0) close(epfd);

	/* Notify world we stopped running */
	priv->running = 0;
	pthread_cond_signal(&priv->ctrl_cond);
	return NULL;
}

/**
 * Timeout (miliseconds) for span start/stop completion
 */
#define SPAN_DEFAULT_TIMEOUT_MSEC	10000

static FIO_SPAN_START_FUNCTION(misdn_span_start)
{
	struct misdn_span_private *span_priv = ftdm_span_io_private(span);
	struct timespec timeout;
	int retval;

	ftdm_log(FTDM_LOG_NOTICE, "mISDN starting span %d (%s)\n",
		ftdm_span_get_id(span), ftdm_span_get_name(span));

	span_priv->running = 0;

	if (ftdm_thread_create_detached(misdn_span_run, span) != FTDM_SUCCESS) {
		ftdm_log(FTDM_LOG_ERROR, "mISDN failed to start span %d (%s)\n",
			ftdm_span_get_id(span), ftdm_span_get_name(span));
		return FTDM_FAIL;
	}

	/*
	 * Wait SPAN_DEFAULT_TIMEOUT_MSEC miliseconds for I/O thread to start up
	 */
	clock_gettime(CLOCK_REALTIME, &timeout);
	ts_add_msec(&timeout, SPAN_DEFAULT_TIMEOUT_MSEC);

	pthread_mutex_lock(&span_priv->ctrl_cond_mutex);

	retval = pthread_cond_timedwait(&span_priv->ctrl_cond, &span_priv->ctrl_cond_mutex, &timeout);
	if (retval == ETIMEDOUT) {
		ftdm_log(FTDM_LOG_ERROR, "mISDN failed to start span %d (%s) in 10 seconds\n",
			ftdm_span_get_id(span), ftdm_span_get_name(span));
		return FTDM_FAIL;
	} else if (retval) {
		ftdm_log(FTDM_LOG_ERROR, "mISDN failed to start span %d (%s): %s\n",
			ftdm_span_get_id(span), ftdm_span_get_name(span), strerror(errno));
		return FTDM_FAIL;
	}

	pthread_mutex_unlock(&span_priv->ctrl_cond_mutex);
	return FTDM_SUCCESS;
}

static FIO_SPAN_STOP_FUNCTION(misdn_span_stop)
{
	struct misdn_span_private *span_priv = ftdm_span_io_private(span);
	struct timespec timeout;
	struct misdn_command cmd;
	int retval;

	ftdm_log(FTDM_LOG_NOTICE, "mISDN stopping span %d (%s)\n",
		ftdm_span_get_id(span), ftdm_span_get_name(span));

	span_priv->running = -1;

	/* Wake up thread */
	cmd.type = MISDN_CMD_STOP;
	retval = write(span_priv->event_pipe_in, &cmd, sizeof(cmd));
	if (retval < sizeof(cmd)) {
		ftdm_log(FTDM_LOG_WARNING, "mISDN failed to send STOP command to span thread\n");
	}

	/*
	 * Wait SPAN_DEFAULT_TIMEOUT_MSEC miliseconds for I/O thread to shut down
	 */
	clock_gettime(CLOCK_REALTIME, &timeout);
	ts_add_msec(&timeout, SPAN_DEFAULT_TIMEOUT_MSEC);

	pthread_mutex_lock(&span_priv->ctrl_cond_mutex);

	retval = pthread_cond_timedwait(&span_priv->ctrl_cond, &span_priv->ctrl_cond_mutex, &timeout);
	if (retval == ETIMEDOUT) {
		ftdm_log(FTDM_LOG_ERROR, "mISDN failed to stop thread in 10 seconds\n");
		return FTDM_FAIL;
	} else if (retval) {
		ftdm_log(FTDM_LOG_ERROR, "mISDN failed to stop thread: %s\n",
			strerror(errno));
		return FTDM_FAIL;
	}

	pthread_mutex_unlock(&span_priv->ctrl_cond_mutex);
	return FTDM_SUCCESS;
}


/**
 * \brief	ftmod_misdn interface
 */
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
	.channel_next_event = misdn_channel_next_event,
	.channel_destroy = misdn_channel_destroy,	/* clean up channel */
	.span_destroy    = misdn_span_destroy,		/* clean up span */

	.span_start      = misdn_span_start,
	.span_stop       = misdn_span_stop,
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
