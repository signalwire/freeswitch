/*
 * Copyright (c) 2010, Moises Silva <moy@sangoma.com>
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

#include <libpri.h>
#include <poll.h>
#include "private/ftdm_core.h"

#define PRI_SPAN(p) (((p) >> 8) & 0xff)
#define PRI_CHANNEL(p) ((p) & 0xff)

#define PRITAP_NETWORK_ANSWER 0x1

typedef enum {
	PRITAP_RUNNING = (1 << 0),
	PRITAP_MASTER = (1 << 1),
} pritap_flags_t;

typedef enum {
	PRITAP_MIX_BOTH = 0,
	PRITAP_MIX_PEER,
	PRITAP_MIX_SELF,
} pritap_mix_mode_t;

typedef struct {
	void *callref;
	ftdm_number_t callingnum;
	ftdm_number_t callingani;
	ftdm_number_t callednum;
	ftdm_channel_t *fchan;
	char callingname[80];
	uint8_t proceeding;
	uint8_t inuse;
} passive_call_t;

typedef enum pritap_iface {
	PRITAP_IFACE_UNKNOWN = 0,
	PRITAP_IFACE_CPE = 1,
	PRITAP_IFACE_NET = 2,
} pritap_iface_t;

typedef struct pritap {
	int32_t flags;
	struct pri *pri;
	int debug;
	pritap_mix_mode_t mixaudio;
	ftdm_channel_t *dchan;
	ftdm_span_t *span;
	ftdm_span_t *peerspan;
	ftdm_mutex_t *pcalls_lock;
	passive_call_t pcalls[FTDM_MAX_CHANNELS_PHYSICAL_SPAN];
	pritap_iface_t iface;
} pritap_t;

static FIO_IO_UNLOAD_FUNCTION(ftdm_pritap_unload)
{
	return FTDM_SUCCESS;
}

static FIO_CHANNEL_GET_SIG_STATUS_FUNCTION(pritap_get_channel_sig_status)
{
	*status = FTDM_SIG_STATE_UP;
	return FTDM_SUCCESS;
}

static FIO_SPAN_GET_SIG_STATUS_FUNCTION(pritap_get_span_sig_status)
{
	*status = FTDM_SIG_STATE_UP;
	return FTDM_SUCCESS;
}


static FIO_CHANNEL_OUTGOING_CALL_FUNCTION(pritap_outgoing_call)
{
	ftdm_log(FTDM_LOG_ERROR, "Cannot dial on PRI tapping line!\n");
	return FTDM_FAIL;
}

static void s_pri_error(struct pri *pri, char *s)
{
	pritap_t *pritap = pri_get_userdata(pri);

	if (pritap && pritap->dchan) {
		ftdm_log_chan(pritap->dchan, FTDM_LOG_ERROR, "%s", s);
	} else {
		ftdm_log(FTDM_LOG_ERROR, "%s", s);
	}
}

static void s_pri_message(struct pri *pri, char *s)
{
	pritap_t *pritap = pri_get_userdata(pri);

	if (pritap && pritap->dchan) {
		ftdm_log_chan(pritap->dchan, FTDM_LOG_DEBUG, "%s", s);
	} else {
		ftdm_log(FTDM_LOG_DEBUG, "%s", s);
	}
}

#define PRI_DEBUG_Q921_ALL (PRI_DEBUG_Q921_RAW | PRI_DEBUG_Q921_DUMP | PRI_DEBUG_Q921_STATE)
#define PRI_DEBUG_Q931_ALL (PRI_DEBUG_Q931_DUMP | PRI_DEBUG_Q931_STATE | PRI_DEBUG_Q931_ANOMALY)

static int parse_debug(const char *in)
{
	int flags = 0;

	if (!in) {
		return 0;
	}

	if (!strcmp(in, "none")) {
		return 0;
	}

	if (!strcmp(in, "all")) {
		return PRI_DEBUG_ALL;
	}

	if (strstr(in, "q921_all")) {
		flags |= PRI_DEBUG_Q921_ALL;
	}

	if (strstr(in, "q921_raw")) {
		flags |= PRI_DEBUG_Q921_RAW;
	}

	if (strstr(in, "q921_dump")) {
		flags |= PRI_DEBUG_Q921_DUMP;
	}

	if (strstr(in, "q921_state")) {
		flags |= PRI_DEBUG_Q921_STATE;
	}

	if (strstr(in, "config")) {
		flags |= PRI_DEBUG_CONFIG;
	}

	if (strstr(in, "q931_all")) {
		flags |= PRI_DEBUG_Q931_ALL;
	}

	if (strstr(in, "q931_dump")) {
		flags |= PRI_DEBUG_Q931_DUMP;
	}

	if (strstr(in, "q931_state")) {
		flags |= PRI_DEBUG_Q931_STATE;
	}

	if (strstr(in, "q931_anomaly")) {
		flags |= PRI_DEBUG_Q931_ANOMALY;
	}

	if (strstr(in, "apdu")) {
		flags |= PRI_DEBUG_APDU;
	}

	if (strstr(in, "aoc")) {
		flags |= PRI_DEBUG_AOC;
	}

	return flags;
}

static ftdm_io_interface_t ftdm_pritap_interface;

static ftdm_status_t ftdm_pritap_start(ftdm_span_t *span);

static FIO_API_FUNCTION(ftdm_pritap_api)
{
	char *mycmd = NULL, *argv[10] = { 0 };
	int argc = 0;
	
	if (data) {
		mycmd = ftdm_strdup(data);
		argc = ftdm_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}

	if (argc > 2) {
		if (!strcasecmp(argv[0], "debug")) {
			ftdm_span_t *span = NULL;

			if (ftdm_span_find_by_name(argv[1], &span) == FTDM_SUCCESS) {
				pritap_t *pritap = span->signal_data;
				if (span->start != ftdm_pritap_start) {
					stream->write_function(stream, "%s: -ERR invalid span.\n", __FILE__);
					goto done;
				}

				pri_set_debug(pritap->pri, parse_debug(argv[2]));				
				stream->write_function(stream, "%s: +OK debug set.\n", __FILE__);
				goto done;
			} else {
				stream->write_function(stream, "%s: -ERR invalid span.\n", __FILE__);
				goto done;
			}
		}

	}

	stream->write_function(stream, "%s: -ERR invalid command.\n", __FILE__);
	
 done:

	ftdm_safe_free(mycmd);

	return FTDM_SUCCESS;
}

static FIO_IO_LOAD_FUNCTION(ftdm_pritap_io_init)
{
	memset(&ftdm_pritap_interface, 0, sizeof(ftdm_pritap_interface));

	ftdm_pritap_interface.name = "pritap";
	ftdm_pritap_interface.api = ftdm_pritap_api;

	*fio = &ftdm_pritap_interface;

	return FTDM_SUCCESS;
}

static FIO_SIG_LOAD_FUNCTION(ftdm_pritap_init)
{
	pri_set_error(s_pri_error);
	pri_set_message(s_pri_message);
	return FTDM_SUCCESS;
}

static ftdm_state_map_t pritap_state_map = {
	{
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_DOWN, FTDM_CHANNEL_STATE_END},
			{FTDM_CHANNEL_STATE_RING, FTDM_CHANNEL_STATE_END}
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_RING, FTDM_CHANNEL_STATE_END},
			{FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_TERMINATING, FTDM_CHANNEL_STATE_PROGRESS, FTDM_CHANNEL_STATE_PROGRESS_MEDIA, FTDM_CHANNEL_STATE_UP, FTDM_CHANNEL_STATE_END}
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_END},
			{FTDM_CHANNEL_STATE_TERMINATING, FTDM_CHANNEL_STATE_END},
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_TERMINATING, FTDM_CHANNEL_STATE_END},
			{FTDM_CHANNEL_STATE_DOWN, FTDM_CHANNEL_STATE_END},
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_PROGRESS, FTDM_CHANNEL_STATE_END},
			{FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_TERMINATING, FTDM_CHANNEL_STATE_PROGRESS_MEDIA, FTDM_CHANNEL_STATE_UP, FTDM_CHANNEL_STATE_END},
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_PROGRESS_MEDIA, FTDM_CHANNEL_STATE_END},
			{FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_TERMINATING, FTDM_CHANNEL_STATE_UP, FTDM_CHANNEL_STATE_END},
		},
		{
			ZSD_INBOUND,
			ZSM_UNACCEPTABLE,
			{FTDM_CHANNEL_STATE_UP, FTDM_CHANNEL_STATE_END},
			{FTDM_CHANNEL_STATE_HANGUP, FTDM_CHANNEL_STATE_TERMINATING, FTDM_CHANNEL_STATE_END},
		},
		
	}
};

#define PRITAP_GET_INTERFACE(iface) iface == PRITAP_IFACE_CPE ? "CPE" :  \
                                    iface == PRITAP_IFACE_NET ? "NET" : "UNKNOWN"
static ftdm_status_t state_advance(ftdm_channel_t *ftdmchan)
{
	ftdm_status_t status;
	ftdm_sigmsg_t sig;
	pritap_t *pritap = ftdmchan->span->signal_data;
	pritap_t *peer_pritap = pritap->peerspan->signal_data;
	
	ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "processing state %s\n", ftdm_channel_state2str(ftdmchan->state));

	memset(&sig, 0, sizeof(sig));
	sig.chan_id = ftdmchan->chan_id;
	sig.span_id = ftdmchan->span_id;
	sig.channel = ftdmchan;

	ftdm_channel_complete_state(ftdmchan);

	switch (ftdmchan->state) {
	case FTDM_CHANNEL_STATE_DOWN:
		{			
			ftdm_channel_t *fchan = ftdmchan;

			/* Destroy the peer data first */
			if (fchan->call_data) {
				ftdm_channel_t *peerchan = fchan->call_data;
				ftdm_channel_t *pchan = peerchan;

				ftdm_channel_lock(peerchan);

				pchan->call_data = NULL;
				pchan->pflags = 0;
				ftdm_channel_close(&pchan);

				ftdm_channel_unlock(peerchan);
			} else {
				ftdm_log_chan_msg(ftdmchan, FTDM_LOG_CRIT, "No call data?\n");
			}

			ftdmchan->call_data = NULL;
			ftdmchan->pflags = 0;
			ftdm_channel_close(&fchan);
		}
		break;

	case FTDM_CHANNEL_STATE_PROGRESS:
	case FTDM_CHANNEL_STATE_PROGRESS_MEDIA:
	case FTDM_CHANNEL_STATE_HANGUP:
		break;

	case FTDM_CHANNEL_STATE_UP:
		{
			if (ftdm_test_pflag(ftdmchan, PRITAP_NETWORK_ANSWER)) {
				ftdm_clear_pflag(ftdmchan, PRITAP_NETWORK_ANSWER);
				sig.event_id = FTDM_SIGEVENT_UP;
				ftdm_span_send_signal(ftdmchan->span, &sig);
			}
		}
		break;

	case FTDM_CHANNEL_STATE_RING:
		{
			sig.event_id = FTDM_SIGEVENT_START;
			/* The ring interface (where the setup was received) is the peer, since we RING the channel
			 * where PROCEED/PROGRESS is received */
			ftdm_sigmsg_add_var(&sig, "pritap_ring_interface", PRITAP_GET_INTERFACE(peer_pritap->iface));
			if ((status = ftdm_span_send_signal(ftdmchan->span, &sig) != FTDM_SUCCESS)) {
				ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_HANGUP);
			}
		}
		break;

	case FTDM_CHANNEL_STATE_TERMINATING:
		{
			if (ftdmchan->last_state != FTDM_CHANNEL_STATE_HANGUP) {
				sig.event_id = FTDM_SIGEVENT_STOP;
				status = ftdm_span_send_signal(ftdmchan->span, &sig);
			}
			ftdm_set_state_locked(ftdmchan, FTDM_CHANNEL_STATE_DOWN);
		}
		break;

	default:
		{
			ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "ignoring state change from %s to %s\n", ftdm_channel_state2str(ftdmchan->last_state), ftdm_channel_state2str(ftdmchan->state));
		}
		break;
	}

	return FTDM_SUCCESS;
}

