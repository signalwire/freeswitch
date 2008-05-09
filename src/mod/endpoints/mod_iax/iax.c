 /*
 * libiax: An implementation of Inter-Asterisk eXchange
 *
 * Copyright (C) 2001, Linux Support Services, Inc.
 *
 * Mark Spencer <markster@linux-support.net>
 *
 * This program is free software, distributed under the terms of
 * the GNU Lesser (Library) General Public License
 */
 
#ifdef	WIN32
#undef __STRICT_ANSI__ //for strdup with ms

#include <string.h>
#include <process.h>
#include <windows.h>
#include <winsock.h>
#include <time.h>
#include <stdlib.h>
#include <malloc.h>
#include <stdarg.h>
#include <stdio.h>
#include <fcntl.h>
#include <io.h>
#include <errno.h>
#include <limits.h>


#define	snprintf _snprintf

#if defined(_MSC_VER)
#define	close		_close
#define inline __inline
#define strdup _strdup
#endif

void gettimeofday(struct timeval *tv, void /*struct timezone*/ *tz);
#include "winpoop.h"

#else

#define _BSD_SOURCE 1
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
       
#endif

#include "iax2.h"
#include "iax-client.h"
#include "md5.h"

#ifdef NEWJB
#include "jitterbuf.h"
#endif
#include "iax-mutex.h"
/*
	work around jitter-buffer shrinking in asterisk:
	channels/chan_iax2.c:schedule_delivery() shrinks jitter buffer by 2.
	this causes frames timestamped 1ms apart to ( sometimes ) be delivered 
	out of order, and results in garbled audio. our temporary fix is to increase
	the minimum number of ( timestamped ) milliseconds between frames to 3 ( 2 + 1 ).
*/
#define IAX_MIN_TIMESTAMP_INCREMENT 3

/* Define socket options for IAX2 sockets, based on platform
 * availability of flags */
#ifdef WIN32
 #define IAX_SOCKOPTS 0
#else
 #ifdef LINUX
 #define IAX_SOCKOPTS MSG_DONTWAIT | MSG_NOSIGNAL
 #else
 #define IAX_SOCKOPTS MSG_DONTWAIT
 #endif
#endif



#ifdef SNOM_HACK
/* The snom phone seems to improperly execute memset in some cases */
#include "../../snom_phonecore2/include/snom_memset.h"
#endif

/* Voice TS Prediction causes libiax2 to clean up the timestamps on
 * outgoing frames.  It works best with either continuous voice, or
 * callers who call iax_send_cng to indicate DTX for silence */
#define USE_VOICE_TS_PREDICTION

/* Define Voice Smoothing to try to make some judgements and adjust timestamps
   on incoming packets to what they "ought to be" */

#define VOICE_SMOOTHING
#undef VOICE_SMOOTHING

/* Define Drop Whole Frames to make IAX shrink its jitter buffer by dropping entire
   frames rather than simply delivering them faster.  Dropping encoded frames, 
   before they're decoded, usually leads to better results than dropping 
   decoded frames. */

#define DROP_WHOLE_FRAMES

#define MIN_RETRY_TIME 10
#define MAX_RETRY_TIME 4000
#define MEMORY_SIZE 1000

#define TRANSFER_NONE  0
#define TRANSFER_BEGIN 1
#define TRANSFER_READY 2
#define TRANSFER_REL   3

#ifndef NEWJB
/* No more than 4 seconds of jitter buffer */
static int max_jitterbuffer = 4000;
/* No more than 50 extra milliseconds of jitterbuffer than needed */
static int max_extra_jitterbuffer = 50;
/* To use or not to use the jitterbuffer */
static int iax_use_jitterbuffer = 1;

/* Dropcount (in per-MEMORY_SIZE) usually percent */
static int iax_dropcount = 3;
#endif

/* UDP Socket (file descriptor) */
static int netfd = -1;

/* Max timeouts */
static int maxretries = 10;

/* configurable jitterbuffer options */
static long jb_target_extra = -1; 

static int do_shutdown = 0;

/* external global networking replacements */
static sendto_t	  iax_sendto = (sendto_t) sendto;
static recvfrom_t iax_recvfrom = (recvfrom_t) recvfrom;

/* ping interval (seconds) */
static int ping_time = 10;
static void send_ping(void *session);

struct iax_session {
	/* Private data */
	void *pvt;
	/* session-local Sendto function */
	sendto_t sendto;
	/* Is voice quelched (e.g. hold) */
	int quelch;
	/* Codec Pref Order */
	char codec_order[32];
	/* Codec Pref Order Index*/
	int codec_order_len;
	/* Last received voice format */
	int voiceformat;
	/* Last transmitted voice format */
	int svoiceformat;
	/* Per session capability */
	int capability;
	/* Last received timestamp */
	time_in_ms_t last_ts;
	/* Last transmitted timestamp */
	time_in_ms_t lastsent;
	/* Last transmitted voice timestamp */
	unsigned int lastvoicets;
	/* Next predicted voice ts */
	time_in_ms_t nextpred;
	/* True if the last voice we transmitted was not silence/CNG */
	int notsilenttx;
	/* Our last measured ping time */
	time_in_ms_t pingtime;
	/* Address of peer */
	struct sockaddr_in peeraddr;
	/* Our call number */
	int callno;
	/* Peer's call number */
	int peercallno;
	/* Our next outgoing sequence number */
	unsigned char oseqno;
	/* Next sequence number they have not yet acknowledged */
	unsigned char rseqno;
	/* Our last received incoming sequence number */
	unsigned char iseqno;
	/* Last acknowledged sequence number */
	unsigned char aseqno;
	/* Peer supported formats */
	int peerformats;
	/* Time value that we base our transmission on */
	time_in_ms_t offset;
	/* Time value we base our delivery on */
	time_in_ms_t rxcore;
	/* History of lags */
	int history[MEMORY_SIZE];
	/* Current base jitterbuffer */
	int jitterbuffer;
	/* Informational jitter */
	int jitter;
	/* Measured lag */
	int lag;
	/* Current link state */
	int state;
	/* Peer name */
	char peer[MAXSTRLEN];
	/* Default Context */
	char context[MAXSTRLEN];
	/* Caller ID if available */
	char callerid[MAXSTRLEN];
	/* DNID */
	char dnid[MAXSTRLEN];
	/* Requested Extension */
	char exten[MAXSTRLEN];
	/* Expected Username */
	char username[MAXSTRLEN];
	/* Expected Secret */
	char secret[MAXSTRLEN];
	/* permitted authentication methods */
	char methods[MAXSTRLEN];
	/* MD5 challenge */
	char challenge[12];
#ifdef VOICE_SMOOTHING
	unsigned int lastts;
#endif
	/* Refresh if applicable */
	int refresh;

	/* ping scheduler id */
	int pingid;
	
	/* Transfer stuff */
	struct sockaddr_in transfer;
	int transferring;
	int transfercallno;
	int transferid;
	int transferpeer;	/* for attended transfer */
	int transfer_moh;	/* for music on hold while performing attended transfer */

#ifdef NEWJB
	jitterbuf *jb;
#endif
	struct iax_netstat remote_netstats;

	unsigned short samplemask;
	/* For linking if there are multiple connections */
	struct iax_session *next;
};

char iax_errstr[256];

static void destroy_session(struct iax_session *session);

#define IAXERROR snprintf(iax_errstr, sizeof(iax_errstr), 

#ifdef DEBUG_SUPPORT

#ifdef DEBUG_DEFAULT
static int debug = 1;
#else
static int debug = 0;
#endif

void iax_enable_debug(void)
{
	debug = 1;
}

void iax_disable_debug(void)
{
	debug = 0;
}

/* This is a little strange, but to debug you call DEBU(G "Hello World!\n"); */ 
#ifdef WIN32
#define G __FILE__, __LINE__, __FUNCTION__, 
#else
#define G __FILE__, __LINE__, (const char *)__func__, 
#endif

#define DEBU __debug 
static int __debug(char *file, int lineno, const char *func, char *fmt, ...) 
{
	va_list args;
	va_start(args, fmt);
	if (debug) {
		fprintf(stderr, "%s line %d in %s: ", file, lineno, func);
		vfprintf(stderr, fmt, args);
	}
	va_end(args);
	return 0;
}

#else /* No debug support */

#ifdef	WIN32
#define	DEBU
#else
#define DEBU(...) (void)(0)
#endif
#define G
#endif

typedef void (*sched_func)(void *);

struct iax_sched {
	/* These are scheduled things to be delivered */
	time_in_ms_t when;
	/* If event is non-NULL then we're delivering an event */
	struct iax_event *event;
	/* If frame is non-NULL then we're transmitting a frame */
	struct iax_frame *frame;
	/* If func is non-NULL then we should call it */
	sched_func func;
	/* and pass it this argument */
	void *arg;
	/* Easy linking */
	struct iax_sched *next;
};

static mutex_t *sched_mutex = NULL;
static mutex_t *session_mutex = NULL;
static struct iax_sched *schedq = NULL;
static struct iax_session *sessions = NULL;
static int callnums = 1;
static int transfer_id = 1;		/* for attended transfer */
static int time_init = 0;

static void init_time(void)
{
#ifdef WIN32_TIME_GET_TIME
	timeBeginPeriod(1);
#endif
	time_init = 1;
}

static void time_end(void)
{
#ifdef WIN32_TIME_GET_TIME
	timeEndPeriod(1);
#endif
	time_init = 0;
}

static time_in_ms_t current_time_in_ms(void)
{
#ifdef WIN32_TIME_GET_TIME
	return timeGetTime();
#else
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return ((tv.tv_sec * 1000) + (tv.tv_usec / 1000));
#endif
}

void iax_set_private(struct iax_session *s, void *ptr)
{
	s->pvt = ptr;
}

void *iax_get_private(struct iax_session *s)
{
	return s->pvt;
}

void iax_set_sendto(struct iax_session *s, sendto_t ptr)
{
	s->sendto = ptr;
}


unsigned int iax_session_get_capability(struct iax_session *s)
{
	return s->capability;
}


static int inaddrcmp(struct sockaddr_in *sin1, struct sockaddr_in *sin2)
{
	return (sin1->sin_addr.s_addr != sin2->sin_addr.s_addr) || (sin1->sin_port != sin2->sin_port);
}

static int iax_sched_add(struct iax_event *event, struct iax_frame *frame, sched_func func, void *arg, time_in_ms_t ms)
{

	/* Schedule event to be delivered to the client
	   in ms milliseconds from now, or a reliable frame to be retransmitted */
	struct iax_sched *sched, *cur, *prev = NULL;
	
	if (!event && !frame && !func) {
		DEBU(G "No event, no frame, no func?  what are we scheduling?\n");
		return -1;
	}
	

	sched = (struct iax_sched*)malloc(sizeof(struct iax_sched));
	if (sched) {
        memset(sched, 0, sizeof(struct iax_sched));
		sched->when = current_time_in_ms() + ms;
		sched->event = event;
		sched->frame = frame;
		sched->func = func;
		sched->arg = arg;
		/* Put it in the list, in order */
		iax_mutex_lock(sched_mutex);
		cur = schedq;
		while(cur && cur->when <= sched->when) {
			prev = cur;
			cur = cur->next;
		}
		sched->next = cur;
		if (prev) {
			prev->next = sched;
		} else {
			schedq = sched;
		}
		iax_mutex_unlock(sched_mutex);
		return 0;
	} else {
		DEBU(G "Out of memory!\n");
		return -1;
	}
}

static int iax_sched_del(struct iax_event *event, struct iax_frame *frame, sched_func func, void *arg, int all)
{
	struct iax_sched *cur, *tmp, *prev = NULL;
	int ret = 0;

	iax_mutex_lock(sched_mutex);
	cur = schedq;
	while (cur) {
		if (cur->event == event && cur->frame == frame && cur->func == func && cur->arg == arg) {
			if (prev)
				prev->next = cur->next;
			else
				schedq = cur->next;
			tmp = cur;
			cur = cur->next;	
			free(tmp);
			if (!all) {
				ret = -1;
				goto done;
			}
		} else {
			prev = cur;
			cur = cur->next;
		}
	}
 done:
	iax_mutex_unlock(sched_mutex);

	return 0;

}


time_in_ms_t iax_time_to_next_event(void)
{
	struct iax_sched *cur = NULL;
	time_in_ms_t minimum = 999999999;
	
	iax_mutex_lock(sched_mutex);
	cur = schedq;

	/* If there are no pending events, we don't need to timeout */
	if (!cur) {
		iax_mutex_unlock(sched_mutex);
		return -1;
	}
	while(cur) {
		if (cur->when < minimum) {
			minimum = cur->when;
		}
		cur = cur->next;
	}
	iax_mutex_unlock(sched_mutex);

	if (minimum <= 0) {
		return -1;
	}



	return minimum - current_time_in_ms();
}

struct iax_session *iax_session_new(void)
{
	struct iax_session *s;
	s = (struct iax_session *)malloc(sizeof(struct iax_session));
	if (s) {
		memset(s, 0, sizeof(struct iax_session));
		/* Initialize important fields */
		s->voiceformat = -1;
		s->svoiceformat = -1;
		/* Default pingtime to 30 ms */
		s->pingtime = 30;
		/* XXX Not quite right -- make sure it's not in use, but that won't matter
	           unless you've had at least 65k calls.  XXX */
		s->callno = callnums++;
		if (callnums > 32767)
			callnums = 1;
		s->peercallno = 0;
		s->transferpeer = 0;		/* for attended transfer */

		s->sendto = iax_sendto;
		s->pingid = -1;
#ifdef NEWJB
		s->jb = jb_new();
		{
			jb_conf jbconf;
			jbconf.max_jitterbuf = 0;
			jbconf.resync_threshold = 1000;
			jbconf.max_contig_interp = 0;
			jbconf.target_extra = jb_target_extra;
			jb_setconf(s->jb, &jbconf);
		}
#endif
		iax_mutex_lock(session_mutex);
		s->next = sessions;
		sessions = s;
		iax_mutex_unlock(session_mutex);
	}
	return s;
}

