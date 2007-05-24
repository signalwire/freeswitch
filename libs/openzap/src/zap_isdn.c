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

	assert(span != NULL);
	assert(data != NULL);

	zap_log(ZAP_LOG_DEBUG, "Yay I got an event! Type:[%d] Size:[%d]\n", gen->MesType, gen->Size);

	gen->MesType = Q931mes_RESTART_ACKNOWLEDGE;
	Q931Rx43(&data->q931, msg, gen->Size);

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
	print_bits(msg, (int)len, bb, sizeof(bb), 1);
	zap_log(ZAP_LOG_DEBUG, "WRITE %d\n%s\n%s\n\n", (int)len, LINE, bb);

#endif

	assert(span != NULL);
	return zap_channel_write(span->isdn_data->dchan, msg, &len) == ZAP_SUCCESS ? 0 : -1;
}

static void *zap_isdn_run(zap_thread_t *me, void *obj)
{
	zap_span_t *span = (zap_span_t *) obj;
	zap_isdn_data_t *data = span->isdn_data;
	unsigned char buf[1024];
	zap_size_t len = sizeof(buf);

#ifdef WIN32
    timeBeginPeriod(1);
#endif

	zap_log(ZAP_LOG_DEBUG, "ISDN thread starting.\n");

	Q921Start(&data->q921);

	while(zap_test_flag(data, ZAP_ISDN_RUNNING)) {
		zap_wait_flag_t flags = ZAP_READ;
		zap_status_t status = zap_channel_wait(data->dchan, &flags, 100);

		Q921TimerTick(&data->q921);

		switch(status) {
		case ZAP_FAIL:
			{
				zap_log(ZAP_LOG_ERROR, "D-Chan Read Error!\n");
				snprintf(span->last_error, sizeof(span->last_error), "D-Chan Read Error!");
				goto done;
			}
			break;
		case ZAP_TIMEOUT:
			{
				/*zap_log(ZAP_LOG_DEBUG, "Timeout!\n");*/
				/*Q931TimeTick(data->q931, L3ULONG ms);*/
			}
			break;
		default:
			{
				if (flags & ZAP_READ) {
					len = sizeof(buf);
					if (zap_channel_read(data->dchan, buf, &len) == ZAP_SUCCESS) {
#ifdef IODEBUG
						char bb[4096] = "";
						print_bits(buf, (int)len, bb, sizeof(bb), 1);
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
		snprintf(span->last_error, sizeof(span->last_error), "Span is already configured for signalling.");
		return ZAP_FAIL;
	}

	if (span->trunk_type >= ZAP_TRUNK_NONE) {
		snprintf(span->last_error, sizeof(span->last_error), "Unknown trunk type!");
		return ZAP_FAIL;
	}
	
	for(i = 1; i <= span->chan_count; i++) {
		if (span->channels[i].type == ZAP_CHAN_TYPE_DQ921) {
			if (zap_channel_open(span->zio->name, span->span_id, i, &dchans[x]) == ZAP_SUCCESS) {
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
	

	span->signal_type = ZAP_SIGTYPE_ISDN;
	
	return ZAP_SUCCESS;
}
