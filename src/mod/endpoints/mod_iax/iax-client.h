/*
 * Asterisk -- A telephony toolkit for Linux.
 *
 * Implementation of Inter-Asterisk eXchange
 * 
 * Copyright (C) 1999, Mark Spencer
 *
 * Mark Spencer <markster@linux-support.net>
 *
 * This program is free software, distributed under the terms of
 * the GNU Lesser General Public License (LGPL)
 */
 
#ifndef _ASTERISK_IAX_CLIENT_H
#define _ASTERISK_IAX_CLIENT_H

#ifdef WIN32_TIME_GET_TIME
#include <Mmsystem.h>
#endif

#if defined(_MSC_VER)
/* disable zero-sized array in struct/union warning */
#pragma warning(disable:4200)
#endif

#if !defined(LINUX) && !defined(__NetBSD__)
#define socklen_t int
#endif

#include "frame.h"
#include "iax2.h"
#include "iax2-parser.h"

#define MAXSTRLEN 80

#define IAX_AUTHMETHOD_PLAINTEXT 	IAX_AUTH_PLAINTEXT
#define IAX_AUTHMETHOD_MD5 			IAX_AUTH_MD5

extern char iax_errstr[];

struct iax_session;


#define IAX_EVENT_CONNECT 0			/* Connect a new call */
#define IAX_EVENT_ACCEPT  1			/* Accept a call */
#define IAX_EVENT_HANGUP  2			/* Hang up a call */
#define IAX_EVENT_REJECT  3			/* Rejected call */
#define IAX_EVENT_VOICE   4			/* Voice Data */
#define IAX_EVENT_DTMF    5			/* A DTMF Tone */
#define IAX_EVENT_TIMEOUT 6			/* Connection timeout...  session will be
									   a pointer to free()'d memory! */
#define IAX_EVENT_LAGRQ   7			/* Lag request -- Internal use only */
#define IAX_EVENT_LAGRP   8			/* Lag Measurement.  See event.lag */
#define IAX_EVENT_RINGA	  9			/* Announce we/they are ringing */
#define IAX_EVENT_PING	  10		/* Ping -- internal use only */
#define IAX_EVENT_PONG	  11		/* Pong -- internal use only */
#define IAX_EVENT_BUSY	  12		/* Report a line busy */
#define IAX_EVENT_ANSWER  13		/* Answer the line */

#define IAX_EVENT_IMAGE   14		/* Send/Receive an image */
#define IAX_EVENT_AUTHRQ  15		/* Authentication request */
#define IAX_EVENT_AUTHRP  16		/* Authentication reply */

#define IAX_EVENT_REGREQ  17		/* Registration request */
#define IAX_EVENT_REGACK  18		/* Registration reply */
#define IAX_EVENT_URL	  19		/* URL received */
#define IAX_EVENT_LDCOMPLETE 20		/* URL loading complete */

#define IAX_EVENT_TRANSFER	21		/* Transfer has taken place */

#define IAX_EVENT_DPREQ		22		/* Dialplan request */
#define IAX_EVENT_DPREP		23		/* Dialplan reply */
#define IAX_EVENT_DIAL		24		/* Dial on a TBD call */

#define IAX_EVENT_QUELCH	25		/* Quelch Audio */
#define IAX_EVENT_UNQUELCH	26		/* Unquelch Audio */

#define IAX_EVENT_UNLINK	27		/* Unlink */
#define IAX_EVENT_LINKREJECT	28		/* Link Rejection */
#define IAX_EVENT_TEXT		29		/* Text Frame :-) */
#define IAX_EVENT_REGREJ  30		/* Registration reply */
#define IAX_EVENT_LINKURL	31		/* Unlink */
#define IAX_EVENT_CNG	32		/* Comfort-noise (almost silence) */
#define IAX_EVENT_POKE   33

/* moved from iax.c to support attended transfer */
#define IAX_EVENT_REREQUEST	999
#define IAX_EVENT_TXREPLY	1000
#define IAX_EVENT_TXREJECT	1001
#define IAX_EVENT_TXACCEPT  1002
#define IAX_EVENT_TXREADY	1003

#define IAX_SCHEDULE_FUZZ 0			/* ms of fuzz to drop */

#ifdef WIN32
#if defined(_MSC_VER)
typedef int (__stdcall *sendto_t)(SOCKET, const char *, int, int, const struct sockaddr *, int);
typedef int (__stdcall *recvfrom_t)(SOCKET, char *, int, int, struct sockaddr *, int *);
#else
typedef int PASCAL (*sendto_t)(SOCKET, const char *, int, int, const struct sockaddr *, int);
typedef int PASCAL (*recvfrom_t)(SOCKET, char *buf, int len, int flags, struct sockaddr *from, int *fromlen);
#endif
#else
typedef int (*sendto_t)(int, const void *, size_t, int, const struct sockaddr *, socklen_t);
typedef int (*recvfrom_t)(int s, void *buf, size_t len, int flags, struct sockaddr *from, socklen_t *fromlen);
#endif

struct iax_event {
	int etype;						/* Type of event */
	int subclass;					/* Subclass data (event specific) */
	time_in_ms_t ts;				/* Timestamp */
	struct iax_session *session;	/* Applicable session */
	int datalen;					/* Length of raw data */
	struct iax_ies ies;				/* IE's for IAX2 frames */
	unsigned char data[];			/* Raw data if applicable */
};