static int iax_session_valid(struct iax_session *session)
{
	/* Return -1 on a valid iax session pointer, 0 on a failure */
	struct iax_session *cur;

	iax_mutex_lock(session_mutex);
	cur = sessions;
	while(cur) {
		if (session == cur) {
			iax_mutex_unlock(session_mutex);
			return -1;
		}
		cur = cur->next;
	}
	iax_mutex_unlock(session_mutex);

	return 0;
}

int iax_get_netstats(struct iax_session *session, time_in_ms_t *rtt, struct iax_netstat *local, struct iax_netstat *remote) {

  if(!iax_session_valid(session)) return -1;

  *rtt = session->pingtime;

  *remote = session->remote_netstats;

#ifdef NEWJB
  {
      jb_info stats;
      jb_getinfo(session->jb, &stats);

      local->jitter = stats.jitter;
      /* XXX: should be short-term loss pct.. */
      if(stats.frames_in == 0) stats.frames_in = 1;
      local->losspct = stats.losspct/1000;
      local->losscnt = stats.frames_lost;
      local->packets = stats.frames_in;
      local->delay = stats.current - stats.min;
      local->dropped = stats.frames_dropped;
      local->ooo = stats.frames_ooo;
  }
#endif
  return 0;
}

static time_in_ms_t calc_timestamp(struct iax_session *session, time_in_ms_t ts, struct ast_frame *f)
{
	time_in_ms_t ms;
	time_in_ms_t time_in_ms;
	int voice = 0;
	int genuine = 0;
	
	if (f && f->frametype == AST_FRAME_VOICE) {
		voice = 1;
	} else if (!f || f->frametype == AST_FRAME_IAX) {
		genuine = 1;
	}
	
	/* If this is the first packet we're sending, get our
	   offset now. */
	if (!session->offset) {
		session->offset = current_time_in_ms();
	}
	/* If the timestamp is specified, just use their specified
	   timestamp no matter what.  Usually this is done for
	   special cases.  */
	if (ts) {
		return ts;
	}
	/* Otherwise calculate the timestamp from the current time */
	time_in_ms = current_time_in_ms();


	/* Calculate the number of milliseconds since we sent the first packet */
	ms = time_in_ms - session->offset;

	if (ms < 0) {
		ms = 0;
	}

	if(voice) {
#ifdef USE_VOICE_TS_PREDICTION

		/* If we haven't most recently sent silence, and we're
		 * close in time, use predicted time */
		if(session->notsilenttx && iax_abs(ms - session->nextpred) <= 240) {
		    /* Adjust our txcore, keeping voice and non-voice
		     * synchronized */
			session->offset += (int)(ms - session->nextpred)/10;
		    
		    if(!session->nextpred) {
				session->nextpred = ms; 
			}
		    ms = session->nextpred; 
		} else {
		    /* in this case, just use the actual time, since
		     * we're either way off (shouldn't happen), or we're
		     * ending a silent period -- and seed the next predicted
		     * time.  Also, round ms to the next multiple of
		     * frame size (so our silent periods are multiples
		     * of frame size too) */
		    time_in_ms_t diff = ms % (f->samples / 8);
		    if(diff)
			ms += f->samples/8 - diff;
		    session->nextpred = ms; 
		}
#else
		if(ms <= session->lastsent)
			ms = session->lastsent + 3;
#endif
		session->notsilenttx = 1;
	} else {
		/* On a dataframe, use last value + 3 (to accomodate jitter buffer shrinking) 
		   if appropriate unless it's a genuine frame */
		if (genuine) {
			if (ms <= session->lastsent)
				ms = session->lastsent + 3;
		} else if (iax_abs(ms - session->lastsent) <= 240) {
			ms = session->lastsent + 3;
		}
	      
	}

	/* Record the last sent packet for future reference */
	/* unless an AST_FRAME_IAX */
	if (!genuine)
		session->lastsent = ms;

#ifdef USE_VOICE_TS_PREDICTION
	/* set next predicted ts based on 8khz samples */
	if(voice)
	    session->nextpred = session->nextpred + f->samples / 8;
#endif

	return ms;
}

#ifdef NEWJB
static unsigned char get_n_bits_at(unsigned char *data, int n, int bit)
{
	int byte = bit / 8;       /* byte containing first bit */
	int rem = 8 - (bit % 8);  /* remaining bits in first byte */
	unsigned char ret = 0;
	
	if (n <= 0 || n > 8)
		return 0;

	if (rem < n) {
		ret = (unsigned char)(data[byte] << (n - rem));
		ret |= (data[byte + 1] >> (8 - n + rem));
	} else {
		ret = (unsigned char)(data[byte] >> (rem - n));
	}

	return (unsigned char)(ret & (0xff >> (8 - n)));
}

static int speex_get_wb_sz_at(unsigned char *data, int len, int bit)
{
	static int SpeexWBSubModeSz[] = {
		0, 36, 112, 192,
		352, 0, 0, 0 };
	int off = bit;
	unsigned char c;

	/* skip up to two wideband frames */
	if (((len * 8 - off) >= 5) && 
		get_n_bits_at(data, 1, off)) {
		c = get_n_bits_at(data, 3, off + 1);
		off += SpeexWBSubModeSz[c];

		if (((len * 8 - off) >= 5) && 
			get_n_bits_at(data, 1, off)) {
			c = get_n_bits_at(data, 3, off + 1);
			off += SpeexWBSubModeSz[c];

			if (((len * 8 - off) >= 5) && 
				get_n_bits_at(data, 1, off)) {
				/* too many in a row */
				DEBU(G "\tCORRUPT too many wideband streams in a row\n");
				return -1;
			}
		}

	}
	return off - bit;
}

static int speex_get_samples(unsigned char *data, int len)
{
	static int SpeexSubModeSz[] = {
		0, 43, 119, 160, 
		220, 300, 364, 492, 
		79, 0, 0, 0,
		0, 0, 0, 0 };
	static int SpeexInBandSz[] = { 
		1, 1, 4, 4,
		4, 4, 4, 4,
		8, 8, 16, 16,
		32, 32, 64, 64 };
	int bit = 0;
	int cnt = 0;
	int off = 0;
	unsigned char c;

	DEBU(G "speex_get_samples(%d)\n", len);
	while ((len * 8 - bit) >= 5) {
		/* skip wideband frames */
		off = speex_get_wb_sz_at(data, len, bit);
		if (off < 0)  {
			DEBU(G "\tERROR reading wideband frames\n");
			break;
		}
		bit += off;

		if ((len * 8 - bit) < 5) {
			DEBU(G "\tERROR not enough bits left after wb\n");
			break;
		}

		/* get control bits */
		c = get_n_bits_at(data, 5, bit);
		DEBU(G "\tCONTROL: %d at %d\n", c, bit);
		bit += 5;

		if (c == 15) { 
			DEBU(G "\tTERMINATOR\n");
			break;
		} else if (c == 14) {
			/* in-band signal; next 4 bits contain signal id */
			c = get_n_bits_at(data, 4, bit);
			bit += 4;
			DEBU(G "\tIN-BAND %d bits\n", SpeexInBandSz[c]);
			bit += SpeexInBandSz[c];
		} else if (c == 13) {
			/* user in-band; next 5 bits contain msg len */
			c = get_n_bits_at(data, 5, bit);
			bit += 5;
			DEBU(G "\tUSER-BAND %d bytes\n", c);
			bit += c * 8;
		} else if (c > 8) {
			DEBU(G "\tUNKNOWN sub-mode %d\n", c);
			break;
		} else {
			/* skip number bits for submode (less the 5 control bits) */
			DEBU(G "\tSUBMODE %d %d bits\n", c, SpeexSubModeSz[c]);
			bit += SpeexSubModeSz[c] - 5;

			cnt += 160; /* new frame */
		}
	}
	DEBU(G "\tSAMPLES: %d\n", cnt);
	return cnt;
}

static inline int get_interp_len(int format)
{
	return (format == AST_FORMAT_ILBC) ? 30 : 20;
}

static int get_sample_cnt(struct iax_event *e)
{
	int cnt = 0;

	/*
	 * In the case of zero length frames, do not return a cnt of 0
	 */
	if ( e->datalen == 0 ) {
		return get_interp_len( e->subclass ) * 8;
	}

	switch (e->subclass) {
	  case AST_FORMAT_SPEEX:
	    cnt = speex_get_samples(e->data, e->datalen);
	    break;
	  case AST_FORMAT_G723_1:
	    cnt = 240;		/* FIXME Not always the case */
	    break;
	  case AST_FORMAT_ILBC:
	    cnt = 240 * (e->datalen / 50);
	    break;
	  case AST_FORMAT_GSM:
	    cnt = 160 * (e->datalen / 33);
	    break;
	  case AST_FORMAT_G729A:
	    cnt = 160 * (e->datalen / 20);
	    break;
	  case AST_FORMAT_SLINEAR:
	    cnt = e->datalen / 2;
	    break;
	  case AST_FORMAT_LPC10:
	    cnt = 22 * 8 + (((char *)(e->data))[7] & 0x1) * 8;
	    break;
	  case AST_FORMAT_ULAW:
	  case AST_FORMAT_ALAW:
	    cnt = e->datalen;
	    break;
	  case AST_FORMAT_ADPCM:
	  case AST_FORMAT_G726:
	    cnt = e->datalen * 2;
	    break;
	  default:
	    return 0;
	}
	return cnt;
}
#endif

static int iax_xmit_frame(struct iax_frame *f)
{
	/* Send the frame raw */
#ifdef DEBUG_SUPPORT
	struct ast_iax2_full_hdr *h = (f->data);
	if (ntohs(h->scallno) & IAX_FLAG_FULL)
		iax_showframe(f, NULL, 0, f->transfer ? 
						&(f->session->transfer) :
					&(f->session->peeraddr), f->datalen - sizeof(struct ast_iax2_full_hdr));
#endif

	return f->session->sendto(netfd, (const char *) f->data, f->datalen,
		IAX_SOCKOPTS,
					f->transfer ? 
						(struct sockaddr *)&(f->session->transfer) :
					(struct sockaddr *)&(f->session->peeraddr), sizeof(f->session->peeraddr));
}

static int iax_reliable_xmit(struct iax_frame *f)
{
	struct iax_frame *fc;
	struct ast_iax2_full_hdr *fh;
	fh = (struct ast_iax2_full_hdr *) f->data;
	if (!fh->type) {
		DEBU(G "Asked to reliably transmit a non-packet.  Crashing.\n");
		*((char *)0)=0;
	}
	fc = (struct iax_frame *)malloc(sizeof(struct iax_frame));
	if (fc) {
		/* Make a copy of the frame */
		memcpy(fc, f, sizeof(struct iax_frame));
		/* And a copy of the data if applicable */
		if (!fc->data || !fc->datalen) {
			IAXERROR "No frame data?");
			DEBU(G "No frame data?\n");
			return -1;
		} else {
			fc->data = (char *)malloc(fc->datalen);
			if (!fc->data) {
				DEBU(G "Out of memory\n");
				IAXERROR "Out of memory\n");
				return -1;
			}
			memcpy(fc->data, f->data, f->datalen);
			iax_sched_add(NULL, fc, NULL, NULL, fc->retrytime);
			return iax_xmit_frame(fc);
		}
	} else
		return -1;
}

void iax_set_networking(sendto_t st, recvfrom_t rf)
{

	init_time();
	iax_sendto = st;
	iax_recvfrom = rf;
}

int __iax_shutdown(void)
{
	struct iax_sched *sp, *fp;

	/* Hangup Calls */
	if (sessions) {
		struct iax_session *sp = NULL, *fp = NULL;
		iax_mutex_lock(session_mutex); 
		for(sp = sessions; sp ;) {
			iax_hangup(sp, "System Shutdown");
			fp = sp;
			sp = sp->next;
			destroy_session(fp);
		}
		iax_mutex_unlock(session_mutex); 
	}

	/* Clear Scheduler */
	iax_mutex_lock(sched_mutex);
	for(sp = schedq; sp ;) {
		fp = sp;
		sp = sp->next;
		free(fp);
	}
	iax_mutex_unlock(sched_mutex);
	
	if (netfd > -1) {
		shutdown(netfd, 2);
		close(netfd);
	}

	time_end();

	iax_mutex_destroy(sched_mutex);
	iax_mutex_destroy(session_mutex);

	return 0;
}

int iax_shutdown(void)
{
	return do_shutdown++;
}

void iax_set_jb_target_extra( long value )
{
	/* store in jb_target_extra, a static global */
	jb_target_extra = value ;
}

int iax_init(char *ip, int preferredportno)
{
	int portno = preferredportno;
	struct sockaddr_in sin;
	int sinlen;
	int flags;

	init_time();

	iax_mutex_create(&sched_mutex);
	iax_mutex_create(&session_mutex);

	if(iax_recvfrom == (recvfrom_t) recvfrom) {
	    if (netfd > -1) {
		    /* Sokay, just don't do anything */
		    DEBU(G "Already initialized.");
		    return 0;
	    }
	    netfd = (int)socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
	    if (netfd < 0) {
		    DEBU(G "Unable to allocate UDP socket\n");
		    IAXERROR "Unable to allocate UDP socket\n");
		    return -1;
	    }
	    
	    if (preferredportno == 0) preferredportno = IAX_DEFAULT_PORTNO;
		if (preferredportno < 0)  preferredportno = 0;

		sin.sin_family = AF_INET;
		sin.sin_addr.s_addr = 0;
		sin.sin_port = htons((short)preferredportno);
		if (bind(netfd, (struct sockaddr *) &sin, sizeof(sin)) < 0) 
		{
#if defined(WIN32)  ||  defined(_WIN32_WCE)
			if (WSAGetLastError() == WSAEADDRINUSE)
#else
			if (errno == EADDRINUSE)
#endif
			{
				/*the port is already in use, so bind to a free port chosen by the IP stack*/
				DEBU(G "Unable to bind to preferred port - port is in use. Trying to bind to a free one");
				sin.sin_port = htons((short)0);
				if (bind(netfd, (struct sockaddr *) &sin, sizeof(sin)) < 0)
				{
					IAXERROR "Unable to bind UDP socket\n");
					return -1;
				}
			} else
			{
				IAXERROR "Unable to bind UDP socket\n");
				return -1;
			}
		}
		sinlen = sizeof(sin);
	    if (getsockname(netfd, (struct sockaddr *) &sin, &sinlen) < 0) {
		    close(netfd);
		    netfd = -1;
		    DEBU(G "Unable to figure out what I'm bound to.");
		    IAXERROR "Unable to determine bound port number.");
	    }