static __inline__ void pritap_check_state(ftdm_span_t *span)
{
	if (ftdm_test_flag(span, FTDM_SPAN_STATE_CHANGE)) {
		uint32_t j;
		ftdm_clear_flag_locked(span, FTDM_SPAN_STATE_CHANGE);
		for(j = 1; j <= span->chan_count; j++) {
			ftdm_channel_lock(span->channels[j]);
			ftdm_channel_advance_states(span->channels[j]);
			ftdm_channel_unlock(span->channels[j]);
		}
	}
}

static int pri_io_read(struct pri *pri, void *buf, int buflen)
{
	int res;
	ftdm_status_t zst;
	pritap_t *pritap = pri_get_userdata(pri);
	ftdm_size_t len = buflen;

	if ((zst = ftdm_channel_read(pritap->dchan, buf, &len)) != FTDM_SUCCESS) {
		if (zst == FTDM_FAIL) {
			ftdm_log(FTDM_LOG_CRIT, "span %d D channel read fail! [%s]\n", pritap->span->span_id, pritap->dchan->last_error);
		} else {
			ftdm_log(FTDM_LOG_CRIT, "span %d D channel read timeout!\n", pritap->span->span_id);
		}
		return -1;
	}

	res = (int)len;

	memset(&((unsigned char*)buf)[res],0,2);
	res += 2;

	/* libpri passive q921 raw dump does not work for all frames */
	if (pritap->debug & PRI_DEBUG_Q921_RAW) {
		char hbuf[2048] = { 0 };

		print_hex_bytes(buf, len, hbuf, sizeof(hbuf));
		ftdm_log_chan(pritap->dchan, FTDM_LOG_DEBUG, "READ %"FTDM_SIZE_FMT"\n%s\n", len, hbuf);
	}
	return res;
}

