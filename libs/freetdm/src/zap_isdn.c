/*
 * Copyright (c) 2007, Anthony Minessale II
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

#include "openzap.h"
#include "zap_isdn.h"
#include "Q931.h"
#include "Q921.h"
#ifdef WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

#define LINE "--------------------------------------------------------------------------------"
#define IODEBUG

static L2ULONG zap_time_now()
{
#ifdef WIN32
	return timeGetTime();
#else
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return ((tv.tv_sec * 1000) + (tv.tv_usec / 1000));
#endif
}

static L3INT zap_isdn_931_err(void *pvt, L3INT id, L3INT p1, L3INT p2)
{
	zap_log(ZAP_LOG_ERROR, "ERROR: [%s] [%d] [%d]\n", q931_error_to_name(id), p1, p2);
	return 0;
}

static L3INT zap_isdn_931_34(void *pvt, L2UCHAR *msg, L2INT mlen)
{
	zap_span_t *span = (zap_span_t *) pvt;
	zap_isdn_data_t *data = span->isdn_data;
	Q931mes_Generic *gen = (Q931mes_Generic *) msg;
	Q931ie_ChanID *chanid = Q931GetIEPtr(gen->ChanID, gen->buf);
	int chan_id = chanid->ChanSlot;
	zap_channel_t *zchan = NULL;
	
	assert(span != NULL);
	assert(data != NULL);
	
	if (chan_id) {
		zchan = &span->channels[chan_id];
	}

	zap_log(ZAP_LOG_DEBUG, "Yay I got an event! Type:[%02x] Size:[%d]\n", gen->MesType, gen->Size);
	switch(gen->MesType) {
	case Q931mes_RESTART:
		{
			if (zchan) {
				zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_RESTART);
			} else {
				uint32_t i;
				for (i = 0; i < span->chan_count; i++) {
					zap_set_state_locked((&span->channels[i]), ZAP_CHANNEL_STATE_RESTART);
				}
			}
		}
		break;
	case Q931mes_RELEASE_COMPLETE:
		{
			zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_DOWN);
		}
		break;
	case Q931mes_DISCONNECT:
		{
			zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_TERMINATING);
		}
		break;
	case Q931mes_SETUP:
		{

			Q931ie_CallingNum *callingnum = Q931GetIEPtr(gen->CallingNum, gen->buf);
			Q931ie_CalledNum *callednum = Q931GetIEPtr(gen->CalledNum, gen->buf);
			zap_status_t status;
			int fail = 1;
			uint32_t cplen = mlen;


			if ((status = zap_channel_open(span->span_id, chan_id, &zchan) == ZAP_SUCCESS)) {
				if (zchan->state == ZAP_CHANNEL_STATE_DOWN) {
					memset(&zchan->caller_data, 0, sizeof(zchan->caller_data));

					zap_set_string(zchan->caller_data.cid_num, callingnum->Digit);
					zap_set_string(zchan->caller_data.cid_name, callingnum->Digit);
					zap_set_string(zchan->caller_data.ani, callingnum->Digit);
					zap_set_string(zchan->caller_data.dnis, callednum->Digit);

					zchan->caller_data.CRV = gen->CRV;
					if (cplen > sizeof(zchan->caller_data.raw_data)) {
						cplen = sizeof(zchan->caller_data.raw_data);
					}
					gen->CRVFlag = !(gen->CRVFlag);
					memcpy(zchan->caller_data.raw_data, msg, cplen);
					zchan->caller_data.raw_data_len = cplen;
					zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_RING);
					fail = 0;
				} 
			} 

			if (fail) {
				zap_log(ZAP_LOG_CRIT, "FIX ME!\n");
				// add me 
			}
			
		}
		break;
	default:
		break;
	}

	return 0;
}

static int zap_isdn_921_23(void *pvt, L2UCHAR *msg, L2INT mlen)
{
	int ret = Q931Rx23(pvt, msg, mlen);
	if (ret != 0)
		zap_log(ZAP_LOG_DEBUG, "931 parse error [%d] [%s]\n", ret, q931_error_to_name(ret));
	return ((ret >= 0) ? 1 : 0);
}

static int zap_isdn_921_21(void *pvt, L2UCHAR *msg, L2INT mlen)
{
	zap_span_t *span = (zap_span_t *) pvt;
	zap_size_t len = (zap_size_t) mlen;
#ifdef IODEBUG
	char bb[4096] = "";
	print_bits(msg, (int)len, bb, sizeof(bb), ZAP_ENDIAN_LITTLE, 0);
	zap_log(ZAP_LOG_DEBUG, "WRITE %d\n%s\n%s\n\n", (int)len, LINE, bb);

#endif

	assert(span != NULL);
	return zap_channel_write(span->isdn_data->dchan, msg, len, &len) == ZAP_SUCCESS ? 0 : -1;
}

static __inline__ void state_advance(zap_channel_t *zchan)
{
	Q931mes_Generic *gen = (Q931mes_Generic *) zchan->caller_data.raw_data;
	zap_isdn_data_t *data = zchan->span->isdn_data;
	zap_sigmsg_t sig;
	zap_status_t status;

	zap_log(ZAP_LOG_ERROR, "%d:%d STATE [%s]\n", 
			zchan->span_id, zchan->chan_id, zap_channel_state2str(zchan->state));

	memset(&sig, 0, sizeof(sig));
	sig.chan_id = zchan->chan_id;
	sig.span_id = zchan->span_id;
	sig.channel = zchan;

	switch (zchan->state) {
	case ZAP_CHANNEL_STATE_RING:
		{
			sig.event_id = ZAP_SIGEVENT_START;
			if ((status = data->sig_cb(&sig) != ZAP_SUCCESS)) {
				zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_HANGUP);
			}

		}
		break;
	case ZAP_CHANNEL_STATE_RESTART:
		{
			zap_set_state_locked(zchan, ZAP_CHANNEL_STATE_DOWN);
			zap_channel_close(&zchan);
		}
		break;
	case ZAP_CHANNEL_STATE_UP:
		{
			gen->MesType = Q931mes_CONNECT;
			gen->BearerCap = 0;
			Q931Rx43(&data->q931, (void *)gen, zchan->caller_data.raw_data_len);
		}
		break;
	case ZAP_CHANNEL_STATE_HANGUP:
		{
			Q931ie_Cause cause;
			gen->MesType = Q931mes_DISCONNECT;
			cause.IEId = Q931ie_CAUSE;
			cause.Size = sizeof(Q931ie_Cause);
			cause.CodStand  = 0;
			cause.Location = 1;
			cause.Recom = 1;
			cause.Value = zchan->caller_data.hangup_cause;
			*cause.Diag = '\0';
			gen->Cause = Q931AppendIE((L3UCHAR *) gen, (L3UCHAR *) &cause);
			Q931Rx43(&data->q931, (L3UCHAR *) gen, gen->Size);
		}
		break;
	case ZAP_CHANNEL_STATE_TERMINATING:
		{
			gen->MesType = Q931mes_RELEASE;
			Q931Rx43(&data->q931, (void *)gen, gen->Size);
		}
	default:
		break;
	}
}

static __inline__ void check_state(zap_span_t *span)
{
	if (zap_test_flag(span, ZAP_SPAN_STATE_CHANGE)) {
		uint32_t j;
		for(j = 1; j <= span->chan_count; j++) {
			if (zap_test_flag((&span->channels[j]), ZAP_CHANNEL_STATE_CHANGE)) {
				state_advance(&span->channels[j]);
				zap_clear_flag_locked((&span->channels[j]), ZAP_CHANNEL_STATE_CHANGE);
			}
		}
		zap_clear_flag_locked(span, ZAP_SPAN_STATE_CHANGE);
	}
}

static void *zap_isdn_run(zap_thread_t *me, void *obj)
{
	zap_span_t *span = (zap_span_t *) obj;
	zap_isdn_data_t *data = span->isdn_data;
	unsigned char buf[1024];
	zap_size_t len = sizeof(buf);
	int errs = 0;

#ifdef WIN32
    timeBeginPeriod(1);
#endif

	zap_log(ZAP_LOG_DEBUG, "ISDN thread starting.\n");

	Q921Start(&data->q921);

	while(zap_test_flag(data, ZAP_ISDN_RUNNING)) {
		zap_wait_flag_t flags = ZAP_READ;
		zap_status_t status = zap_channel_wait(data->dchan, &flags, 100);

		Q921TimerTick(&data->q921);
		check_state(span);
		
		switch(status) {
		case ZAP_FAIL:
			{
				zap_log(ZAP_LOG_ERROR, "D-Chan Read Error!\n");
				snprintf(span->last_error, sizeof(span->last_error), "D-Chan Read Error!");
				if (++errs == 10) {
					goto done;
				}
			}
			break;
		case ZAP_TIMEOUT:
			{
				/*zap_log(ZAP_LOG_DEBUG, "Timeout!\n");*/
				/*Q931TimeTick(data->q931, L3ULONG ms);*/
				errs = 0;
			}
			break;
		default:
			{
				errs = 0;
				if (flags & ZAP_READ) {
					len = sizeof(buf);
					if (zap_channel_read(data->dchan, buf, &len) == ZAP_SUCCESS) {
#ifdef IODEBUG
						char bb[4096] = "";
						print_bits(buf, (int)len, bb, sizeof(bb), ZAP_ENDIAN_LITTLE, 0);
						zap_log(ZAP_LOG_DEBUG, "READ %d\n%s\n%s\n\n", (int)len, LINE, bb);
#endif

						Q921QueueHDLCFrame(&data->q921, buf, (int)len);
						Q921Rx12(&data->q921);
					}
				} else {
					zap_log(ZAP_LOG_DEBUG, "No Read FLAG!\n");
				}
			}
			break;
		}

	}
	
 done:

	zap_channel_close(&data->dchans[0]);
	zap_channel_close(&data->dchans[1]);
	zap_clear_flag(span->isdn_data, ZAP_ISDN_RUNNING);