#ifdef	WIN32
	    flags = 1;
	    if (ioctlsocket(netfd,FIONBIO,(unsigned long *) &flags)) {
		    closesocket(netfd);
		    netfd = -1;
		    DEBU(G "Unable to set non-blocking mode.");
		    IAXERROR "Unable to set non-blocking mode.");
	    }
	    
#else
	    if ((flags = fcntl(netfd, F_GETFL)) < 0) {
		    close(netfd);
		    netfd = -1;
		    DEBU(G "Unable to retrieve socket flags.");
		    IAXERROR "Unable to retrieve socket flags.");
	    }
	    if (fcntl(netfd, F_SETFL, flags | O_NONBLOCK) < 0) {
		    close(netfd);
		    netfd = -1;
		    DEBU(G "Unable to set non-blocking mode.");
		    IAXERROR "Unable to set non-blocking mode.");
	    }
#endif
	    portno = ntohs(sin.sin_port);
	}
	srand((unsigned int)time(NULL));
	callnums = rand() % 32767 + 1;
	transfer_id = rand() % 32767 + 1;
	DEBU(G "Started on port %d\n", portno);
	return portno;	
}



static void convert_reply(char *out, unsigned char *in)
{
	int x;
	for (x=0;x<16;x++)
		out += sprintf(out, "%2.2x", (int)in[x]);
}

static unsigned char compress_subclass(int subclass)
{
	int x;
	int power=-1;
	/* If it's 128 or smaller, just return it */
	if (subclass < IAX_FLAG_SC_LOG)
		return (unsigned char)subclass;
	/* Otherwise find its power */
	for (x = 0; x < IAX_MAX_SHIFT; x++) {
		if (subclass & (1 << x)) {
			if (power > -1) {
				DEBU(G "Can't compress subclass %d\n", subclass);
				return 0;
			} else
				power = x;
		}
	}
	return (unsigned char)(power | IAX_FLAG_SC_LOG);
}

static int iax_send(struct iax_session *pvt, struct ast_frame *f, time_in_ms_t ts, int seqno, int now, int transfer, int final) 
{
	/* Queue a packet for delivery on a given private structure.  Use "ts" for
	   timestamp, or calculate if ts is 0.  Send immediately without retransmission
	   or delayed, with retransmission */
	struct ast_iax2_full_hdr *fh;
	struct ast_iax2_mini_hdr *mh;
	unsigned char	buf[5120];
	struct iax_frame *fr;
	int res;
	int sendmini=0;
	time_in_ms_t lastsent;
	time_in_ms_t fts;
	
	if (!pvt) {
		IAXERROR "No private structure for packet?\n");
		return -1;
	}
	
	/* this must come before the next call to calc_timestamp() since
	 calc_timestamp() will change lastsent to the returned value */
	lastsent = pvt->lastsent;

	/* Calculate actual timestamp */
	fts = calc_timestamp(pvt, ts, f);

	if (((fts & 0xFFFF0000L) == (lastsent & 0xFFFF0000L))
		/* High two bits are the same on timestamp, or sending on a trunk */ &&
	    (f->frametype == AST_FRAME_VOICE) 
		/* is a voice frame */ &&
		(f->subclass == pvt->svoiceformat) 
		/* is the same type */ ) {
			/* Force immediate rather than delayed transmission */
			now = 1;
			/* Mark that mini-style frame is appropriate */
			sendmini = 1;
	}
	/* Allocate an iax_frame */
	if (now) {
		fr = (struct iax_frame *) buf;
	} else
		fr = iax_frame_new(DIRECTION_OUTGRESS, f->datalen);
	if (!fr) {
		IAXERROR "Out of memory\n");
		return -1;
	}
	/* Copy our prospective frame into our immediate or retransmitted wrapper */
	iax_frame_wrap(fr, f);

	fr->ts = fts;
	if (!fr->ts) {
		IAXERROR "timestamp is 0?\n");
		if (!now)
			iax_frame_free(fr);
		return -1;
	}

	fr->callno = (unsigned short)pvt->callno;
	fr->transfer = transfer;
	fr->final = final;
	fr->session = pvt;
	if (!sendmini) {
		/* We need a full frame */
		if (seqno > -1)
			fr->oseqno = seqno;
		else
			fr->oseqno = pvt->oseqno++;
		fr->iseqno = pvt->iseqno;
		fh = (struct ast_iax2_full_hdr *)(((char *)fr->af.data) - sizeof(struct ast_iax2_full_hdr));
		fh->scallno = htons(fr->callno | IAX_FLAG_FULL);
		fh->ts = htonl((long)(fr->ts));
		fh->oseqno = (unsigned char)fr->oseqno;
		if (transfer) {
			fh->iseqno = 0;
		} else
			fh->iseqno = (unsigned char)fr->iseqno;
		/* Keep track of the last thing we've acknowledged */
		pvt->aseqno = (unsigned char)fr->iseqno;
		fh->type = (char)(fr->af.frametype & 0xFF);
		fh->csub = compress_subclass(fr->af.subclass);
		if (transfer) {
			fr->dcallno = (unsigned short)pvt->transfercallno;
		} else
			fr->dcallno = (unsigned short)pvt->peercallno;
		fh->dcallno = htons(fr->dcallno);
		fr->datalen = fr->af.datalen + sizeof(struct ast_iax2_full_hdr);
		fr->data = fh;
		fr->retries = maxretries;
		/* Retry after 2x the ping time has passed */
		fr->retrytime = pvt->pingtime * 2;
		if (fr->retrytime < MIN_RETRY_TIME)
			fr->retrytime = MIN_RETRY_TIME;
		if (fr->retrytime > MAX_RETRY_TIME)
			fr->retrytime = MAX_RETRY_TIME;
		/* Acks' don't get retried */
		if ((f->frametype == AST_FRAME_IAX) && (f->subclass == IAX_COMMAND_ACK))
			fr->retries = -1;
		if (f->frametype == AST_FRAME_VOICE) {
			pvt->svoiceformat = f->subclass;
		}
		if (now) {
			res = iax_xmit_frame(fr);
		} else
			res = iax_reliable_xmit(fr);
	} else {
		/* Mini-frames have no sequence number */
		fr->oseqno = -1;
		fr->iseqno = -1;
		/* Mini frame will do */
		mh = (struct ast_iax2_mini_hdr *)(((char *)fr->af.data) - sizeof(struct ast_iax2_mini_hdr));
		mh->callno = htons(fr->callno);
		mh->ts = htons((short)(fr->ts & 0xFFFF));
		fr->datalen = fr->af.datalen + sizeof(struct ast_iax2_mini_hdr);
		fr->data = mh;
		fr->retries = -1;
		res = iax_xmit_frame(fr);
	}
	if( !now && fr!=NULL )
	        iax_frame_free( fr ); 
	return res;
}

#if 0
static int iax_predestroy(struct iax_session *pvt)
{
	if (!pvt) {
		return -1;
	}
	if (!pvt->alreadygone) {
		/* No more pings or lagrq's */
		if (pvt->pingid > -1)
			ast_sched_del(sched, pvt->pingid);
		if (pvt->lagid > -1)
			ast_sched_del(sched, pvt->lagid);
		if (pvt->autoid > -1)
			ast_sched_del(sched, pvt->autoid);
		if (pvt->initid > -1)
			ast_sched_del(sched, pvt->initid);
		pvt->pingid = -1;
		pvt->lagid = -1;
		pvt->autoid = -1;
		pvt->initid = -1;
		pvt->alreadygone = 1;
	}
	return 0;
}
#endif

static int __send_command(struct iax_session *i, char type, int command, time_in_ms_t ts, unsigned char *data, int datalen, int seqno, 
		int now, int transfer, int final, int samples)
{
	struct ast_frame f;
	f.frametype = type;
	f.subclass = command;
	f.datalen = datalen;
	f.samples = samples;
	f.mallocd = 0;
	f.offset = 0;
#ifdef __GNUC__
	f.src = (char *) __FUNCTION__;
#else
	f.src = (char *) __FILE__;
#endif
	f.data = data;
	return iax_send(i, &f, ts, seqno, now, transfer, final);
}

static int send_command(struct iax_session *i, char type, int command, time_in_ms_t ts, unsigned char *data, int datalen, int seqno)
{
	return __send_command(i, type, command, ts, data, datalen, seqno, 0, 0, 0, 0);
}

static int send_command_final(struct iax_session *i, char type, int command, unsigned int ts, unsigned char *data, int datalen, int seqno)
{
#if 0
	/* It is assumed that the callno has already been locked */
	iax_predestroy(i);
#endif	
	int r;
	r = __send_command(i, type, command, ts, data, datalen, seqno, 0, 0, 1, 0);
	if (r >= 0) {
		iax_mutex_lock(session_mutex); 
		destroy_session(i);
		iax_mutex_unlock(session_mutex);
	}
	return r;
}

static int send_command_immediate(struct iax_session *i, char type, int command, unsigned int ts, unsigned char *data, int datalen, int seqno)
{
	return __send_command(i, type, command, ts, data, datalen, seqno, 1, 0, 0, 0);
}

static int send_command_transfer(struct iax_session *i, char type, int command, unsigned int ts, unsigned char *data, int datalen)
{
	return __send_command(i, type, command, ts, data, datalen, 0, 0, 1, 0, 0);
}

static int send_command_samples(struct iax_session *i, char type, int command, unsigned int ts, unsigned char *data, int datalen, int seqno, int samples)
{
	return __send_command(i, type, command, ts, data, datalen, seqno, 0, 0, 0, samples);
}


int iax_transfer(struct iax_session *session, char *number)
{	
	static int res;				//Return Code
	struct iax_ie_data ied;			//IE Data Structure (Stuff To Send)

	// Clear The Memory Used For IE Buffer
	memset(&ied, 0, sizeof(ied));
	
	// Copy The Transfer Destination Into The IE Structure
	iax_ie_append_str(&ied, IAX_IE_CALLED_NUMBER, (unsigned char *) number);
	
	// Send The Transfer Command - Asterisk Will Handle The Rest!			
	res = send_command(session, AST_FRAME_IAX, IAX_COMMAND_TRANSFER, 0, ied.buf, ied.pos, -1);
	
	// Return Success
	return 0;	
}

static void stop_transfer(struct iax_session *session)
{
	struct iax_sched *sch;

	iax_mutex_lock(sched_mutex);
	sch = schedq;
	while(sch) {
		if (sch->frame && (sch->frame->session == session))
					sch->frame->retries = -1;
		sch = sch->next;
	}
	iax_mutex_unlock(sched_mutex);
}	/* stop_transfer */

static void complete_transfer(struct iax_session *session, int peercallno, int xfr2peer, int preserveSeq)
{
	session->peercallno = peercallno;
	/* Change from transfer to session now */
	if (xfr2peer) {
		memcpy(&session->peeraddr, &session->transfer, sizeof(session->peeraddr));
		memset(&session->transfer, 0, sizeof(session->transfer));
		session->transferring = TRANSFER_NONE;
		session->transferpeer = 0;
		session->transfer_moh = 0;
		/* Force retransmission of a real voice packet, and reset all timing */
		session->svoiceformat = -1;
		session->voiceformat = 0;
	}
	session->rxcore = session->offset = 0;
	memset(&session->history, 0, sizeof(session->history));
#ifdef NEWJB
	{ /* Reset jitterbuffer */
	    jb_frame frame;
	    while(jb_getall(session->jb,&frame) == JB_OK) 
		iax_event_free(frame.data);
	
	    jb_reset(session->jb);
	}
#endif
	session->jitterbuffer = 0;
	session->jitter = 0;
	session->lag = 0;

	if (! preserveSeq)
	{
		/* Reset sequence numbers */
		session->aseqno = 0;
		session->oseqno = 0;
		session->iseqno = 0;
	}

	session->lastsent = 0;
	session->last_ts = 0;
	session->lastvoicets = 0;
	session->pingtime = 30;
	/* We have to dump anything we were going to (re)transmit now that we've been
	   transferred since they're all invalid and for the old host. */
	stop_transfer(session);
}	/* complete_transfer */

int iax_setup_transfer(struct iax_session *org_session, struct iax_session *new_session)
{
	int res;
	struct iax_ie_data ied0;
	struct iax_ie_data ied1;

	struct iax_session *s0 = org_session;
	struct iax_session *s1 = new_session;

	memset(&ied0, 0, sizeof(ied0));
	memset(&ied1, 0, sizeof(ied1));

	/* reversed setup */
	iax_ie_append_addr(&ied0, IAX_IE_APPARENT_ADDR, &s1->peeraddr);
	iax_ie_append_short(&ied0, IAX_IE_CALLNO, (unsigned short)s1->peercallno);
	iax_ie_append_int(&ied0, IAX_IE_TRANSFERID, transfer_id);

	iax_ie_append_addr(&ied1, IAX_IE_APPARENT_ADDR, &s0->peeraddr);
	iax_ie_append_short(&ied1, IAX_IE_CALLNO, (unsigned short)s0->peercallno);
	iax_ie_append_int(&ied1, IAX_IE_TRANSFERID, transfer_id);

	s0->transfer = s1->peeraddr;
	s1->transfer = s0->peeraddr;

	s0->transferid = transfer_id;
	s1->transferid = transfer_id;

	s0->transfercallno = s0->peercallno;
	s1->transfercallno = s1->peercallno;

	s0->transferring = TRANSFER_BEGIN;
	s1->transferring = TRANSFER_BEGIN;

	s0->transferpeer = s1->callno;
	s1->transferpeer = s0->callno;

	transfer_id++;

	if (transfer_id > 32767)
		transfer_id = 1;

	res = send_command(s0, AST_FRAME_IAX, IAX_COMMAND_TXREQ, 0, ied0.buf, ied0.pos, -1);
	if (res < 0) {
		return -1;
	}

	res = send_command(s1, AST_FRAME_IAX, IAX_COMMAND_TXREQ, 0, ied1.buf, ied1.pos, -1);
	if (res < 0) {
		return -1;
	}

	return 0;
}