static int pri_io_write(struct pri *pri, void *buf, int buflen)
{
	pritap_t *pritap = pri_get_userdata(pri);
	ftdm_size_t len = buflen - 2;

	/* libpri passive q921 raw dump does not work for all frames */
	if (pritap->debug & PRI_DEBUG_Q921_RAW) {
		char hbuf[2048] = { 0 };

		print_hex_bytes(buf, len, hbuf, sizeof(hbuf));
		ftdm_log_chan(pritap->dchan, FTDM_LOG_DEBUG, "WRITE %"FTDM_SIZE_FMT"\n%s\n", len, hbuf);
	}

	if (ftdm_channel_write(pritap->dchan, buf, buflen, &len) != FTDM_SUCCESS) {
		ftdm_log(FTDM_LOG_CRIT, "span %d D channel write failed! [%s]\n", pritap->span->span_id, pritap->dchan->last_error);
		return -1; 
	}   

	return (int)buflen;
}

static int tap_pri_get_crv(struct pri *ctrl, q931_call *call)
{
	int callmode = 0;
	int crv = pri_get_crv(ctrl, call, &callmode);
	crv <<= 3;
	crv |= (callmode & 0x7);
	return crv;
}

static passive_call_t *tap_pri_get_pcall_bycrv(pritap_t *pritap, int crv)
{
	int i;
	int tstcrv;

	ftdm_mutex_lock(pritap->pcalls_lock);

	for (i = 0; i < ftdm_array_len(pritap->pcalls); i++) {
		tstcrv = pritap->pcalls[i].callref ? tap_pri_get_crv(pritap->pri, pritap->pcalls[i].callref) : 0;
		if (pritap->pcalls[i].callref && tstcrv == crv) {
			if (pritap->pcalls[i].inuse) {
				ftdm_mutex_unlock(pritap->pcalls_lock);
				return &pritap->pcalls[i];
			}
			/* This just means the crv is being re-used in another call before this one was destroyed */
			ftdm_log(FTDM_LOG_DEBUG, "Found crv %d in slot %d of span %s with call %p but is no longer in use\n",
					crv, i, pritap->span->name, pritap->pcalls[i].callref);
		}
	}

	ftdm_log(FTDM_LOG_DEBUG, "crv %d was not found active in span %s\n", crv, pritap->span->name);

	ftdm_mutex_unlock(pritap->pcalls_lock);

	return NULL;
}

/*
 * This is a tricky function with some side effects, some explanation needed ...
 *
 * The libpri stack process HDLC frames, then finds Q921 frames and Q931 events, each time
 * it finds a new Q931 event, checks if the crv of that event matches a known call in the internal
 * list found in the PRI control block (for us, one control block per span), if it does not find
 * the call, allocates a new one and then sends the event up to the user (us, ftmod_pritap in this case)
 *
 * The user is then expected to destroy the call when done with it (on hangup), but things get tricky here
 * because in ftmod_pritap we do not destroy the call right away to be sure we only destroy it when no one
 * else needs that pointer, therefore we decide to delay the destruction of the call pointer until later
 * when a new call comes which triggers the garbage collecting code in this function
 *
 * Now, what happens if a new call arrives right away with the same crv than the last call? the pri stack
 * does *not* allocate a new call pointer because is still a known call and we must therefore re-use the
 * same call pointer
 *
 * This function accepts a pointer to a callref, even a NULL one. When callref is NULL we search for an
 * available slot so the caller of this function can use it to store a new callref pointer. In the process
 * we also scan for slots that still have a callref pointer but are no longer in use (inuse=0) and we
 * destroy that callref and clear the slot (memset). The trick is, we only do this if the callref to
 * be garbage collected is NOT the one provided by the parameter callref, of course! otherwise we may
 * be freeing a pointer to a callref for a new call that used an old (recycled) callref!
 */
