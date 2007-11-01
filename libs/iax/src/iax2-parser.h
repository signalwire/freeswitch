/*
 * Asterisk -- A telephony toolkit for Linux.
 *
 * Implementation of Inter-Asterisk eXchange
 * 
 * Copyright (C) 2003, Digium
 *
 * Mark Spencer <markster@digium.com>
 *
 * This program is free software, distributed under the terms of
 * the GNU Lesser (Library) General Public License
 */
 
#ifndef _IAX2_PARSER_H
#define _IAX2_PARSER_H

struct iax_ies {
	char *called_number;
	char *calling_number;
	char *calling_ani;
	char *calling_name;
	int calling_ton;
	int calling_tns;
	int calling_pres;
	char *called_context;
	char *username;
	char *password;
	unsigned int capability;
	unsigned int format;
	char *codec_prefs;
	char *language;
	int version;
	unsigned short adsicpe;
	char *dnid;
	char *rdnis;
	unsigned int authmethods;
	char *challenge;
	char *md5_result;
	char *rsa_result;
	struct sockaddr_in *apparent_addr;
	unsigned short refresh;
	unsigned short dpstatus;
	unsigned short callno;
	char *cause;
	unsigned char causecode;
	unsigned char iax_unknown;
	int msgcount;
	int autoanswer;
	int musiconhold;
	unsigned int transferid;
	unsigned int datetime;
	char *devicetype;
	char *serviceident;
	int firmwarever;
	unsigned int fwdesc;
	unsigned char *fwdata;
	unsigned char fwdatalen;
	unsigned int provver;
	unsigned short samprate;
	unsigned int provverpres;
	unsigned int rr_jitter;
	unsigned int rr_loss;
	unsigned int rr_pkts;
	unsigned short rr_delay;
	unsigned int rr_dropped;
	unsigned int rr_ooo;
};

#define DIRECTION_INGRESS 1
#define DIRECTION_OUTGRESS 2

struct iax_frame {
#ifdef LIBIAX
	struct iax_session *session;
	struct iax_event *event;
#endif

	/* /Our/ call number */
	unsigned short callno;
	/* /Their/ call number */
	unsigned short dcallno;
	/* Start of raw frame (outgoing only) */
	void *data;
	/* Length of frame (outgoing only) */
	int datalen;
	/* How many retries so far? */
	int retries;
	/* Outgoing relative timestamp (ms) */
	time_in_ms_t ts;
	/* How long to wait before retrying */
	time_in_ms_t retrytime;
	/* Are we received out of order?  */
	int outoforder;
	/* Have we been sent at all yet? */
	int sentyet;
	/* Outgoing Packet sequence number */
	int oseqno;
	/* Next expected incoming packet sequence number */
	int iseqno;
	/* Non-zero if should be sent to transfer peer */
	int transfer;
	/* Non-zero if this is the final message */
	int final;
	/* Ingress or outgres */
	int direction;
	/* Retransmission ID */
	int retrans;
	/* Easy linking */
	struct iax_frame *next;
	struct iax_frame *prev;
	/* Actual, isolated frame header */
	struct ast_frame af;
	unsigned char unused[AST_FRIENDLY_OFFSET];
	unsigned char afdata[];	/* Data for frame */
};

struct iax_ie_data {
	unsigned char buf[1024];
	int pos;
};

/* Choose a different function for output */
extern void iax_set_output(void (*output)(const char *data));
/* Choose a different function for errors */
extern void iax_set_error(void (*output)(const char *data));
extern void iax_showframe(struct iax_frame *f, struct ast_iax2_full_hdr *fhi, int rx, struct sockaddr_in *sin, int datalen);

extern const char *iax_ie2str(int ie);

extern int iax_ie_append_raw(struct iax_ie_data *ied, unsigned char ie, const void *data, int datalen);
extern int iax_ie_append_addr(struct iax_ie_data *ied, unsigned char ie, struct sockaddr_in *sin);
extern int iax_ie_append_int(struct iax_ie_data *ied, unsigned char ie, unsigned int value);
extern int iax_ie_append_short(struct iax_ie_data *ied, unsigned char ie, unsigned short value);
extern int iax_ie_append_str(struct iax_ie_data *ied, unsigned char ie, const unsigned char *str);
extern int iax_ie_append_byte(struct iax_ie_data *ied, unsigned char ie, unsigned char dat);
extern int iax_ie_append(struct iax_ie_data *ied, unsigned char ie);
extern int iax_parse_ies(struct iax_ies *ies, unsigned char *data, int datalen);

extern int iax_get_frames(void);
extern int iax_get_iframes(void);
extern int iax_get_oframes(void);
extern void iax_frame_wrap(struct iax_frame *fr, struct ast_frame *f);
extern struct iax_frame *iax_frame_new(int direction, int datalen);
extern void iax_frame_free(struct iax_frame *fr);
#endif