static int iax_finish_transfer(struct iax_session *s, short new_peer)
{
	int res;
	struct iax_ie_data ied;

	memset(&ied, 0, sizeof(ied));

	iax_ie_append_short(&ied, IAX_IE_CALLNO, new_peer);

	res = send_command(s, AST_FRAME_IAX, IAX_COMMAND_TXREL, 0, ied.buf, ied.pos, -1);

	complete_transfer(s, new_peer, 0, 1);

	return res;

}

static struct iax_session *iax_find_session2(short callno)
{
	struct iax_session *cur;

	iax_mutex_lock(session_mutex); 
	cur = sessions;
	while(cur) {
		if (callno == cur->callno && callno != 0)  {
			iax_mutex_unlock(session_mutex); 
			return cur;
		}
		cur = cur->next;
	}
	iax_mutex_unlock(session_mutex); 

	return NULL;
}

static int iax_handle_txready(struct iax_session *s)
{
	struct iax_session *s0, *s1;
	short	s0_org_peer, s1_org_peer;

	if (s->transfer_moh) {
		s->transfer_moh = 0;
		iax_unquelch(s);
	}

	complete_transfer(s, s->peercallno, 0, 1);

	s->transferring = TRANSFER_REL;

	s0 = s;
	s1 = iax_find_session2((short)s0->transferpeer);

	if (s1 != NULL &&
	    s1->callno == s0->transferpeer &&
		 s0->transferring == TRANSFER_REL &&
		 s1->transferring == TRANSFER_REL) {

		s0_org_peer = (short)s0->peercallno;
		s1_org_peer = (short)s1->peercallno;

		iax_finish_transfer(s0, s1_org_peer);
		iax_finish_transfer(s1, s0_org_peer);
		return 1;
	}

	return 0;
}

static void iax_handle_txreject(struct iax_session *s)
{
	struct iax_session *s0, *s1;

	s0 = s;
	s1 = iax_find_session2((short)s0->transferpeer);
	if (s1 != NULL &&
		 s0->transferpeer == s1->callno &&
		 s1->transferring) {
		if (s1->transfer_moh) {
			s1->transfer_moh = 0;
			send_command_immediate(s1, AST_FRAME_IAX, IAX_COMMAND_UNQUELCH, 0, NULL, 0, s1->iseqno);
		}
	}
	if (s0->transfer_moh) {
		s0->transfer_moh = 0;
		send_command_immediate(s0, AST_FRAME_IAX, IAX_COMMAND_UNQUELCH, 0, NULL, 0, s0->iseqno);
	}

	memset(&s->transfer, 0, sizeof(s->transfer));
	s->transferring = TRANSFER_NONE;
	s->transferpeer = 0;
	s->transfer_moh = 0;
}

static void destroy_session(struct iax_session *session)
{
	struct iax_session *cur, *prev=NULL;
	struct iax_sched *curs, *prevs=NULL, *nexts=NULL;
	int    loop_cnt=0;

	iax_mutex_lock(sched_mutex);
	curs = schedq;
	while(curs) {
		nexts = curs->next;
		if (curs->frame && curs->frame->session == session) {
			/* Just mark these frames as if they've been sent */
			curs->frame->retries = -1;
		} else if (curs->event && curs->event->session == session) {
			if (prevs)
				prevs->next = nexts;
			else
				schedq = nexts;
			if (curs->event)
				iax_event_free(curs->event);
			free(curs);
		} else {
			prevs = curs;
		}
		curs = nexts;
		loop_cnt++;
	}
	iax_mutex_unlock(sched_mutex);
	

	cur = sessions;
	while(cur) {
		if (cur == session) {
			if (prev)
				prev->next = session->next;
			else
				sessions = session->next;
#ifdef NEWJB
			{
			    jb_frame frame;
			    while(jb_getall(session->jb,&frame) == JB_OK) 
				iax_event_free(frame.data);
		   	
			    jb_destroy(session->jb);
			}
#endif
			free(session);
			return;
		}
		prev = cur;
		cur = cur->next;
	}
}

static int iax_send_lagrp(struct iax_session *session, time_in_ms_t ts);
static int iax_send_pong(struct iax_session *session, time_in_ms_t ts);

static struct iax_event *handle_event(struct iax_event *event)
{
	/* We have a candidate event to be delievered.  Be sure
	   the session still exists. */
	if (event) {
		if (iax_session_valid(event->session)) {
			/* Lag requests are never actually sent to the client, but
			   other than that are handled as normal packets */
			switch(event->etype) {
				/* the user on the outside may need to look at the session so we will not free 
				   it here anymore we will test for hangup event in iax_event_free and do it
				   there.
				 */
			case IAX_EVENT_REJECT:
			case IAX_EVENT_HANGUP:
				return event;
			case IAX_EVENT_LAGRQ:
				event->etype = IAX_EVENT_LAGRP;
				iax_send_lagrp(event->session, event->ts);
				iax_event_free(event);
				break;
			case IAX_EVENT_PING:
				event->etype = IAX_EVENT_PONG;
				iax_send_pong(event->session, event->ts);
				iax_event_free(event);
				break;
			case IAX_EVENT_POKE:
				event->etype = IAX_EVENT_PONG;
				iax_send_pong(event->session, event->ts);
				iax_destroy(event->session);
				iax_event_free(event);
				break;         
			default:
				return event;
			}
		} else 
			iax_event_free(event);
	}
	return NULL;
}

static int iax2_vnak(struct iax_session *session)
{
	return send_command_immediate(session, AST_FRAME_IAX, IAX_COMMAND_VNAK, 0, NULL, 0, session->iseqno);
}

int iax_send_dtmf(struct iax_session *session, char digit)
{
	return send_command(session, AST_FRAME_DTMF, digit, 0, NULL, 0, -1);
}

int iax_send_voice(struct iax_session *session, int format, unsigned char *data, int datalen, int samples)
{
	/* Send a (possibly compressed) voice frame */
	if (!session->quelch)
		return send_command_samples(session, AST_FRAME_VOICE, format, 0, data, datalen, -1, samples);
	return 0;
}

int iax_send_cng(struct iax_session *session, int level, unsigned char *data, int datalen)
{    
	session->notsilenttx = 0;
	return send_command(session, AST_FRAME_CNG, level, 0, data, datalen, -1);
}

int iax_send_image(struct iax_session *session, int format, unsigned char *data, int datalen)
{
	/* Send an image frame */
	return send_command(session, AST_FRAME_IMAGE, format, 0, data, datalen, -1);
}

int iax_register(struct iax_session *session, char *server, char *peer, char *secret, int refresh)
{
	/* Send a registration request */
	char tmp[256];
	char *p;
	int res;
	int portno = IAX_DEFAULT_PORTNO;
	struct iax_ie_data ied;
	struct hostent *hp;
	
	tmp[255] = '\0';
	strncpy(tmp, server, sizeof(tmp) - 1);
	p = strchr(tmp, ':');
	if (p) {
		*p = '\0';
		portno = atoi(p+1);
	}
	
	memset(&ied, 0, sizeof(ied));
	if (secret)
		strncpy(session->secret, secret, sizeof(session->secret) - 1);
	else
		strcpy(session->secret, "");

	/* Connect first */
	hp = gethostbyname(tmp);
	if (!hp) {
		snprintf(iax_errstr, sizeof(iax_errstr), "Invalid hostname: %s", tmp);
		return -1;
	}
	memcpy(&session->peeraddr.sin_addr, hp->h_addr, sizeof(session->peeraddr.sin_addr));
	session->peeraddr.sin_port = htons((unsigned short)portno);
	session->peeraddr.sin_family = AF_INET;
	strncpy(session->username, peer, sizeof(session->username) - 1);
	session->refresh = refresh;
	iax_ie_append_str(&ied, IAX_IE_USERNAME, (unsigned char *) peer);
	iax_ie_append_short(&ied, IAX_IE_REFRESH, (unsigned short)refresh);
	res = send_command(session, AST_FRAME_IAX, IAX_COMMAND_REGREQ, 0, ied.buf, ied.pos, -1);
	return res;
}

int iax_reject_registration(struct iax_session *session, char *reason)
{
	struct iax_ie_data ied;
	memset(&ied, 0, sizeof(ied));
	iax_ie_append_str(&ied, IAX_IE_CAUSE, reason ? (unsigned char *) reason : (unsigned char *) "Unspecified");
	return send_command_final(session, AST_FRAME_IAX, IAX_COMMAND_REGREJ, 0, ied.buf, ied.pos, -1);
}

int iax_ack_registration(struct iax_session *session)
{
	return send_command_final(session, AST_FRAME_IAX, IAX_COMMAND_REGACK, 0, NULL, 0, -1);
}

int iax_auth_registration(struct iax_session *session)
{
	return send_command_final(session, AST_FRAME_IAX, IAX_COMMAND_REGAUTH, 0, NULL, 0, -1);
}

int iax_reject(struct iax_session *session, const char *reason)
{
	struct iax_ie_data ied;
	memset(&ied, 0, sizeof(ied));
	iax_ie_append_str(&ied, IAX_IE_CAUSE, reason ? (unsigned char *) reason : (unsigned char *) "Unspecified");
	return send_command_final(session, AST_FRAME_IAX, IAX_COMMAND_REJECT, 0, ied.buf, ied.pos, -1);
}

int iax_hangup(struct iax_session *session, char *byemsg)
{
	struct iax_ie_data ied;
	iax_sched_del(NULL, NULL, send_ping, (void *) session, 1);
	memset(&ied, 0, sizeof(ied));
	iax_ie_append_str(&ied, IAX_IE_CAUSE, byemsg ? (unsigned char *) byemsg : (unsigned char *) "Normal clearing");
	return send_command_final(session, AST_FRAME_IAX, IAX_COMMAND_HANGUP, 0, ied.buf, ied.pos, -1);
}

int iax_sendurl(struct iax_session *session, char *url)
{
	return send_command(session, AST_FRAME_HTML, AST_HTML_URL, 0, (unsigned char *) url, (int)strlen(url), -1);
}

int iax_ring_announce(struct iax_session *session)
{
	return send_command(session, AST_FRAME_CONTROL, AST_CONTROL_RINGING, 0, NULL, 0, -1);
}

int iax_lag_request(struct iax_session *session)
{
	return send_command(session, AST_FRAME_IAX, IAX_COMMAND_LAGRQ, 0, NULL, 0, -1);
}

int iax_busy(struct iax_session *session)
{
	return send_command(session, AST_FRAME_CONTROL, AST_CONTROL_BUSY, 0, NULL, 0, -1);
}

int iax_congestion(struct iax_session *session)
{
	return send_command(session, AST_FRAME_CONTROL, AST_CONTROL_CONGESTION, 0, NULL, 0, -1);
}


int iax_accept(struct iax_session *session, int format)
{
	struct iax_ie_data ied;
	memset(&ied, 0, sizeof(ied));
	iax_ie_append_int(&ied, IAX_IE_FORMAT, format);
	return send_command(session, AST_FRAME_IAX, IAX_COMMAND_ACCEPT, 0, ied.buf, ied.pos, -1);
}

int iax_answer(struct iax_session *session)
{
	return send_command(session, AST_FRAME_CONTROL, AST_CONTROL_ANSWER, 0, NULL, 0, -1);
}

int iax_load_complete(struct iax_session *session)
{
	return send_command(session, AST_FRAME_HTML, AST_HTML_LDCOMPLETE, 0, NULL, 0, -1);
}

int iax_send_url(struct iax_session *session, char *url, int link)
{
	return send_command(session, AST_FRAME_HTML, link ? AST_HTML_LINKURL : AST_HTML_URL, 0, (unsigned char *) url, (int)strlen(url), -1);
}

int iax_send_text(struct iax_session *session, char *text)
{
	return send_command(session, AST_FRAME_TEXT, 0, 0, (unsigned char *) text, (int)strlen(text) + 1, -1);
}

int iax_send_unlink(struct iax_session *session)
{
	return send_command(session, AST_FRAME_HTML, AST_HTML_UNLINK, 0, NULL, 0, -1);
}

int iax_send_link_reject(struct iax_session *session)
{
	return send_command(session, AST_FRAME_HTML, AST_HTML_LINKREJECT, 0, NULL, 0, -1);
}

static int iax_send_pong(struct iax_session *session, time_in_ms_t ts)
{
        struct iax_ie_data ied;

        memset(&ied, 0, sizeof(ied));
#ifdef NEWJB
	{
	    jb_info stats;
	    jb_getinfo(session->jb, &stats);

	    iax_ie_append_int(&ied,IAX_IE_RR_JITTER, (int)stats.jitter);
	    /* XXX: should be short-term loss pct.. */
	    if(stats.frames_in == 0) stats.frames_in = 1;
	    iax_ie_append_int(&ied,IAX_IE_RR_LOSS, 
		((0xff & (stats.losspct/1000)) << 24 | (stats.frames_lost & 0x00ffffff)));
	    iax_ie_append_int(&ied,IAX_IE_RR_PKTS, stats.frames_in);
	    iax_ie_append_short(&ied,IAX_IE_RR_DELAY, (unsigned short)(stats.current - stats.min));
	    iax_ie_append_int(&ied,IAX_IE_RR_DROPPED, stats.frames_dropped);
	    iax_ie_append_int(&ied,IAX_IE_RR_OOO, stats.frames_ooo);
	}
#else
	    iax_ie_append_int(&ied,IAX_IE_RR_JITTER, session->jitter);
	    /* don't know, don't send! iax_ie_append_int(&ied,IAX_IE_RR_LOSS, 0); */
	    /* don't know, don't send! iax_ie_append_int(&ied,IAX_IE_RR_PKTS, stats.frames_in); */
	    /* don't know, don't send! iax_ie_append_short(&ied,IAX_IE_RR_DELAY, stats.current - stats.min); */
#endif

	return send_command(session, AST_FRAME_IAX, IAX_COMMAND_PONG, ts, ied.buf, ied.pos, -1);
}