static passive_call_t *tap_pri_get_pcall(pritap_t *pritap, void *callref)
{
	int i;
	int crv;

	ftdm_mutex_lock(pritap->pcalls_lock);

	for (i = 0; i < ftdm_array_len(pritap->pcalls); i++) {
		/* If this slot has a call reference
		 * and it is different than the *callref provided to us
		 * and is no longer in use,
		 * then it is time to garbage collect it ... */
		if (pritap->pcalls[i].callref  && callref != pritap->pcalls[i].callref && !pritap->pcalls[i].inuse) {
			crv = tap_pri_get_crv(pritap->pri, pritap->pcalls[i].callref);
			/* garbage collection */
			ftdm_log(FTDM_LOG_DEBUG, "Garbage collecting callref %d/%p from span %s in slot %d\n", 
					crv, pritap->pcalls[i].callref, pritap->span->name, i);
			pri_passive_destroycall(pritap->pri, pritap->pcalls[i].callref);
			memset(&pritap->pcalls[i], 0, sizeof(pritap->pcalls[0]));
		}
		if (callref == pritap->pcalls[i].callref) {
			if (callref == NULL) {
				pritap->pcalls[i].inuse = 1;
				ftdm_log(FTDM_LOG_DEBUG, "Enabling callref slot %d in span %s\n", i, pritap->span->name);
			} else if (!pritap->pcalls[i].inuse) {
				crv = tap_pri_get_crv(pritap->pri, callref);
				ftdm_log(FTDM_LOG_DEBUG, "Recyclying callref slot %d in span %s for callref %d/%p\n",
						i, pritap->span->name, crv, callref);
				memset(&pritap->pcalls[i], 0, sizeof(pritap->pcalls[0]));
				pritap->pcalls[i].callref = callref;
				pritap->pcalls[i].inuse = 1;
			}

			ftdm_mutex_unlock(pritap->pcalls_lock);

			return &pritap->pcalls[i];
		}
	}

	ftdm_mutex_unlock(pritap->pcalls_lock);

	return NULL;
}

static void tap_pri_put_pcall(pritap_t *pritap, void *callref)
{
	int i;
	int crv;
	int tstcrv;

	if (!callref) {
		ftdm_log(FTDM_LOG_ERROR, "Cannot put pcall for null callref in span %s\n", pritap->span->name);
		return;
	}

	ftdm_mutex_lock(pritap->pcalls_lock);

	crv = tap_pri_get_crv(pritap->pri, callref);
	for (i = 0; i < ftdm_array_len(pritap->pcalls); i++) {
		if (!pritap->pcalls[i].callref) {
			continue;
		}
		tstcrv = tap_pri_get_crv(pritap->pri, pritap->pcalls[i].callref);
		if (tstcrv == crv) {
			if (pritap->pcalls[i].inuse) {
				ftdm_log(FTDM_LOG_DEBUG, "releasing slot %d in span %s used by callref %d/%p\n", i,
						pritap->span->name, crv, pritap->pcalls[i].callref);
				pritap->pcalls[i].inuse = 0;
			}
		}
	}

	ftdm_mutex_unlock(pritap->pcalls_lock);
}

static __inline__ ftdm_channel_t *tap_pri_get_fchan(pritap_t *pritap, passive_call_t *pcall, int channel)
{
	ftdm_channel_t *fchan = NULL;
	int err = 0;
	int chanpos = PRI_CHANNEL(channel);
	if (!chanpos || chanpos > pritap->span->chan_count) {
		ftdm_log(FTDM_LOG_CRIT, "Invalid pri tap channel %d requested in span %s\n", channel, pritap->span->name);
		return NULL;
	}

	fchan = pritap->span->channels[PRI_CHANNEL(channel)];

	ftdm_channel_lock(fchan);

	if (ftdm_test_flag(fchan, FTDM_CHANNEL_INUSE)) {
		ftdm_log(FTDM_LOG_ERROR, "Channel %d requested in span %s is already in use!\n", channel, pritap->span->name);
		err = 1;
		goto done;
	}

	if (ftdm_channel_open_chan(fchan) != FTDM_SUCCESS) {
		ftdm_log(FTDM_LOG_ERROR, "Could not open tap channel %d requested in span %s\n", channel, pritap->span->name);
		err = 1;
		goto done;
	}

	memset(&fchan->caller_data, 0, sizeof(fchan->caller_data));

	ftdm_set_string(fchan->caller_data.cid_num.digits, pcall->callingnum.digits);
	if (!ftdm_strlen_zero(pcall->callingname)) {
		ftdm_set_string(fchan->caller_data.cid_name, pcall->callingname);
	} else {
		ftdm_set_string(fchan->caller_data.cid_name, pcall->callingnum.digits);
	}
	ftdm_set_string(fchan->caller_data.ani.digits, pcall->callingani.digits);
	ftdm_set_string(fchan->caller_data.dnis.digits, pcall->callednum.digits);

done:
	if (fchan) {
		ftdm_channel_unlock(fchan);
	}

	if (err) {
		return NULL;
	}

	return fchan;
}