#ifdef WIN32
    timeEndPeriod(1);
#endif

	zap_log(ZAP_LOG_DEBUG, "ISDN thread ended.\n");
	return NULL;
}

zap_status_t zap_isdn_init(void)
{
	Q931Initialize();

	Q921SetGetTimeCB(zap_time_now);
	
	return ZAP_SUCCESS;
}

zap_status_t zap_isdn_start(zap_span_t *span)
{
	zap_set_flag(span->isdn_data, ZAP_ISDN_RUNNING);
	return zap_thread_create_detached(zap_isdn_run, span);
}

zap_status_t zap_isdn_configure_span(zap_span_t *span, Q921NetUser_t mode, Q931Dialect_t dialect, zio_signal_cb_t sig_cb)
{
	uint32_t i, x = 0;
	zap_channel_t *dchans[2] = {0};

	if (span->signal_type) {
		snprintf(span->last_error, sizeof(span->last_error), "Span is already configured for signalling [%d].", span->signal_type);
		return ZAP_FAIL;
	}

	if (span->trunk_type >= ZAP_TRUNK_NONE) {
		snprintf(span->last_error, sizeof(span->last_error), "Unknown trunk type!");
		return ZAP_FAIL;
	}
	
	for(i = 1; i <= span->chan_count; i++) {
		if (span->channels[i].type == ZAP_CHAN_TYPE_DQ921) {
			if (zap_channel_open(span->span_id, i, &dchans[x]) == ZAP_SUCCESS) {
				zap_log(ZAP_LOG_DEBUG, "opening d-channel #%d %d:%d\n", x, dchans[x]->span_id, dchans[x]->chan_id);
				x++;
			}
		}
	}

	if (!x) {
		snprintf(span->last_error, sizeof(span->last_error), "Span has no D-Channels!");
		return ZAP_FAIL;
	}

	
	span->isdn_data = malloc(sizeof(*span->isdn_data));
	assert(span->isdn_data != NULL);
	memset(span->isdn_data, 0, sizeof(*span->isdn_data));
	
	span->isdn_data->sig_cb = sig_cb;
	span->isdn_data->dchans[0] = dchans[0];
	span->isdn_data->dchans[1] = dchans[1];
	span->isdn_data->dchan = span->isdn_data->dchans[0];
	
	Q921_InitTrunk(&span->isdn_data->q921,
				   0,
				   0,
				   mode,
				   0,
				   zap_isdn_921_21,
				   (Q921TxCB_t)zap_isdn_921_23,
				   span,
				   &span->isdn_data->q931);
	
	Q931Api_InitTrunk(&span->isdn_data->q931,
					  dialect,
					  mode,
					  span->trunk_type,
					  zap_isdn_931_34,
					  (Q931TxCB_t)Q921Rx32,
					  zap_isdn_931_err,
					  &span->isdn_data->q921,
					  span);

	span->isdn_data->q931.autoRestartAck = 1;


	span->signal_type = ZAP_SIGTYPE_ISDN;
	
	return ZAP_SUCCESS;
}

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 expandtab:
 */