/* external API; deprecated since we send pings ourselves now (finally) */
int iax_send_ping(struct iax_session *session)
{
	return send_command(session, AST_FRAME_IAX, IAX_COMMAND_PING, 0, NULL, 0, -1);
}

/* scheduled ping sender; sends ping, then reschedules */
static void send_ping(void *s)
{
	struct iax_session *session = (struct iax_session *)s;

	/* important, eh? */
	if(!iax_session_valid(session)) return;

	send_command(session, AST_FRAME_IAX, IAX_COMMAND_PING, 0, NULL, 0, -1);
	session->pingid = iax_sched_add(NULL,NULL, send_ping, (void *)session, ping_time * 1000);
	return;
}

static int iax_send_lagrp(struct iax_session *session, time_in_ms_t ts)
{
	return send_command(session, AST_FRAME_IAX, IAX_COMMAND_LAGRP, ts, NULL, 0, -1);
}

static int iax_send_txcnt(struct iax_session *session)
{
	struct iax_ie_data ied;
	memset(&ied, 0, sizeof(ied));
	iax_ie_append_int(&ied, IAX_IE_TRANSFERID, session->transferid);
	return send_command_transfer(session, AST_FRAME_IAX, IAX_COMMAND_TXCNT, 0, ied.buf, ied.pos);
}

static int iax_send_txrej(struct iax_session *session)
{
	struct iax_ie_data ied;
	memset(&ied, 0, sizeof(ied));
	iax_ie_append_int(&ied, IAX_IE_TRANSFERID, session->transferid);
	return send_command_transfer(session, AST_FRAME_IAX, IAX_COMMAND_TXREJ, 0, ied.buf, ied.pos);
}

static int iax_send_txaccept(struct iax_session *session)
{
	struct iax_ie_data ied;
	memset(&ied, 0, sizeof(ied));
	iax_ie_append_int(&ied, IAX_IE_TRANSFERID, session->transferid);
	return send_command_transfer(session, AST_FRAME_IAX, IAX_COMMAND_TXACC, 0, ied.buf, ied.pos);
}

static int iax_send_txready(struct iax_session *session)
{
	struct iax_ie_data ied;
	memset(&ied, 0, sizeof(ied));
	/* see asterisk chan_iax2.c */
	iax_ie_append_short(&ied, IAX_IE_CALLNO, (unsigned short)session->callno);
	return send_command(session, AST_FRAME_IAX, IAX_COMMAND_TXREADY, 0, ied.buf, ied.pos, -1);
}

int iax_auth_reply(struct iax_session *session, char *password, char *challenge, int methods)
{
	char reply[16];
	struct MD5Context md5;
	char realreply[256];
	struct iax_ie_data ied;
	memset(&ied, 0, sizeof(ied));
	if ((methods & IAX_AUTH_MD5) && challenge) {
		MD5Init(&md5);
		MD5Update(&md5, (const unsigned char *) challenge, (unsigned int)strlen(challenge));
		MD5Update(&md5, (const unsigned char *) password, (unsigned int)strlen(password));
		MD5Final((unsigned char *) reply, &md5);
		memset(realreply, 0, sizeof(realreply));
		convert_reply(realreply, (unsigned char *) reply);
		iax_ie_append_str(&ied, IAX_IE_MD5_RESULT, (unsigned char *) realreply);
	} else {
		iax_ie_append_str(&ied, IAX_IE_PASSWORD, (unsigned char *) password);
	}
	return send_command(session, AST_FRAME_IAX, IAX_COMMAND_AUTHREP, 0, ied.buf, ied.pos, -1);
}

static int iax_regauth_reply(struct iax_session *session, char *password, char *challenge, int methods)
{
	char reply[16];
	struct MD5Context md5;
	char realreply[256];
	struct iax_ie_data ied;
	memset(&ied, 0, sizeof(ied));
	iax_ie_append_str(&ied, IAX_IE_USERNAME, (unsigned char *) session->username);
	iax_ie_append_short(&ied, IAX_IE_REFRESH, (unsigned short)session->refresh);
	if ((methods & IAX_AUTHMETHOD_MD5) && challenge) {
		MD5Init(&md5);
		MD5Update(&md5, (const unsigned char *) challenge, (unsigned int)strlen(challenge));
		MD5Update(&md5, (const unsigned char *) password, (unsigned int)strlen(password));
		MD5Final((unsigned char *) reply, &md5);
		memset(realreply, 0, sizeof(realreply));
		convert_reply(realreply, (unsigned char *) reply);
		iax_ie_append_str(&ied, IAX_IE_MD5_RESULT, (unsigned char *) realreply);
	} else {
		iax_ie_append_str(&ied, IAX_IE_PASSWORD, (unsigned char *) password);
	}
	return send_command(session, AST_FRAME_IAX, IAX_COMMAND_REGREQ, 0, ied.buf, ied.pos, -1);
}


int iax_dial(struct iax_session *session, char *number)
{
	struct iax_ie_data ied;
	memset(&ied, 0, sizeof(ied));
	iax_ie_append_str(&ied, IAX_IE_CALLED_NUMBER, (unsigned char *) number);
	return send_command(session, AST_FRAME_IAX, IAX_COMMAND_DIAL, 0, ied.buf, ied.pos, -1);
}

int iax_quelch(struct iax_session *session)
{
	return send_command(session, AST_FRAME_IAX, IAX_COMMAND_QUELCH, 0, NULL, 0, -1);
}

int iax_unquelch(struct iax_session *session)
{
	return send_command(session, AST_FRAME_IAX, IAX_COMMAND_UNQUELCH, 0, NULL, 0, -1);
}

int iax_dialplan_request(struct iax_session *session, char *number)
{
	struct iax_ie_data ied;
	memset(&ied, 0, sizeof(ied));
	iax_ie_append_str(&ied, IAX_IE_CALLED_NUMBER, (unsigned char *) number);
	return send_command(session, AST_FRAME_IAX, IAX_COMMAND_DPREQ, 0, ied.buf, ied.pos, -1);
}

static inline int which_bit(unsigned int i)
{
    unsigned char x;
    for(x = 0; x < 32; x++) {
        if ((unsigned)(1 << x) == i) {
            return x + 1;
        }
    }
    return 0;
}

char iax_pref_codec_add(struct iax_session *session, unsigned int format)
{
	int diff = (int) 'A';
	session->codec_order[session->codec_order_len++] = (char)((which_bit(format)) + diff);
	session->codec_order[session->codec_order_len] = '\0';
	return session->codec_order[session->codec_order_len-1];
}


void iax_pref_codec_del(struct iax_session *session, unsigned int format)
{
	int diff = (int) 'A';
	size_t x;
	char old[32];
	char remove = (char)(which_bit(format) + diff);

	strncpy(old, session->codec_order, sizeof(old));
	session->codec_order_len = 0;

	for (x = 0; x < strlen(old) ; x++) {
		if (old[x] != remove) {
			session->codec_order[session->codec_order_len++] = old[x];
		}
	}
	session->codec_order[session->codec_order_len] = '\0';
}

int iax_pref_codec_get(struct iax_session *session, unsigned int *array, int len)
{
	int diff = (int) 'A';
	int x;
	
	for (x = 0; x < session->codec_order_len && x < len; x++) {
		array[x] = (1 << (session->codec_order[x] - diff - 1));
	}

	return x;
}

void iax_set_samplerate(struct iax_session *session, unsigned short samplemask)
{
	session->samplemask = samplemask;
}


int iax_call(struct iax_session *session, const char *cidnum, const char *cidname, char *ich, char *lang, int wait, int formats, int capabilities)
{
	char tmp[256]="";
	char *part1, *part2;
	int res;
	int portno;
	char *username, *hostname, *secret, *context, *exten, *dnid;
	struct iax_ie_data ied;
	struct hostent *hp;
	/* We start by parsing up the temporary variable which is of the form of:
	   [user@]peer[:portno][/exten[@context]] */
	if (!ich) {
		IAXERROR "Invalid IAX Call Handle\n");
		DEBU(G "Invalid IAX Call Handle\n");
		return -1;
	}
	memset(&ied, 0, sizeof(ied));
	strncpy(tmp, ich, sizeof(tmp) - 1);	

	iax_ie_append_short(&ied, IAX_IE_VERSION, IAX_PROTO_VERSION);
    if (session->samplemask) {
		iax_ie_append_short(&ied, IAX_IE_SAMPLINGRATE, session->samplemask);
	}
	if (cidnum)
		iax_ie_append_str(&ied, IAX_IE_CALLING_NUMBER, (unsigned char *) cidnum);
	if (cidname)
		iax_ie_append_str(&ied, IAX_IE_CALLING_NAME, (unsigned char *) cidname);

	if (session->codec_order_len) {
		iax_ie_append_str(&ied, IAX_IE_CODEC_PREFS, (unsigned char *) session->codec_order);
	}

	session->capability = capabilities;
	session->pingid = iax_sched_add(NULL,NULL, send_ping, (void *)session, 2 * 1000);

	/* XXX We should have a preferred format XXX */
	iax_ie_append_int(&ied, IAX_IE_FORMAT, formats);
	iax_ie_append_int(&ied, IAX_IE_CAPABILITY, capabilities);
	if (lang)
		iax_ie_append_str(&ied, IAX_IE_LANGUAGE, (unsigned char *) lang);
	
	/* Part 1 is [user[:password]@]peer[:port] */
	part1 = strtok(tmp, "/");

	/* Part 2 is exten[@context] if it is anything all */
	part2 = strtok(NULL, "/");
	
	if (strchr(part1, '@')) {
		username = strtok(part1, "@");
		hostname = strtok(NULL, "@");
	} else {
		username = NULL;
		hostname = part1;
	}
	
	if (username && strchr(username, ':')) {
		username = strtok(username, ":");
		secret = strtok(NULL, ":");
	} else
		secret = NULL;

	if(username)
	  strncpy(session->username, username, sizeof(session->username) - 1);

	if(secret)
	  strncpy(session->secret, secret, sizeof(session->secret) - 1);
	
	if (strchr(hostname, ':')) {
		strtok(hostname, ":");
		portno = atoi(strtok(NULL, ":"));
	} else {
		portno = IAX_DEFAULT_PORTNO;
	}
	if (part2) {
		exten = strtok(part2, "@");
		dnid = exten;
		context = strtok(NULL, "@");
	} else {
		exten = NULL;
		dnid = NULL;
		context = NULL;
	}
	if (username)
		iax_ie_append_str(&ied, IAX_IE_USERNAME, (unsigned char *) username);
	if (exten && strlen(exten))
		iax_ie_append_str(&ied, IAX_IE_CALLED_NUMBER, (unsigned char *) exten);
	if (dnid && strlen(dnid))
		iax_ie_append_str(&ied, IAX_IE_DNID, (unsigned char *) dnid);
	if (context && strlen(context))
		iax_ie_append_str(&ied, IAX_IE_CALLED_CONTEXT, (unsigned char *) context);

	/* Setup host connection */
	hp = gethostbyname(hostname);
	if (!hp) {
		snprintf(iax_errstr, sizeof(iax_errstr), "Invalid hostname: %s", hostname);
		return -1;
	}
	memcpy(&session->peeraddr.sin_addr, hp->h_addr, sizeof(session->peeraddr.sin_addr));
	session->peeraddr.sin_port = htons((unsigned short)portno);
	session->peeraddr.sin_family = AF_INET;
	res = send_command(session, AST_FRAME_IAX, IAX_COMMAND_NEW, 0, ied.buf, ied.pos, -1);
	if (res < 0)
		return res;
	if (wait) {
		DEBU(G "Waiting not yet implemented\n");
		return -1;
	}
	return res;
}

static time_in_ms_t calc_rxstamp(struct iax_session *session)
{
	time_in_ms_t time_in_ms;

	time_in_ms = current_time_in_ms();

	if (!session->rxcore) {
		session->rxcore = time_in_ms;
	}

	return time_in_ms - session->rxcore;
}

#ifdef notdef_cruft
static int match(struct sockaddr_in *sin, short callno, short dcallno, struct iax_session *cur)
{
	if ((cur->peeraddr.sin_addr.s_addr == sin->sin_addr.s_addr) &&
		(cur->peeraddr.sin_port == sin->sin_port)) {
		/* This is the main host */
		if ((cur->peercallno == callno) || 
			((dcallno == cur->callno) && !cur->peercallno)) {
			/* That's us.  Be sure we keep track of the peer call number */
			cur->peercallno = callno;
			return 1;
		}
	}
	if ((cur->transfer.sin_addr.s_addr == sin->sin_addr.s_addr) &&
	    (cur->transfer.sin_port == sin->sin_port) && (cur->transferring)) {
		/* We're transferring */
		if (dcallno == cur->callno)
			return 1;
	}
	return 0;
}
#endif

/* splitted match into 2 passes otherwise causing problem of matching
   up the wrong session using the dcallno and the peercallno because
   during a transfer (2 IAX channels on the same client/system) the
   same peercallno (from two different asterisks) exist in more than
	one session.
 */
static int forward_match(struct sockaddr_in *sin, short callno, short dcallno, struct iax_session *cur)
{
	if ((cur->transfer.sin_addr.s_addr == sin->sin_addr.s_addr) &&
		(cur->transfer.sin_port == sin->sin_port) && (cur->transferring)) {
		/* We're transferring */
		if (dcallno == cur->callno)
		{
			return 1;
		}
	}

	if ((cur->peeraddr.sin_addr.s_addr == sin->sin_addr.s_addr) &&
		(cur->peeraddr.sin_port == sin->sin_port)) {
		if (dcallno == cur->callno && dcallno != 0)  {
					/* That's us.  Be sure we keep track of the peer call number */
			if (cur->peercallno == 0) {
				cur->peercallno = callno;
			}
			return 1;
		}
	}

	return 0;
}

static int reverse_match(struct sockaddr_in *sin, short callno, struct iax_session *cur)
{
	if ((cur->transfer.sin_addr.s_addr == sin->sin_addr.s_addr) &&
		(cur->transfer.sin_port == sin->sin_port) && (cur->transferring)) {
		/* We're transferring */
		if (callno == cur->peercallno)  {
			return 1;
		}
	}
	if ((cur->peeraddr.sin_addr.s_addr == sin->sin_addr.s_addr) &&
		(cur->peeraddr.sin_port == sin->sin_port)) {
		if (callno == cur->peercallno)  {
			return 1;
		}
	}

	return 0;
}