static void handle_pri_passive_event(pritap_t *pritap, pri_event *e)
{
	passive_call_t *pcall = NULL;
	passive_call_t *peerpcall = NULL;
	ftdm_channel_t *fchan = NULL;
	ftdm_channel_t *peerfchan = NULL;
	int layer1, transcap = 0;
	int crv = 0;
	pritap_t *peertap = pritap->peerspan->signal_data;

	switch (e->e) {

	case PRI_EVENT_RING:
		/* we cannot use ftdm_channel_t because we still dont know which channel will be used 
		 * (ie, flexible channel was requested), thus, we need our own list of call references */
		crv = tap_pri_get_crv(pritap->pri, e->ring.call);
		ftdm_log(FTDM_LOG_DEBUG, "Ring on channel %s:%d:%d with callref %d\n", 
				pritap->span->name, PRI_SPAN(e->ring.channel), PRI_CHANNEL(e->ring.channel), crv);
		pcall = tap_pri_get_pcall_bycrv(pritap, crv);
		if (pcall) {
			ftdm_log(FTDM_LOG_WARNING, "There is a call with callref %d already, ignoring duplicated ring event\n", crv);
			break;
		}

		/* Try to get a recycled call (ie, e->ring.call is a call that the PRI stack allocated previously and then
		 * re-used for the next RING event because we did not destroy it fast enough) */
		pcall = tap_pri_get_pcall(pritap, e->ring.call);
		if (!pcall) {
			/* ok so the call is really not known to us, let's get a new one */
			pcall = tap_pri_get_pcall(pritap, NULL);
			if (!pcall) {
				ftdm_log(FTDM_LOG_ERROR, "Failed to get a free passive PRI call slot for callref %d, this is a bug!\n", crv);
				break;
			}
		}
		pcall->callref = e->ring.call;
		ftdm_set_string(pcall->callingnum.digits, e->ring.callingnum);
		ftdm_set_string(pcall->callingani.digits, e->ring.callingani);
		ftdm_set_string(pcall->callednum.digits, e->ring.callednum);
		ftdm_set_string(pcall->callingname, e->ring.callingname);
		break;

	case PRI_EVENT_PROGRESS:
		crv = tap_pri_get_crv(pritap->pri, e->proceeding.call);
		ftdm_log(FTDM_LOG_DEBUG, "Progress on channel %s:%d:%d with callref %d\n", 
				pritap->span->name, PRI_SPAN(e->proceeding.channel), PRI_CHANNEL(e->proceeding.channel), crv);
		break;

	case PRI_EVENT_PROCEEDING:
		crv = tap_pri_get_crv(pritap->pri, e->proceeding.call);
		/* at this point we should know the real b chan that will be used and can therefore proceed to notify about the call, but
		 * only if a couple of call tests are passed first */
		ftdm_log(FTDM_LOG_DEBUG, "Proceeding on channel %s:%d:%d with callref %d\n", 
				pritap->span->name, PRI_SPAN(e->proceeding.channel), PRI_CHANNEL(e->proceeding.channel), crv);

		/* check that we already know about this call in the peer PRI (which was the one receiving the PRI_EVENT_RING event) */
		if (!(pcall = tap_pri_get_pcall_bycrv(peertap, crv))) {
			ftdm_log(FTDM_LOG_DEBUG, 
				"ignoring proceeding in channel %s:%d:%d for callref %d since we don't know about it\n",
				pritap->span->name, PRI_SPAN(e->proceeding.channel), PRI_CHANNEL(e->proceeding.channel), crv);
			break;
		}
		if (pcall->proceeding) {
			ftdm_log(FTDM_LOG_DEBUG, "Ignoring duplicated proceeding with callref %d\n", crv);
			break;
		}
		pcall->proceeding = 1;

		/* This call should not be known to this PRI yet ... */
		if ((peerpcall = tap_pri_get_pcall_bycrv(pritap, crv))) {
			ftdm_log(FTDM_LOG_ERROR,
				"ignoring proceeding in channel %s:%d:%d for callref %d, dup???\n",
				pritap->span->name, PRI_SPAN(e->proceeding.channel), PRI_CHANNEL(e->proceeding.channel), crv);
			break;
		}

		/* Check if the call pointer is being recycled */
		peerpcall = tap_pri_get_pcall(pritap, e->proceeding.call);
		if (!peerpcall) {
			peerpcall = tap_pri_get_pcall(pritap, NULL);
			if (!peerpcall) {
				ftdm_log(FTDM_LOG_ERROR, "Failed to get a free peer PRI passive call slot for callref %d in span %s, this is a bug!\n", 
						crv, pritap->span->name);
				break;
			}
			peerpcall->callref = e->proceeding.call;
		}

		/* check that the layer 1 and trans capability are supported */
		layer1 = pri_get_layer1(peertap->pri, pcall->callref);
		transcap = pri_get_transcap(peertap->pri, pcall->callref);

		if (PRI_LAYER_1_ULAW != layer1 && PRI_LAYER_1_ALAW != layer1) {
			ftdm_log(FTDM_LOG_NOTICE, "Not monitoring callref %d with unsupported layer 1 format %d\n", crv, layer1);
			break;
		}
		
		if (transcap != PRI_TRANS_CAP_SPEECH && transcap != PRI_TRANS_CAP_3_1K_AUDIO && transcap != PRI_TRANS_CAP_7K_AUDIO) {
			ftdm_log(FTDM_LOG_NOTICE, "Not monitoring callref %d with unsupported capability %d\n", crv, transcap);
			break;
		}

		fchan = tap_pri_get_fchan(pritap, pcall, e->proceeding.channel);
		if (!fchan) {
			ftdm_log(FTDM_LOG_ERROR, "Proceeding requested on odd/unavailable channel %s:%d:%d for callref %d\n",
				pritap->span->name, PRI_SPAN(e->proceeding.channel), PRI_CHANNEL(e->proceeding.channel), crv);
			break;
		}

		peerfchan = tap_pri_get_fchan(peertap, pcall, e->proceeding.channel);
		if (!peerfchan) {
			ftdm_log(FTDM_LOG_ERROR, "Proceeding requested on odd/unavailable channel %s:%d:%d for callref %d\n",
				peertap->span->name, PRI_SPAN(e->proceeding.channel), PRI_CHANNEL(e->proceeding.channel), crv);
			break;
		}
		pcall->fchan = fchan;
		peerpcall->fchan = fchan;

		ftdm_log_chan(fchan, FTDM_LOG_NOTICE, "Starting new tapped call with callref %d\n", crv);

		ftdm_channel_lock(fchan);
		fchan->call_data = peerfchan;
		ftdm_set_state(fchan, FTDM_CHANNEL_STATE_RING);
		ftdm_channel_unlock(fchan);

		ftdm_channel_lock(peerfchan);
		peerfchan->call_data = fchan;
		ftdm_channel_unlock(peerfchan);

		break;

	case PRI_EVENT_ANSWER:
		crv = tap_pri_get_crv(pritap->pri, e->answer.call);
		ftdm_log(FTDM_LOG_DEBUG, "Answer on channel %s:%d:%d with callref %d\n", 
				pritap->span->name, PRI_SPAN(e->answer.channel), PRI_CHANNEL(e->answer.channel), crv);
		if (!(pcall = tap_pri_get_pcall_bycrv(pritap, crv))) {
			ftdm_log(FTDM_LOG_DEBUG, 
				"ignoring answer in channel %s:%d:%d for callref %d since we don't know about it\n",
				pritap->span->name, PRI_SPAN(e->answer.channel), PRI_CHANNEL(e->proceeding.channel), crv);
			break;
		}
		if (!pcall->fchan) {
			ftdm_log(FTDM_LOG_ERROR,
				"Received answer in channel %s:%d:%d for callref %d but we never got a channel\n",
				pritap->span->name, PRI_SPAN(e->answer.channel), PRI_CHANNEL(e->answer.channel), crv);
			break;
		}
		ftdm_channel_lock(pcall->fchan);
		ftdm_log_chan(pcall->fchan, FTDM_LOG_NOTICE, "Tapped call was answered in state %s\n", ftdm_channel_state2str(pcall->fchan->state));
		ftdm_set_pflag(pcall->fchan, PRITAP_NETWORK_ANSWER);
		ftdm_set_state(pcall->fchan, FTDM_CHANNEL_STATE_UP);
		ftdm_channel_unlock(pcall->fchan);
		break;

	case PRI_EVENT_HANGUP_REQ:
		crv = tap_pri_get_crv(pritap->pri, e->hangup.call);

		ftdm_log(FTDM_LOG_DEBUG, "Hangup on channel %s:%d:%d with callref %d\n",
				pritap->span->name, PRI_SPAN(e->hangup.channel), PRI_CHANNEL(e->hangup.channel), crv);

		if (!(pcall = tap_pri_get_pcall_bycrv(pritap, crv))) {
			ftdm_log(FTDM_LOG_DEBUG, 
				"ignoring hangup in channel %s:%d:%d for callref %d since we don't know about it",
				pritap->span->name, PRI_SPAN(e->hangup.channel), PRI_CHANNEL(e->hangup.channel), crv);
			break;
		}

		if (pcall->fchan) {
			fchan = pcall->fchan;
			ftdm_channel_lock(fchan);
			if (fchan->state < FTDM_CHANNEL_STATE_TERMINATING) {
				ftdm_set_state(fchan, FTDM_CHANNEL_STATE_TERMINATING);
			}
			pcall->fchan = NULL; /* after this event we're not supposed to need to do anything with the channel anymore */
			ftdm_channel_unlock(fchan);
		}

		tap_pri_put_pcall(pritap, e->hangup.call);
		tap_pri_put_pcall(peertap, e->hangup.call);
		break;

	case PRI_EVENT_HANGUP_ACK:
		crv = tap_pri_get_crv(pritap->pri, e->hangup.call);
		ftdm_log(FTDM_LOG_DEBUG, "Hangup ack on channel %s:%d:%d with callref %d\n", 
				pritap->span->name, PRI_SPAN(e->hangup.channel), PRI_CHANNEL(e->hangup.channel), crv);
		tap_pri_put_pcall(pritap, e->hangup.call);
		tap_pri_put_pcall(peertap, e->hangup.call);
		break;

	default:
		ftdm_log(FTDM_LOG_DEBUG, "Ignoring passive event %s on span %s\n", pri_event2str(e->gen.e), pritap->span->name);
		break;

	}
}