#if defined(__cplusplus)
extern "C"
{
#endif

/* All functions return 0 on success and -1 on failure unless otherwise
   specified */

/* Called to initialize IAX structures and sockets.  Returns actual
   portnumber (which it will try preferred portno first, but if not
   take what it can get */
extern int iax_init(char *ip, int preferredportno);

extern int iax_shutdown(void);

/* Get filedescriptor for IAX to use with select or gtk_input_add */
extern int iax_get_fd(void);

/* Find out how many milliseconds until the next scheduled event */
extern time_in_ms_t iax_time_to_next_event(void);

/* Generate a new IAX session */
extern struct iax_session *iax_session_new(void);

/* Return exactly one iax event (if there is one pending).  If blocking is
   non-zero, IAX will block until some event is received */
extern struct iax_event *iax_get_event(int blocking);


extern int iax_auth_reply(struct iax_session *session, char *password, 
						char *challenge, int methods);

/* Free an event */
extern void iax_event_free(struct iax_event *event);

struct sockaddr_in;

/* Front ends for sending events */
extern int iax_send_dtmf(struct iax_session *session, char digit);
extern int iax_send_voice(struct iax_session *session, int format, unsigned char *data, int datalen, int samples);
extern int iax_send_cng(struct iax_session *session, int level, unsigned char *data, int datalen);
extern int iax_send_image(struct iax_session *session, int format, unsigned char *data, int datalen);
extern int iax_send_url(struct iax_session *session, char *url, int link);
extern int iax_send_text(struct iax_session *session, char *text);
extern int iax_send_ping(struct iax_session *session);
extern int iax_load_complete(struct iax_session *session);
extern int iax_reject(struct iax_session *session, const char *reason);
int iax_reject_registration(struct iax_session *session, char *reason);
int iax_ack_registration(struct iax_session *session);
int iax_auth_registration(struct iax_session *session);
extern int iax_busy(struct iax_session *session);
extern int iax_congestion(struct iax_session *session);
extern int iax_hangup(struct iax_session *session, char *byemsg);
extern int iax_call(struct iax_session *session, const char *cidnum, const char *cidname, char *ich, char *lang, int wait, int format, int capability);
extern int iax_accept(struct iax_session *session, int format);
extern int iax_answer(struct iax_session *session);
extern int iax_sendurl(struct iax_session *session, char *url);
extern int iax_send_unlink(struct iax_session *session);
extern int iax_send_link_reject(struct iax_session *session);
extern int iax_ring_announce(struct iax_session *session);
extern struct sockaddr_in iax_get_peer_addr(struct iax_session *session);
extern int iax_register(struct iax_session *session, char *hostname, char *peer, char *secret, int refresh);
extern int iax_lag_request(struct iax_session *session);
extern int iax_dial(struct iax_session *session, char *number);	/* Dial on a TBD call */
extern int iax_dialplan_request(struct iax_session *session, char *number);	/* Request dialplan status for number */
extern int iax_quelch(struct iax_session *session);
extern int iax_unquelch(struct iax_session * session);
extern int iax_transfer(struct iax_session *session, char *number);  
extern int iax_quelch_moh(struct iax_session *session, int MOH);

extern void iax_destroy(struct iax_session  * session);

extern void iax_enable_debug(void);
extern void iax_disable_debug(void);

/* For attended transfer, application create a new session,
 * make a call on the new session.
 * On answer of the new session, call iax_setup_transfer and wait for
 * IAX_EVENT_TXREADY when both sides are completed succefully or
 * IAX_EVENT_TXREJECT for either side.
 * If there are music on hold the it will be stopped by this library.
 */
extern int iax_setup_transfer(struct iax_session *s0, struct iax_session *s1);

struct iax_netstat {
	time_in_ms_t jitter;
	int losspct;
	int losscnt;
	int packets;
	time_in_ms_t delay;
	int dropped;
	int ooo;
};
/* fills in rtt, and an iax_netstat structure for each of local/remote directions of call */
extern int iax_get_netstats(struct iax_session *s, time_in_ms_t *rtt, struct iax_netstat *local, struct iax_netstat *remote);


extern void iax_set_private(struct iax_session *s, void *pvt);
extern void *iax_get_private(struct iax_session *s);
extern void iax_set_sendto(struct iax_session *s, sendto_t sendto);

/* to use application networking instead of internal, set call this instead of iax_init,
 * and pass in sendto and recvfrom replacements.  blocking reads may not be implemented */
extern void iax_set_networking(sendto_t st, recvfrom_t rf);

/* destroy an iax session */
extern void iax_session_destroy(struct iax_session **session);

/* Handle externally received frames */
struct iax_event *iax_net_process(unsigned char *buf, int len, struct sockaddr_in *sin);
extern unsigned int iax_session_get_capability(struct iax_session *s);
extern char iax_pref_codec_add(struct iax_session *session, unsigned int format);
extern void iax_pref_codec_del(struct iax_session *session, unsigned int format);
extern int iax_pref_codec_get(struct iax_session *session, unsigned int *array, int len);

/* Fine tune jitterbuffer */
extern void iax_set_jb_target_extra( long value );

extern char *iax_get_peer_ip(struct iax_session *session);
extern char *iax_event_get_apparent_ip(struct iax_event *event);
extern void iax_set_samplerate(struct iax_session *session, unsigned short samplemask);

#if defined(__cplusplus)
}
#endif

#endif /* _ASTERISK_IAX_CLIENT_H */