static struct iax_session *iax_find_session(struct sockaddr_in *sin, 
											short callno, 
											short dcallno,
											int makenew)
{
	struct iax_session *cur;

	iax_mutex_lock(session_mutex); 
	cur = sessions;
	while(cur) {
		if (forward_match(sin, callno, dcallno, cur)) {
			iax_mutex_unlock(session_mutex); 
			return cur;
		}
		cur = cur->next;
	}

	cur = sessions;
	while(cur) {
		if (reverse_match(sin, callno, cur)) {
			iax_mutex_unlock(session_mutex); 
			return cur;
		}
		cur = cur->next;
	}

	iax_mutex_unlock(session_mutex); 

	if (makenew && !dcallno) {
		cur = iax_session_new();
		cur->peercallno = callno;
		cur->peeraddr.sin_addr.s_addr = sin->sin_addr.s_addr;
		cur->peeraddr.sin_port = sin->sin_port;
		cur->peeraddr.sin_family = AF_INET;
		cur->pingid = iax_sched_add(NULL,NULL, send_ping, (void *)cur, 2 * 1000);
		DEBU(G "Making new session, peer callno %d, our callno %d\n", callno, cur->callno);
	} else {
		DEBU(G "No session, peer = %d, us = %d\n", callno, dcallno);
	}

	return cur;	
}

#ifdef EXTREME_DEBUG
static int display_time(int ms)
{
	static int oldms = -1;
	if (oldms < 0) {
		DEBU(G "First measure\n");
		oldms = ms;
		return 0;
	}
	DEBU(G "Time from last frame is %d ms\n", ms - oldms);
	oldms = ms;
	return 0;
}
#endif

#define FUDGE 1

#ifdef NEWJB
/* From chan_iax2/steve davies:  need to get permission from steve or digium, I guess */
static time_in_ms_t unwrap_timestamp(time_in_ms_t ts, time_in_ms_t last)
{
        time_in_ms_t x;

        if ( (ts & 0xFFFF0000) == (last & 0xFFFF0000) ) {
                x = ts - last;
                if (x < -50000) {
                        /* Sudden big jump backwards in timestamp:
                           What likely happened here is that miniframe timestamp has circled but we haven't
                           gotten the update from the main packet.  We'll just pretend that we did, and
                           update the timestamp appropriately. */
                        ts = ( (last & 0xFFFF0000) + 0x10000) | (ts & 0xFFFF);
                                DEBU(G "schedule_delivery: pushed forward timestamp\n");
                }
                if (x > 50000) {
                        /* Sudden apparent big jump forwards in timestamp:
                           What's likely happened is this is an old miniframe belonging to the previous
                           top-16-bit timestamp that has turned up out of order.
                           Adjust the timestamp appropriately. */
                        ts = ( (last & 0xFFFF0000) - 0x10000) | (ts & 0xFFFF);
                                DEBU(G "schedule_delivery: pushed back timestamp\n");
                }
        }
	return ts;
}
#endif


static struct iax_event *schedule_delivery(struct iax_event *e, time_in_ms_t ts, int updatehistory)
{
	/* 
	 * This is the core of the IAX jitterbuffer delivery mechanism: 
	 * Dynamically adjust the jitterbuffer and decide how time_in_ms_t to wait
	 * before delivering the packet.
	 */
#ifndef NEWJB
	int ms, x;
	int drops[MEMORY_SIZE];
	int min, max=0, maxone=0, y, z, match;
#endif

#ifdef EXTREME_DEBUG	
	DEBU(G "[%p] We are at %d, packet is for %d\n", e->session, calc_rxstamp(e->session), ts);
#endif
	
#ifdef VOICE_SMOOTHING
	if (e->etype == IAX_EVENT_VOICE) {
		/* Smooth voices if we know enough about the format */
		switch(e->event.voice.format) {
		case AST_FORMAT_GSM:
			/* GSM frames are 20 ms long, although there could be periods of 
			   silence.  If the time is < 50 ms, assume it ought to be 20 ms */
			if (ts - e->session->lastts < 50)  
				ts = e->session->lastts + 20;
#ifdef EXTREME_DEBUG
			display_time(ts);
#endif
			break;
		default:
			/* Can't do anything */
		}
		e->session->lastts = ts;
	}
#endif

#ifdef NEWJB
	{
	    int type = JB_TYPE_CONTROL;
	    int len = 0;

	    if(e->etype == IAX_EVENT_VOICE) {
	      type = JB_TYPE_VOICE;
	      len = get_sample_cnt(e) / 8;
	    } else if(e->etype == IAX_EVENT_CNG) {
	      type = JB_TYPE_SILENCE;	
	    }

	    /* unwrap timestamp */
	    ts = unwrap_timestamp(ts,e->session->last_ts);

	    /* move forward last_ts if it's greater.  We do this _after_ unwrapping, because
	     * asterisk _still_ has cases where it doesn't send full frames when it ought to */
	    if(ts > e->session->last_ts) {
		e->session->last_ts = ts;		
	    }


	    /* insert into jitterbuffer */
	    /* TODO: Perhaps we could act immediately if it's not droppable and late */
	    if(jb_put(e->session->jb, e, type, len, ts, calc_rxstamp(e->session)) == JB_DROP) {
		  iax_event_free(e);
	    }

	}
#else
	
	/* How many ms from now should this packet be delivered? (remember
	   this can be a negative number, too */
	ms = (int)(calc_rxstamp(e->session) - ts);

	/*  
	   Drop voice frame if timestamp is way off 
	   if ((e->etype == IAX_EVENT_VOICE) && ((ms > 65536) || (ms < -65536))) {
	   DEBU(G "Dropping a voice packet with odd ts (ts = %d; ms = %d)\n", ts, ms);
	   free(e);
	   return NULL;
	   }
	*/

	/* Adjust if voice frame timestamp is off by a step */
	if (ms > 32768) {
		/* What likely happened here is that our counter has circled but we haven't
		   gotten the update from the main packet.  We'll just pretend that we did, and
		   update the timestamp appropriately. */
		ms -= 65536;
	}
	if (ms < -32768) {
		/* We got this packet out of order.  Lets add 65536 to it to bring it into our new
		   time frame */
		ms += 65536;
	}

#if 0	
	printf("rxstamp is %d, timestamp is %d, ms is %d\n", calc_rxstamp(e->session), ts, ms);
#endif
	/* Rotate history queue.  Leading 0's are irrelevant. */
	if (updatehistory) {
	    for (x=0; x < MEMORY_SIZE - 1; x++) 
		    e->session->history[x] = e->session->history[x+1];
	    
	    /* Add new entry for this time */
	    e->session->history[x] = ms;
	}
	
	/* We have to find the maximum and minimum time delay we've had to deliver. */
	min = e->session->history[0];
	for (z=0;z < iax_dropcount + 1; z++) {
		/* We drop the top iax_dropcount entries.  iax_dropcount represents
		   a tradeoff between quality of voice and latency.  3% drop seems to
		   be unnoticable to the client and can significantly improve latency.  
		   We add one more to our droplist, but that's the one we actually use, 
		   and don't drop.  */
		max = -99999999;
		for (x=0;x<MEMORY_SIZE;x++) {
			if (max < e->session->history[x]) {
				/* New candidate value.  Make sure we haven't dropped it. */
				match=0;
				for(y=0;!match && (y<z); y++) 
					match |= (drops[y] == x);
				/* If there is no match, this is our new maximum */
				if (!match) {
					max = e->session->history[x];
					maxone = x;
				}
			}
			if (!z) {
				/* First pass, calcualte our minimum, too */
				if (min > e->session->history[x])
					min = e->session->history[x];
			}
		}
		drops[z] = maxone;
	}
	/* Again, just for reference.  The "jitter buffer" is the max.  The difference
	   is the perceived jitter correction. */
	e->session->jitter = max - min;
	
	/* If the jitter buffer is substantially too large, shrink it, slowly enough
	   that the client won't notice ;-) . */
	if (max < e->session->jitterbuffer - max_extra_jitterbuffer) {
#ifdef EXTREME_DEBUG
		DEBU(G "Shrinking jitterbuffer (target = %d, current = %d...\n", max, e->session->jitterbuffer);
#endif
		e->session->jitterbuffer -= 1;
	}
		
	/* Keep the jitter buffer from becoming unreasonably large */
	if (max > min + max_jitterbuffer) {
		DEBU(G "Constraining jitter buffer (min = %d, max = %d)...\n", min, max);
		max = min + max_jitterbuffer;
	}
	
	/* If the jitter buffer is too small, we immediately grow our buffer to
	   accomodate */
	if (max > e->session->jitterbuffer)
		e->session->jitterbuffer = max;
	
	/* Start with our jitter buffer delay, and subtract the lateness (or earliness).
	   Remember these times are all relative to the first packet, so their absolute
	   values are really irrelevant. */
	ms = e->session->jitterbuffer - ms - IAX_SCHEDULE_FUZZ;
	
	/* If the jitterbuffer is disabled, always deliver immediately */
	if (!iax_use_jitterbuffer)
		ms = 0;
	
	if (ms < 1) {
#ifdef EXTREME_DEBUG
		DEBU(G "Calculated delay is only %d\n", ms);
#endif
		if ((ms > -4) || (e->etype != IAX_EVENT_VOICE)) {
			/* Return the event immediately if it's it's less than 3 milliseconds
			   too late, or if it's not voice (believe me, you don't want to
			   just drop a hangup frame because it's late, or a ping, or some such.
			   That kinda ruins retransmissions too ;-) */
			/* Queue for immediate delivery */
			iax_sched_add(e, NULL, NULL, NULL, 0);
			return NULL;
			//return e;
		}
		DEBU(G "(not so) Silently dropping a packet (ms = %d)\n", ms);
		/* Silently discard this as if it were to be delivered */
		free(e);
		return NULL;
	}
	/* We need this to be delivered in the future, so we use our scheduler */
	iax_sched_add(e, NULL, NULL, NULL, ms);
#ifdef EXTREME_DEBUG
	DEBU(G "Delivering packet in %d ms\n", ms);
#endif
#endif /* NEWJB */
	return NULL;
	
}

static int uncompress_subclass(unsigned char csub)
{
	/* If the SC_LOG flag is set, return 2^csub otherwise csub */
	if (csub & IAX_FLAG_SC_LOG)
		return 1 << (csub & ~IAX_FLAG_SC_LOG & IAX_MAX_SHIFT);
	else
		return csub;
}

static inline char *extract(char *src, char *string)
{
	/* Extract and duplicate what we need from a string */
	char *s, *t;
	s = strstr(src, string);
	if (s) {
		s += strlen(string);
		s = strdup(s);
		/* End at ; */
		t = strchr(s, ';');
		if (t) {
			*t = '\0';
		}
	}
	return s;
		
}