static void *ftdm_pritap_run(ftdm_thread_t *me, void *obj)
{
	ftdm_span_t *span = (ftdm_span_t *) obj;
	ftdm_span_t *peer = NULL;
	pritap_t *pritap = span->signal_data;
	pritap_t *p_pritap = NULL;
	pri_event *event = NULL;
	struct pollfd dpoll[2];
	int rc = 0;

	ftdm_log(FTDM_LOG_DEBUG, "Tapping PRI thread started on span %s\n", span->name);
	
	pritap->span = span;

	ftdm_set_flag(span, FTDM_SPAN_IN_THREAD);
	
	if (ftdm_channel_open(span->span_id, pritap->dchan->chan_id, &pritap->dchan) != FTDM_SUCCESS) {
		ftdm_log(FTDM_LOG_ERROR, "Failed to open D-channel for span %s\n", span->name);
		goto done;
	}

	if ((pritap->pri = pri_new_cb(pritap->dchan->sockfd, PRI_NETWORK, PRI_SWITCH_NI2, pri_io_read, pri_io_write, pritap))){
		pri_set_debug(pritap->pri, pritap->debug);
	} else {
		ftdm_log(FTDM_LOG_CRIT, "Failed to create tapping PRI\n");
		goto done;
	}

	/* The last span starting runs the show ...
	 * This simplifies locking and avoid races by having multiple threads for a single tapped link
	 * Since both threads really handle a single tapped link there is no benefit on multi-threading, just complications ... */
	peer = pritap->peerspan;
	p_pritap = peer->signal_data;
	if (!ftdm_test_flag(pritap, PRITAP_MASTER)) {
		ftdm_log(FTDM_LOG_DEBUG, "Running dummy thread on span %s\n", span->name);
		while (ftdm_running() && !ftdm_test_flag(span, FTDM_SPAN_STOP_THREAD)) {
			poll(NULL, 0, 100);
		}
	} else {
		memset(&dpoll, 0, sizeof(dpoll));
		dpoll[0].fd = pritap->dchan->sockfd;
		dpoll[1].fd = p_pritap->dchan->sockfd;

		ftdm_log(FTDM_LOG_DEBUG, "Master tapping thread on span %s (fd1=%d, fd2=%d)\n", span->name,
				pritap->dchan->sockfd, p_pritap->dchan->sockfd);

		while (ftdm_running() && !ftdm_test_flag(span, FTDM_SPAN_STOP_THREAD)) {

			pritap_check_state(span);
			pritap_check_state(peer);

			dpoll[0].revents = 0;
			dpoll[0].events = POLLIN;

			dpoll[1].revents = 0;
			dpoll[1].events = POLLIN;

			rc = poll(&dpoll[0], 2, 10);

			if (rc < 0) {
				if (errno == EINTR) {
					ftdm_log(FTDM_LOG_DEBUG, "D-channel waiting interrupted, continuing ...\n");
					continue;
				}
				ftdm_log(FTDM_LOG_ERROR, "poll failed: %s\n", strerror(errno));
				continue;
			}

			pri_schedule_run(pritap->pri);
			pri_schedule_run(p_pritap->pri);

			pritap_check_state(span);
			pritap_check_state(peer);

			if (rc) {
				if (dpoll[0].revents & POLLIN) {
					event = pri_read_event(pritap->pri);
					if (event) {
						handle_pri_passive_event(pritap, event);
						pritap_check_state(span);
					}
				}

				if (dpoll[1].revents & POLLIN) {
					event = pri_read_event(p_pritap->pri);
					if (event) {
						handle_pri_passive_event(p_pritap, event);
						pritap_check_state(peer);
					}
				}
			}

		}
	}


done:
	ftdm_log(FTDM_LOG_DEBUG, "Tapping PRI thread ended on span %s\n", span->name);

	ftdm_clear_flag(span, FTDM_SPAN_IN_THREAD);
	ftdm_clear_flag(pritap, PRITAP_RUNNING);
	ftdm_clear_flag(pritap, PRITAP_MASTER);

	return NULL;
}