static struct iax_event *iax_header_to_event(struct iax_session *session,
											 struct ast_iax2_full_hdr *fh,
											 int datalen, struct sockaddr_in *sin)
{
	struct iax_event *e;
	struct iax_sched *sch;
	unsigned int ts;
	int subclass = uncompress_subclass(fh->csub);
	time_in_ms_t nowts;
	int updatehistory = 1;
	ts = ntohl(fh->ts);
	/* don't run last_ts backwards; i.e. for retransmits and the like */
	if (ts > session->last_ts &&
	    (fh->type == AST_FRAME_IAX && 
	     subclass != IAX_COMMAND_ACK &&
	     subclass != IAX_COMMAND_PONG &&
	     subclass != IAX_COMMAND_LAGRP)) {
	    session->last_ts = ts;
	}


#ifdef DEBUG_SUPPORT
	iax_showframe(NULL, fh, 1, sin, datalen);
#endif

	/* Get things going with it, timestamp wise, if we
	   haven't already. */

		/* Handle implicit ACKing unless this is an INVAL, and only if this is 
		   from the real peer, not the transfer peer */
		if (!inaddrcmp(sin, &session->peeraddr) && 
			(((subclass != IAX_COMMAND_INVAL)) ||
			(fh->type != AST_FRAME_IAX))) {
			unsigned char x;
			/* XXX This code is not very efficient.  Surely there is a better way which still
			       properly handles boundary conditions? XXX */
			/* First we have to qualify that the ACKed value is within our window */
			for (x=session->rseqno; x != session->oseqno; x++)
				if (fh->iseqno == x)
					break;
			if ((x != session->oseqno) || (session->oseqno == fh->iseqno)) {
				/* The acknowledgement is within our window.  Time to acknowledge everything
				   that it says to */
				for (x=session->rseqno; x != fh->iseqno; x++) {
					/* Ack the packet with the given timestamp */
					DEBU(G "Cancelling transmission of packet %d\n", x);
					iax_mutex_lock(sched_mutex);
					sch = schedq;
					while(sch) {
						if (sch->frame && (sch->frame->session == session) && 
						    (sch->frame->oseqno == x)) 
							sch->frame->retries = -1;
						sch = sch->next;
					}
					iax_mutex_unlock(sched_mutex);
				}
				/* Note how much we've received acknowledgement for */
				session->rseqno = fh->iseqno;
			} else
				DEBU(G "Received iseqno %d not within window %d->%d\n", fh->iseqno, session->rseqno, session->oseqno);
		}

	/* Check where we are */
		if ((ntohs(fh->dcallno) & IAX_FLAG_RETRANS) || (fh->type != AST_FRAME_VOICE))
			updatehistory = 0;
		if ((session->iseqno != fh->oseqno) &&
			(session->iseqno ||
			   ((subclass != IAX_COMMAND_TXREADY) &&
			    (subclass != IAX_COMMAND_TXREL) &&
				(subclass != IAX_COMMAND_TXCNT) &&
				(subclass != IAX_COMMAND_TXACC)) ||
			  (fh->type != AST_FRAME_IAX))) {
			if (
			 ((subclass != IAX_COMMAND_ACK) &&
			  (subclass != IAX_COMMAND_INVAL) &&
			  (subclass != IAX_COMMAND_TXREADY) &&
			  (subclass != IAX_COMMAND_TXREL) &&
			  (subclass != IAX_COMMAND_TXCNT) &&
			  (subclass != IAX_COMMAND_TXACC) &&
			  (subclass != IAX_COMMAND_VNAK)) ||
			  (fh->type != AST_FRAME_IAX)) {
			 	/* If it's not an ACK packet, it's out of order. */
				DEBU(G "Packet arrived out of order (expecting %d, got %d) (frametype = %d, subclass = %d)\n", 
					session->iseqno, fh->oseqno, fh->type, subclass);
				if (session->iseqno > fh->oseqno) {
					/* If we've already seen it, ack it XXX There's a border condition here XXX */
					if ((fh->type != AST_FRAME_IAX) || 
							((subclass != IAX_COMMAND_ACK) && (subclass != IAX_COMMAND_INVAL))) {
						DEBU(G "Acking anyway\n");
						/* XXX Maybe we should handle its ack to us, but then again, it's probably outdated anyway, and if
						   we have anything to send, we'll retransmit and get an ACK back anyway XXX */
						send_command_immediate(session, AST_FRAME_IAX, IAX_COMMAND_ACK, ts, NULL, 0,fh->iseqno);
					}
				} else {
					/* Send a VNAK requesting retransmission */
					iax2_vnak(session);
				}
				return NULL;
			}
		} else {
			/* Increment unless it's an ACK or VNAK */
			if (((subclass != IAX_COMMAND_ACK) &&
			    (subclass != IAX_COMMAND_INVAL) &&
			    (subclass != IAX_COMMAND_TXCNT) &&
			    (subclass != IAX_COMMAND_TXACC) &&
				(subclass != IAX_COMMAND_VNAK)) ||
			    (fh->type != AST_FRAME_IAX))
				session->iseqno++;
		}
			
	e = (struct iax_event *)malloc(sizeof(struct iax_event) + datalen + 1);

	if (e) {
		memset(e, 0, sizeof(struct iax_event) + datalen);
		/* Set etype to some unknown value so do not inavertently
		   sending IAX_EVENT_CONNECT event, which is 0 to application.
		 */
		e->etype = -1;
		e->session = session;
		switch(fh->type) {
		case AST_FRAME_DTMF:
			e->etype = IAX_EVENT_DTMF;
			e->subclass = subclass;
			/* 
			 We want the DTMF event deliver immediately so all I/O can be
			 terminate quickly in an IVR system.
			e = schedule_delivery(e, ts, updatehistory); */
			break;
		case AST_FRAME_VOICE:
			e->etype = IAX_EVENT_VOICE;
			e->subclass = subclass;
			session->voiceformat = subclass;
			if (datalen) {
				memcpy(e->data, fh->iedata, datalen);
				e->datalen = datalen;
			}
			e = schedule_delivery(e, ts, updatehistory);
			break;
		case AST_FRAME_CNG:
			e->etype = IAX_EVENT_CNG;
			e->subclass = subclass;
                        if (datalen) {
                                memcpy(e->data, fh->iedata, datalen);
                                e->datalen = datalen;
                        }
                        e = schedule_delivery(e, ts, updatehistory);
                        break;
		case AST_FRAME_IAX:
			/* Parse IE's */
			if (datalen) {
				memcpy(e->data, fh->iedata, datalen);
				e->datalen = datalen;
			}
			if (iax_parse_ies(&e->ies, e->data, e->datalen)) {
				IAXERROR "Unable to parse IE's");
				free(e);
				e = NULL;
				break;
			}
			switch(subclass) {
			case IAX_COMMAND_NEW:
				/* This is a new, incoming call */
				/* save the capability for validation */
				session->capability = e->ies.capability;
				if (e->ies.codec_prefs) {
					strncpy(session->codec_order, e->ies.codec_prefs, sizeof(session->codec_order));
					session->codec_order_len = (int)strlen(session->codec_order);
				}
				e->etype = IAX_EVENT_CONNECT;
				e = schedule_delivery(e, ts, updatehistory);
				break;
			case IAX_COMMAND_AUTHREQ:
				/* This is a request for a call */
				e->etype = IAX_EVENT_AUTHRQ;
				if (strlen(session->username) && !strcmp(e->ies.username, session->username) &&
					strlen(session->secret)) {
						/* Hey, we already know this one */
						iax_auth_reply(session, session->secret, e->ies.challenge, e->ies.authmethods);
						free(e);
						e = NULL;
						break;
				}
				e = schedule_delivery(e, ts, updatehistory);
				break;
			case IAX_COMMAND_HANGUP:
				e->etype = IAX_EVENT_HANGUP;
				e = schedule_delivery(e, ts, updatehistory);
				break;
			case IAX_COMMAND_INVAL:
				e->etype = IAX_EVENT_HANGUP;
				e = schedule_delivery(e, ts, updatehistory);
				break;
			case IAX_COMMAND_REJECT:
				e->etype = IAX_EVENT_REJECT;
				e = schedule_delivery(e, ts, updatehistory);
				break;
			case IAX_COMMAND_ACK:
				free(e);
				e = NULL;
				break;
			case IAX_COMMAND_LAGRQ:
				/* Pass this atime_in_ms_t for later handling */
				e->etype = IAX_EVENT_LAGRQ;
				e->ts = ts;
				e = schedule_delivery(e, ts, updatehistory);
				break;
			case IAX_COMMAND_POKE:
				e->etype = IAX_EVENT_POKE;
				e->ts = ts;
				break;
			case IAX_COMMAND_PING:
				/* PINGS and PONGS don't get scheduled; */
				e->etype = IAX_EVENT_PING;
				e->ts = ts;
				break;
			case IAX_COMMAND_PONG:
				e->etype = IAX_EVENT_PONG;
				/* track weighted average of ping time */
				session->pingtime = ((2 * session->pingtime) + (calc_timestamp(session,0,NULL) - ts)) / 3;
				session->remote_netstats.jitter = e->ies.rr_jitter;
				session->remote_netstats.losspct = e->ies.rr_loss >> 24;;
				session->remote_netstats.losscnt = e->ies.rr_loss & 0xffffff;
				session->remote_netstats.packets = e->ies.rr_pkts;
				session->remote_netstats.delay = e->ies.rr_delay;
				session->remote_netstats.dropped = e->ies.rr_dropped;
				session->remote_netstats.ooo = e->ies.rr_ooo;
				break;
			case IAX_COMMAND_ACCEPT:
				if (e->ies.format & session->capability) {
					e->etype = IAX_EVENT_ACCEPT;
				}
				else {
					struct iax_ie_data ied;
					/* Although this should not happen, we added this to make sure
					   the negotiation protocol is enforced.
						For lack of event to notify the application we use the defined
						REJECT event.
					 */
					memset(&ied, 0, sizeof(ied));
					iax_ie_append_str(&ied, IAX_IE_CAUSE, (unsigned char *) "Unable to negotiate codec");
					send_command_final(session, AST_FRAME_IAX, IAX_COMMAND_REJECT, 0, ied.buf, ied.pos, -1);
					e->etype = IAX_EVENT_REJECT;
				}
				e = schedule_delivery(e, ts, updatehistory);
				break;
			case IAX_COMMAND_REGREQ:
				e->etype = IAX_EVENT_REGREQ;
				e = schedule_delivery(e, ts, updatehistory);
				break;
			case IAX_COMMAND_REGREL:
				e->etype = IAX_EVENT_REGREQ;
				e = schedule_delivery(e, ts, updatehistory);
				break;
			case IAX_COMMAND_REGACK:
				e->etype = IAX_EVENT_REGACK;
				e = schedule_delivery(e, ts, updatehistory);
				break;
			case IAX_COMMAND_REGAUTH:
				iax_regauth_reply(session, session->secret, e->ies.challenge, e->ies.authmethods);
				free(e);
				e = NULL;
				break;
			case IAX_COMMAND_REGREJ:
				e->etype = IAX_EVENT_REGREJ;
				e = schedule_delivery(e, ts, updatehistory);
				break;
			case IAX_COMMAND_LAGRP:
				e->etype = IAX_EVENT_LAGRP;
				nowts = calc_timestamp(session, 0, NULL);
				e->ts = nowts - ts;
				e->subclass = session->jitter;
				/* Can't call schedule_delivery since timestamp is non-normal */
				break;;
			case IAX_COMMAND_TXREQ:
				/* added check for defensive programming
				 * - in case the asterisk server
				 * or another client does not send the
				 *  apparent transfer address
				 */
				if (e->ies.apparent_addr != NULL) {
				    /* so a full voice frame is sent on the 
				       next voice output */
				    session->svoiceformat = -1;	
				    session->transfer = *e->ies.apparent_addr;
				    session->transfer.sin_family = AF_INET;
				    session->transfercallno = e->ies.callno;
				    session->transferring = TRANSFER_BEGIN;
				    session->transferid = e->ies.transferid;
				    iax_send_txcnt(session);
				}
				free(e);
				e = NULL;
				break;
			case IAX_COMMAND_DPREP:
				/* Received dialplan reply */
				e->etype = IAX_EVENT_DPREP;
				/* Return immediately, makes no sense to schedule */
				break;
			case IAX_COMMAND_TXCNT:
				if (session->transferring)  {
					session->transfer = *sin;
					iax_send_txaccept(session);
				}
				free(e);
				e = NULL;
				break;
			case IAX_COMMAND_TXACC:
				if (session->transferring) {
					stop_transfer(session);
					session->transferring = TRANSFER_READY;
					iax_send_txready(session);
				}
				free(e);
				e = NULL;
				break;
			case IAX_COMMAND_TXREL:
				/* Release the transfer */
			   send_command_immediate(session, AST_FRAME_IAX, IAX_COMMAND_ACK, fh->ts, NULL, 0, fh->iseqno);
				if (session->transferring) {
					complete_transfer(session, e->ies.callno, 1, 0);
				}
				else {
					complete_transfer(session, session->peercallno, 0, 1);
				}
				e->etype = IAX_EVENT_TRANSFER;
				/* notify that asterisk no longer sitting between peers */
				e = schedule_delivery(e, ts, updatehistory);
				break;
			case IAX_COMMAND_QUELCH:
				e->etype = IAX_EVENT_QUELCH;
				session->quelch = 1;
				break;
			case IAX_COMMAND_UNQUELCH:
				e->etype = IAX_EVENT_UNQUELCH;
				session->quelch = 0;
				break;
			case IAX_COMMAND_TXREJ:
				e->etype = IAX_EVENT_TXREJECT;
				iax_handle_txreject(session);
				break;

			case IAX_COMMAND_TXREADY:
				send_command_immediate(session, AST_FRAME_IAX, IAX_COMMAND_ACK, ts, NULL, 0, fh->iseqno);
				if (iax_handle_txready(session)) {
					e->etype = IAX_EVENT_TXREADY;
				}
				else {
					free(e);
					e = NULL;
				}
				break;
			default:
				DEBU(G "Don't know what to do with IAX command %d\n", subclass);
				free(e);
				e = NULL;
			}
			break;
		case AST_FRAME_CONTROL:
			switch(subclass) {
			case AST_CONTROL_ANSWER:
				e->etype = IAX_EVENT_ANSWER;
				e = schedule_delivery(e, ts, updatehistory);
				break;
			case AST_CONTROL_CONGESTION:
			case AST_CONTROL_BUSY:
				e->etype = IAX_EVENT_BUSY;
				e = schedule_delivery(e, ts, updatehistory);
				break;
			case AST_CONTROL_RINGING:
				e->etype = IAX_EVENT_RINGA;
				e = schedule_delivery(e, ts, updatehistory);
				break;
			default:
				DEBU(G "Don't know what to do with AST control %d\n", subclass);
				free(e);
				return NULL;
			}
			break;
		case AST_FRAME_IMAGE:
			e->etype = IAX_EVENT_IMAGE;
			e->subclass = subclass;
			if (datalen) {
				memcpy(e->data, fh->iedata, datalen);
			}
			e = schedule_delivery(e, ts, updatehistory);
			break;

		case AST_FRAME_TEXT:
			e->etype = IAX_EVENT_TEXT;
			if (datalen) {
				memcpy(e->data, fh->iedata, datalen);
			}
			e = schedule_delivery(e, ts, updatehistory);
			break;

		case AST_FRAME_HTML:
			switch(fh->csub) {
			case AST_HTML_LINKURL:
				e->etype = IAX_EVENT_LINKURL;
				/* Fall through */
			case AST_HTML_URL:
				if (e->etype == -1)
					e->etype = IAX_EVENT_URL;
				e->subclass = fh->csub;
				e->datalen = datalen;
				if (datalen) {
					memcpy(e->data, fh->iedata, datalen);
				}
				e = schedule_delivery(e, ts, updatehistory);
				break;
			case AST_HTML_LDCOMPLETE:
				e->etype = IAX_EVENT_LDCOMPLETE;
				e = schedule_delivery(e, ts, updatehistory);
				break;
			case AST_HTML_UNLINK:
				e->etype = IAX_EVENT_UNLINK;
				e = schedule_delivery(e, ts, updatehistory);
				break;
			case AST_HTML_LINKREJECT:
				e->etype = IAX_EVENT_LINKREJECT;
				e = schedule_delivery(e, ts, updatehistory);
				break;
			default:
				DEBU(G "Don't know how to handle HTML type %d frames\n", fh->csub);
				free(e);
				return NULL;
			}
			break;
		default:
			DEBU(G "Don't know what to do with frame type %d\n", fh->type);
			free(e);
			return NULL;
		}
	} else
		DEBU(G "Out of memory\n");
	    
	/* Already ack'd iax frames */
	if (session->aseqno != session->iseqno) {
	    send_command_immediate(session, AST_FRAME_IAX, IAX_COMMAND_ACK, ts, NULL, 0, fh->iseqno);
	}
	return e;
}

static struct iax_event *iax_miniheader_to_event(struct iax_session *session,
						struct ast_iax2_mini_hdr *mh,
						int datalen)
{
	struct iax_event *e;
	time_in_ms_t ts;
	int updatehistory = 1;
	e = (struct iax_event *)malloc(sizeof(struct iax_event) + datalen);
	if (e) {
		if (session->voiceformat > 0) {
			e->etype = IAX_EVENT_VOICE;
			e->session = session;
			e->subclass = session->voiceformat;
			e->datalen = datalen;
			if (datalen) {
#ifdef EXTREME_DEBUG
				DEBU(G "%d bytes of voice\n", datalen);
#endif
				memcpy(e->data, mh->data, datalen);
			}
			ts = (session->last_ts & 0xFFFF0000) | ntohs(mh->ts);
			return schedule_delivery(e, ts, updatehistory);
		} else {
			DEBU(G "No last format received on session %d\n", session->callno);
			free(e);
			e = NULL;
		}
	} else
		DEBU(G "Out of memory\n");
	return e;
}

void iax_destroy(struct iax_session *session)
{
	iax_mutex_lock(session_mutex); 
	destroy_session(session);
	iax_mutex_unlock(session_mutex); 
}

static struct iax_event *iax_net_read(void)
{
	unsigned char buf[65536];
	int res, sinlen;
	struct sockaddr_in sin;

	sinlen = sizeof(sin);
	res = iax_recvfrom(netfd, (char *)buf, sizeof(buf), 0, (struct sockaddr *) &sin, &sinlen);

	if (res < 0) {
#ifdef	WIN32
		if (WSAGetLastError() != WSAEWOULDBLOCK) {
			DEBU(G "Error on read: %d\n", WSAGetLastError());
			IAXERROR "Read error on network socket: %s", strerror(errno));
		}
#else
		if (errno != EAGAIN) {
			DEBU(G "Error on read: %s\n", strerror(errno));
			IAXERROR "Read error on network socket: %s", strerror(errno));
		}
#endif
		return NULL;
	}
	return iax_net_process(buf, res, &sin);
}

static struct iax_session *iax_txcnt_session(struct ast_iax2_full_hdr *fh, int datalen,
				struct sockaddr_in *sin, short callno, short dcallno)
{
	int subclass = uncompress_subclass(fh->csub);
	unsigned char buf[ 65536 ]; /* allocated on stack with same size as iax_net_read() */
	struct iax_ies ies;
	struct iax_session *cur;

	if ((fh->type != AST_FRAME_IAX) || (subclass != IAX_COMMAND_TXCNT) || (!datalen)) {
		return NULL; /* special handling for TXCNT only */
	}
	memcpy(buf, fh->iedata, datalen);	/* prepare local buf for iax_parse_ies() */

	if (iax_parse_ies(&ies, buf, datalen)) {
		return NULL;	/* Unable to parse IE's */
	}
	if (!ies.transferid) {
		return NULL;	/* TXCNT without proper IAX_IE_TRANSFERID */
	}
	iax_mutex_lock(session_mutex); 
	for( cur=sessions; cur; cur=cur->next ) {
		if ((cur->transferring) && (cur->transferid == (int)ies.transferid) &&
		   	(cur->callno == dcallno) && (cur->transfercallno == callno)) {
			/* We're transferring ---
			 * 	skip address/port checking which would fail while remote peer behind symmetric NAT
			 * 	verify transferid instead
			 */
			cur->transfer.sin_addr.s_addr = sin->sin_addr.s_addr;	/* setup for further handling */
			cur->transfer.sin_port = sin->sin_port;
			break;		
		}
	}
	iax_mutex_unlock(session_mutex); 
	return cur;
}

struct iax_event *iax_net_process(unsigned char *buf, int len, struct sockaddr_in *sin)
{
	struct ast_iax2_full_hdr *fh = (struct ast_iax2_full_hdr *)buf;
	struct ast_iax2_mini_hdr *mh = (struct ast_iax2_mini_hdr *)buf;
	struct iax_session *session;
	
	if (ntohs(fh->scallno) & IAX_FLAG_FULL) {
		int subclass = uncompress_subclass(fh->csub);
		int makenew = 0;

		/* Full size header */
		if (len < sizeof(struct ast_iax2_full_hdr)) {
			DEBU(G "Short header received from %s\n", inet_ntoa(sin->sin_addr));
			IAXERROR "Short header received from %s\n", inet_ntoa(sin->sin_addr));
			return NULL;
		}
		/* Only allow it to make new sessions on types where that makes sense */
		if ((fh->type == AST_FRAME_IAX) && ((subclass == IAX_COMMAND_NEW) ||
											(subclass == IAX_COMMAND_POKE) ||
											(subclass == IAX_COMMAND_REGREL) ||
											(subclass == IAX_COMMAND_REGREQ))) {
			makenew = 1;
		}

		/* We have a full header, process appropriately */
		session = iax_find_session(sin, ntohs(fh->scallno) & ~IAX_FLAG_FULL, ntohs(fh->dcallno) & ~IAX_FLAG_RETRANS, makenew);
		if (!session)
			session = iax_txcnt_session(fh, len-sizeof(struct ast_iax2_full_hdr), sin, ntohs(fh->scallno) & ~IAX_FLAG_FULL, ntohs(fh->dcallno) & ~IAX_FLAG_RETRANS);
		if (session) 
			return iax_header_to_event(session, fh, len - sizeof(struct ast_iax2_full_hdr), sin);
		/* if we get here, the frame was invalid for some reason, we should probably send IAX_COMMAND_INVAL (as long as the subclass was not already IAX_COMMAND_INVAL) */
		DEBU(G "No session?\n");
		return NULL;
	} else {
		if (len < sizeof(struct ast_iax2_mini_hdr)) {
			DEBU(G "Short header received from %s\n", inet_ntoa(sin->sin_addr));
			IAXERROR "Short header received from %s\n", inet_ntoa(sin->sin_addr));
			return NULL;
		}
		/* Miniature, voice frame */
		session = iax_find_session(sin, ntohs(fh->scallno), 0, 0);
		if (session)
			return iax_miniheader_to_event(session, mh, len - sizeof(struct ast_iax2_mini_hdr));
		DEBU(G "No session?\n");
		return NULL;
	}
}

static struct iax_sched *iax_get_sched(time_in_ms_t time_in_ms)
{
	struct iax_sched *cur, *prev=NULL;
	iax_mutex_lock(sched_mutex);
	cur = schedq;
	/* Check the event schedule first. */
	while(cur) {
		if (time_in_ms > cur->when) {
			/* Take it out of the event queue */
			if (prev) {
				prev->next = cur->next;
			} else {
				schedq = cur->next;
			}
			iax_mutex_unlock(sched_mutex);
			return cur;
		}
		cur = cur->next;
	}
	iax_mutex_unlock(sched_mutex);
	return NULL;
}

struct iax_event *iax_get_event(int blocking)
{
	struct iax_event *event;
	struct iax_frame *frame;
	time_in_ms_t time_in_ms;
	struct iax_sched *cur;
	
	if (do_shutdown) {
		__iax_shutdown();
		do_shutdown = 0;
		return NULL;
	}
	time_in_ms = current_time_in_ms();
	while((cur = iax_get_sched(time_in_ms))) {

		event = cur->event;
		frame = cur->frame;
		if (event) {

			/* See if this is an event we need to handle */
			event = handle_event(event);
			if (event) {
				free(cur);
				return event;
			}
		} else if(frame) {
			/* It's a frame, transmit it and schedule a retry */
			if (frame->retries < 0) {
				/* It's been acked.  No need to send it.   Destroy the old
				   frame. If final, destroy the session. */
				if (frame->final) {
					iax_mutex_lock(session_mutex); 
					destroy_session(frame->session);
					iax_mutex_unlock(session_mutex); 
				}
				if (frame->data)
					free(frame->data);
				free(frame);
			} else if (frame->retries == 0) {
				if (frame->transfer) {
					/* Send a transfer reject since we weren't able to connect */
					iax_send_txrej(frame->session);
					if (frame->data)
						free(frame->data);
					free(frame);
					free(cur);
					break;
				} else {
					/* We haven't been able to get an ACK on this packet. If a 
					   final frame, destroy the session, otherwise, pass up timeout */
					if (frame->final) {
						iax_mutex_lock(session_mutex); 
						destroy_session(frame->session);
						iax_mutex_unlock(session_mutex); 
						if (frame->data)
							free(frame->data);
						free(frame);
					} else {
						event = (struct iax_event *)malloc(sizeof(struct iax_event));
						if (event) {
							event->etype = IAX_EVENT_TIMEOUT;
							event->session = frame->session;
							if (frame->data)
								free(frame->data);
							free(frame);
							free(cur);
							return handle_event(event);
						}
					}
				}
			} else {
				struct ast_iax2_full_hdr *fh;
				/* Decrement remaining retries */
				frame->retries--;
				/* Multiply next retry time by 4, not above MAX_RETRY_TIME though */
				frame->retrytime *= 4;
				/* Keep under 1000 ms if this is a transfer packet */
				if (!frame->transfer) {
					if (frame->retrytime > MAX_RETRY_TIME)
						frame->retrytime = MAX_RETRY_TIME;
				} else if (frame->retrytime > 1000)
					frame->retrytime = 1000;
				fh = (struct ast_iax2_full_hdr *)(frame->data);
				fh->dcallno = htons(IAX_FLAG_RETRANS | frame->dcallno);
				iax_xmit_frame(frame);
				/* Schedule another retransmission */
				DEBU(G "Scheduling retransmission %d\n", frame->retries);
				iax_sched_add(NULL, frame, NULL, NULL, frame->retrytime);
			}
		} else if (cur->func) {
		    cur->func(cur->arg);
		}
		free(cur);
	}

#ifdef NEWJB
	/* get jitterbuffer-scheduled events */
	{
	    struct iax_session *cur;
	    jb_frame frame;
		iax_mutex_lock(session_mutex); 
	    for(cur=sessions; cur; cur=cur->next) {
			int ret;
			time_in_ms_t now;
			time_in_ms_t next;

			now = time_in_ms - cur->rxcore;
			if(now > (next = jb_next(cur->jb))) {
				ret = jb_get(cur->jb,&frame,now,get_interp_len(cur->voiceformat));
				switch(ret) {
				case JB_OK:
					//			    if(frame.type == JB_TYPE_VOICE && next + 20 != jb_next(cur->jb)) fprintf(stderr, "NEXT %ld is not %ld+20!\n", jb_next(cur->jb), next);
					event = frame.data;
					event = handle_event(event);
					if (event) {
						iax_mutex_unlock(session_mutex);
						return event;
					}
					break;
				case JB_INTERP:
					//			    if(next + 20 != jb_next(cur->jb)) fprintf(stderr, "NEXT %ld is not %ld+20!\n", jb_next(cur->jb), next);
					/* create an interpolation frame */
					//fprintf(stderr, "Making Interpolation frame\n");
					event = (struct iax_event *)malloc(sizeof(struct iax_event));
					if (event) {
						event->etype    = IAX_EVENT_VOICE;
						event->subclass = cur->voiceformat;
						event->ts	    = now; /* XXX: ??? applications probably ignore this anyway */
						event->session  = cur;
						event->datalen  = 0;
						event = handle_event(event);
						if(event) {
							iax_mutex_unlock(session_mutex);
							return event;
						}
					}
					break;
				case JB_DROP:
					//			    if(next != jb_next(cur->jb)) fprintf(stderr, "NEXT %ld is not next %ld!\n", jb_next(cur->jb), next);
					iax_event_free(frame.data);
					break;
				case JB_NOFRAME:
				case JB_EMPTY:
					/* do nothing */
					break;
				default:
					/* shouldn't happen */
					break;
				}
			}
	    }
		iax_mutex_unlock(session_mutex);
	}

#endif
	/* Now look for networking events */
	if (blocking) {
		/* Block until there is data if desired */
		fd_set fds;
		time_in_ms_t nextEventTime;
		
		FD_ZERO(&fds);
		FD_SET(netfd, &fds);

		nextEventTime = iax_time_to_next_event(); 
		if(nextEventTime < 0 && blocking > 1) {
			nextEventTime = blocking;
		}
		if(nextEventTime < 0) 
			select(netfd + 1, &fds, NULL, NULL, NULL);
		else 
		{ 
			struct timeval nextEvent; 

			nextEvent.tv_sec = (long)(nextEventTime / 1000); 
			nextEvent.tv_usec = (long)((nextEventTime % 1000) * 1000);

			select(netfd + 1, &fds, NULL, NULL, &nextEvent); 
		} 

	}
	event = iax_net_read();
	
	return handle_event(event);
}

struct sockaddr_in iax_get_peer_addr(struct iax_session *session)
{
	return session->peeraddr;
}

char *iax_get_peer_ip(struct iax_session *session)
{
	return inet_ntoa(session->peeraddr.sin_addr);
}

char *iax_event_get_apparent_ip(struct iax_event *event)
{
	return inet_ntoa(event->ies.apparent_addr->sin_addr);
}

void iax_session_destroy(struct iax_session **session) 
{
	iax_mutex_lock(session_mutex); 
	destroy_session(*session);
	*session = NULL;
	iax_mutex_unlock(session_mutex); 
}

void iax_event_free(struct iax_event *event)
{
	/* 
	   We gave the user a chance to play with the session now we need to destroy it 
	   if you are not calling this function on every event you read you are now going
	   to leak sessions as well as events!
	*/
	switch(event->etype) {
	case IAX_EVENT_REJECT:
	case IAX_EVENT_HANGUP:
		/* Destroy this session -- it's no longer valid */
		if (event->session) { /* maybe the user did it already */
			iax_mutex_lock(session_mutex); 
			destroy_session(event->session);
			iax_mutex_unlock(session_mutex); 
		}
		break;
	}
	free(event);
}

int iax_get_fd(void) 
{
	/* Return our network file descriptor.  The client can select on this (probably with other
	   things, or can add it to a network add sort of gtk_input_add for example */
	return netfd;
}

int iax_quelch_moh(struct iax_session *session, int MOH)
{
	
	struct iax_ie_data ied;			//IE Data Structure (Stuff To Send)
	memset(&ied, 0, sizeof(ied));	
	
	// You can't quelch the quelched
	if (session->quelch == 1)
		return -1;
		
	if (MOH) {
		iax_ie_append(&ied, IAX_IE_MUSICONHOLD);
		session->transfer_moh = 1;
	}
		
	return send_command(session, AST_FRAME_IAX, IAX_COMMAND_QUELCH, 0, ied.buf, ied.pos, -1);
}