static ftdm_status_t ftdm_pritap_stop(ftdm_span_t *span)
{
	pritap_t *pritap = span->signal_data;

	if (!ftdm_test_flag(pritap, PRITAP_RUNNING)) {
		return FTDM_FAIL;
	}

	ftdm_set_flag(span, FTDM_SPAN_STOP_THREAD);

	while (ftdm_test_flag(span, FTDM_SPAN_IN_THREAD)) {
		ftdm_sleep(100);
	}

	ftdm_mutex_destroy(&pritap->pcalls_lock);
	return FTDM_SUCCESS;
}

static ftdm_status_t ftdm_pritap_sig_read(ftdm_channel_t *ftdmchan, void *data, ftdm_size_t size)
{
	ftdm_status_t status;
	fio_codec_t codec_func;
	ftdm_channel_t *peerchan = ftdmchan->call_data;
	pritap_t *pritap = ftdmchan->span->signal_data;
	int16_t chanbuf[size];
	int16_t peerbuf[size];
	int16_t mixedbuf[size];
	int i = 0;
	ftdm_size_t sizeread = size;

	if (!FTDM_IS_VOICE_CHANNEL(ftdmchan) || !ftdmchan->call_data) {
		return FTDM_SUCCESS;
	}

	if (pritap->mixaudio == PRITAP_MIX_SELF) {
		return FTDM_SUCCESS;
	}

	if (pritap->mixaudio == PRITAP_MIX_PEER) {
		/* start out by clearing the self audio to make sure we don't return audio we were
		 * not supposed to in an error condition  */
		memset(data, FTDM_SILENCE_VALUE(ftdmchan), size);
	}

	if (ftdmchan->native_codec != peerchan->native_codec) {
		ftdm_log_chan(ftdmchan, FTDM_LOG_CRIT, "Invalid peer channel with format %d, ours = %d\n", 
				peerchan->native_codec, ftdmchan->native_codec);
		return FTDM_FAIL;
	}

	memcpy(chanbuf, data, size);
	status = peerchan->fio->read(peerchan, peerbuf, &sizeread);
	if (status != FTDM_SUCCESS) {
		ftdm_log_chan_msg(ftdmchan, FTDM_LOG_ERROR, "Failed to read from peer channel!\n");
		return FTDM_FAIL;
	}
	if (sizeread != size) {
		ftdm_log_chan(ftdmchan, FTDM_LOG_ERROR, "read from peer channel only %"FTDM_SIZE_FMT" bytes!\n", sizeread);
		return FTDM_FAIL;
	}

	if (pritap->mixaudio == PRITAP_MIX_PEER) {
		/* only the peer audio is requested */
		memcpy(data, peerbuf, size);
		return FTDM_SUCCESS;
	}

	codec_func = peerchan->native_codec == FTDM_CODEC_ULAW ? fio_ulaw2slin : peerchan->native_codec == FTDM_CODEC_ALAW ? fio_alaw2slin : NULL;
	if (codec_func) {
		sizeread = size;
		codec_func(chanbuf, sizeof(chanbuf), &sizeread);
		sizeread = size;
		codec_func(peerbuf, sizeof(peerbuf), &sizeread);
	}

	for (i = 0; i < size; i++) {
		mixedbuf[i] = ftdm_saturated_add(chanbuf[i], peerbuf[i]);
	}

	codec_func = peerchan->native_codec == FTDM_CODEC_ULAW ? fio_slin2ulaw : peerchan->native_codec == FTDM_CODEC_ALAW ? fio_slin2alaw : NULL;
	if (codec_func) {
		size = sizeof(mixedbuf);
		codec_func(mixedbuf, size, &size);
	}
	memcpy(data, mixedbuf, size);
	return FTDM_SUCCESS;
}

static ftdm_status_t ftdm_pritap_start(ftdm_span_t *span)
{
	ftdm_status_t ret;
	pritap_t *pritap = span->signal_data;
	pritap_t *p_pritap = pritap->peerspan->signal_data;

	if (ftdm_test_flag(pritap, PRITAP_RUNNING)) {
		return FTDM_FAIL;
	}

	ftdm_mutex_create(&pritap->pcalls_lock);

	ftdm_clear_flag(span, FTDM_SPAN_STOP_THREAD);
	ftdm_clear_flag(span, FTDM_SPAN_IN_THREAD);

	ftdm_set_flag(pritap, PRITAP_RUNNING);
	if (p_pritap && ftdm_test_flag(p_pritap, PRITAP_RUNNING)) {
		/* our peer already started, we're the master */
		ftdm_set_flag(pritap, PRITAP_MASTER);
	}
	ret = ftdm_thread_create_detached(ftdm_pritap_run, span);

	if (ret != FTDM_SUCCESS) {
		return ret;
	}

	return ret;
}

static FIO_CONFIGURE_SPAN_SIGNALING_FUNCTION(ftdm_pritap_configure_span)
{
	uint32_t i;
	const char *var, *val;
	const char *debug = NULL;
	pritap_mix_mode_t mixaudio = PRITAP_MIX_BOTH;
	ftdm_channel_t *dchan = NULL;
	pritap_t *pritap = NULL;
	ftdm_span_t *peerspan = NULL;
	pritap_iface_t iface = PRITAP_IFACE_UNKNOWN;
	unsigned paramindex = 0;

	if (span->trunk_type >= FTDM_TRUNK_NONE) {
		ftdm_log(FTDM_LOG_WARNING, "Invalid trunk type '%s' defaulting to T1.\n", ftdm_trunk_type2str(span->trunk_type));
		span->trunk_type = FTDM_TRUNK_T1;
	}
	
	for (i = 1; i <= span->chan_count; i++) {
		if (span->channels[i]->type == FTDM_CHAN_TYPE_DQ921) {
			dchan = span->channels[i];
		}
	}

	if (!dchan) {
		ftdm_log(FTDM_LOG_ERROR, "No d-channel specified in freetdm.conf!\n");
		return FTDM_FAIL;
	}
	
	for (paramindex = 0; ftdm_parameters[paramindex].var; paramindex++) {
		var = ftdm_parameters[paramindex].var;
		val = ftdm_parameters[paramindex].val;
		ftdm_log(FTDM_LOG_DEBUG, "Tapping PRI key=value, %s=%s\n", var, val);

		if (!strcasecmp(var, "debug")) {
			debug = val;
		} else if (!strcasecmp(var, "mixaudio")) {
			if (ftdm_true(val) || !strcasecmp(val, "both")) {
				ftdm_log(FTDM_LOG_DEBUG, "Setting mix audio mode to 'both' for span %s\n", span->name);
				mixaudio = PRITAP_MIX_BOTH;
			} else if (!strcasecmp(val, "peer")) {
				ftdm_log(FTDM_LOG_DEBUG, "Setting mix audio mode to 'peer' for span %s\n", span->name);
				mixaudio = PRITAP_MIX_PEER;
			} else {
				ftdm_log(FTDM_LOG_DEBUG, "Setting mix audio mode to 'self' for span %s\n", span->name);
				mixaudio = PRITAP_MIX_SELF;
			}
		} else if (!strcasecmp(var, "interface")) {
			if (!strcasecmp(val, "cpe")) {
				iface = PRITAP_IFACE_CPE;
			} else if (!strcasecmp(val, "net")) {
				iface = PRITAP_IFACE_NET;
			} else {
				ftdm_log(FTDM_LOG_WARNING, "Ignoring invalid tapping interface type %s\n", val);
			}
		} else if (!strcasecmp(var, "peerspan")) {
			if (ftdm_span_find_by_name(val, &peerspan) != FTDM_SUCCESS) {
				ftdm_log(FTDM_LOG_ERROR, "Invalid tapping peer span %s\n", val);
				break;
			}
		} else {
			ftdm_log(FTDM_LOG_ERROR,  "Unknown pri tapping parameter [%s]", var);
		}
	}

	if (!peerspan) {
		ftdm_log(FTDM_LOG_ERROR, "No valid peerspan was specified!\n");
		return FTDM_FAIL;
	}
    
	pritap = ftdm_calloc(1, sizeof(*pritap));
	if (!pritap) {
		return FTDM_FAIL;
	}

	pritap->debug = parse_debug(debug);
	pritap->dchan = dchan;
	pritap->peerspan = peerspan;
	pritap->mixaudio = mixaudio;
	pritap->iface = iface;

	span->start = ftdm_pritap_start;
	span->stop = ftdm_pritap_stop;
	span->sig_read = ftdm_pritap_sig_read;
	span->signal_cb = sig_cb;
	
	span->signal_data = pritap;
	span->signal_type = FTDM_SIGTYPE_ISDN;
	span->outgoing_call = pritap_outgoing_call;

	span->get_channel_sig_status = pritap_get_channel_sig_status;
	span->get_span_sig_status = pritap_get_span_sig_status;
	
	span->state_map = &pritap_state_map;
	span->state_processor = state_advance;

	return FTDM_SUCCESS;
}

/**
 * \brief FreeTDM pritap signaling and IO module definition
 */
ftdm_module_t ftdm_module = { 
	"pritap",
	ftdm_pritap_io_init,
	ftdm_pritap_unload,
	ftdm_pritap_init,
	NULL,
	NULL,
	ftdm_pritap_configure_span,
};


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
