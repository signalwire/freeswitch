/*
 * Copyright (c) 2007-2012, Anthony Minessale II
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
 *
 * Contributors: 
 *
 * Moises Silva <moy@sangoma.com>
 * W McRoberts <fs@whmcr.com>
 * Puskás Zsolt <errotan@gmail.com>
 *
 */

#include "private/ftdm_core.h"
#include "ftmod_zt.h"

/* used by dahdi to indicate there is no data available, but events to read */
#ifndef ELAST
#define ELAST 500
#endif

/**
 * \brief Zaptel globals
 */
static struct {
	uint32_t codec_ms;
	uint32_t wink_ms;
	uint32_t flash_ms;
	uint32_t eclevel;
	uint32_t etlevel;
    float rxgain;
    float txgain;
} zt_globals;

#if defined(__FreeBSD__)
typedef unsigned long ioctlcmd;
#else
typedef int ioctlcmd;
#endif

/**
 * \brief General IOCTL codes
 */
struct ioctl_codes {
    ioctlcmd GET_BLOCKSIZE;
    ioctlcmd SET_BLOCKSIZE;
    ioctlcmd FLUSH;
    ioctlcmd SYNC;
    ioctlcmd GET_PARAMS;
    ioctlcmd SET_PARAMS;
    ioctlcmd HOOK;
    ioctlcmd GETEVENT;
    ioctlcmd IOMUX;
    ioctlcmd SPANSTAT;
    ioctlcmd MAINT;
    ioctlcmd GETCONF;
    ioctlcmd SETCONF;
    ioctlcmd CONFLINK;
    ioctlcmd CONFDIAG;
    ioctlcmd GETGAINS;
    ioctlcmd SETGAINS;
    ioctlcmd SPANCONFIG;
    ioctlcmd CHANCONFIG;
    ioctlcmd SET_BUFINFO;
    ioctlcmd GET_BUFINFO;
    ioctlcmd AUDIOMODE;
    ioctlcmd ECHOCANCEL;
    ioctlcmd HDLCRAWMODE;
    ioctlcmd HDLCFCSMODE;
    ioctlcmd SPECIFY;
    ioctlcmd SETLAW;
    ioctlcmd SETLINEAR;
    ioctlcmd GETCONFMUTE;
    ioctlcmd ECHOTRAIN;
    ioctlcmd SETTXBITS;
    ioctlcmd GETRXBITS;
    ioctlcmd SETPOLARITY;
    ioctlcmd TONEDETECT;
};

/**
 * \brief Zaptel IOCTL codes
 */
static struct ioctl_codes zt_ioctl_codes = {
    .GET_BLOCKSIZE = ZT_GET_BLOCKSIZE,
    .SET_BLOCKSIZE = ZT_SET_BLOCKSIZE,
    .FLUSH = ZT_FLUSH,
    .SYNC = ZT_SYNC,
    .GET_PARAMS = ZT_GET_PARAMS,
    .SET_PARAMS = ZT_SET_PARAMS,
    .HOOK = ZT_HOOK,
    .GETEVENT = ZT_GETEVENT,
    .IOMUX = ZT_IOMUX,
    .SPANSTAT = ZT_SPANSTAT,
    .MAINT = ZT_MAINT,
    .GETCONF = ZT_GETCONF,
    .SETCONF = ZT_SETCONF,
    .CONFLINK = ZT_CONFLINK,
    .CONFDIAG = ZT_CONFDIAG,
    .GETGAINS = ZT_GETGAINS,
    .SETGAINS = ZT_SETGAINS,
    .SPANCONFIG = ZT_SPANCONFIG,
    .CHANCONFIG = ZT_CHANCONFIG,
    .SET_BUFINFO = ZT_SET_BUFINFO,
    .GET_BUFINFO = ZT_GET_BUFINFO,
    .AUDIOMODE = ZT_AUDIOMODE,
    .ECHOCANCEL = ZT_ECHOCANCEL,
    .HDLCRAWMODE = ZT_HDLCRAWMODE,
    .HDLCFCSMODE = ZT_HDLCFCSMODE,
    .SPECIFY = ZT_SPECIFY,
    .SETLAW = ZT_SETLAW,
    .SETLINEAR = ZT_SETLINEAR,
    .GETCONFMUTE = ZT_GETCONFMUTE,
    .ECHOTRAIN = ZT_ECHOTRAIN,
    .SETTXBITS = ZT_SETTXBITS,
    .GETRXBITS = ZT_GETRXBITS,
    .TONEDETECT = ZT_TONEDETECT,
};

/**
 * \brief Dahdi IOCTL codes
 */
static struct ioctl_codes dahdi_ioctl_codes = {
    .GET_BLOCKSIZE = DAHDI_GET_BLOCKSIZE,
    .SET_BLOCKSIZE = DAHDI_SET_BLOCKSIZE,
    .FLUSH = DAHDI_FLUSH,
    .SYNC = DAHDI_SYNC,
    .GET_PARAMS = DAHDI_GET_PARAMS,
    .SET_PARAMS = DAHDI_SET_PARAMS,
    .HOOK = DAHDI_HOOK,
    .GETEVENT = DAHDI_GETEVENT,
    .IOMUX = DAHDI_IOMUX,
    .SPANSTAT = DAHDI_SPANSTAT,
    .MAINT = DAHDI_MAINT,
    .GETCONF = DAHDI_GETCONF,
    .SETCONF = DAHDI_SETCONF,
    .CONFLINK = DAHDI_CONFLINK,
    .CONFDIAG = DAHDI_CONFDIAG,
    .GETGAINS = DAHDI_GETGAINS,
    .SETGAINS = DAHDI_SETGAINS,
    .SPANCONFIG = DAHDI_SPANCONFIG,
    .CHANCONFIG = DAHDI_CHANCONFIG,
    .SET_BUFINFO = DAHDI_SET_BUFINFO,
    .GET_BUFINFO = DAHDI_GET_BUFINFO,
    .AUDIOMODE = DAHDI_AUDIOMODE,
    .ECHOCANCEL = DAHDI_ECHOCANCEL,
    .HDLCRAWMODE = DAHDI_HDLCRAWMODE,
    .HDLCFCSMODE = DAHDI_HDLCFCSMODE,
    .SPECIFY = DAHDI_SPECIFY,
    .SETLAW = DAHDI_SETLAW,
    .SETLINEAR = DAHDI_SETLINEAR,
    .GETCONFMUTE = DAHDI_GETCONFMUTE,
    .ECHOTRAIN = DAHDI_ECHOTRAIN,
    .SETTXBITS = DAHDI_SETTXBITS,
    .GETRXBITS = DAHDI_GETRXBITS,
    .SETPOLARITY = DAHDI_SETPOLARITY,
    .TONEDETECT = DAHDI_TONEDETECT,
};

#define ZT_INVALID_SOCKET -1
static struct ioctl_codes codes;
static const char *ctlpath = NULL;
static const char *chanpath = NULL;

static const char dahdi_ctlpath[] = "/dev/dahdi/ctl";
static const char dahdi_chanpath[] = "/dev/dahdi/channel";

static const char zt_ctlpath[] = "/dev/zap/ctl";
static const char zt_chanpath[] = "/dev/zap/channel";

static ftdm_socket_t CONTROL_FD = ZT_INVALID_SOCKET;

FIO_SPAN_NEXT_EVENT_FUNCTION(zt_next_event);
FIO_SPAN_POLL_EVENT_FUNCTION(zt_poll_event);
FIO_CHANNEL_NEXT_EVENT_FUNCTION(zt_channel_next_event);

/**
 * \brief Initialises codec, and rx/tx gains
 * \param g Structure for gains to be initialised
 * \param rxgain RX gain value
 * \param txgain TX gain value
 * \param codec Codec
 */
static void zt_build_gains(struct zt_gains *g, float rxgain, float txgain, int codec)
{
	int j;
	int k;
	float linear_rxgain = pow(10.0, rxgain / 20.0);
    float linear_txgain = pow(10.0, txgain / 20.0);

	switch (codec) {
	case FTDM_CODEC_ALAW:
		for (j = 0; j < (sizeof(g->receive_gain) / sizeof(g->receive_gain[0])); j++) {
			if (rxgain) {
				k = (int) (((float) alaw_to_linear(j)) * linear_rxgain);
				if (k > 32767) k = 32767;
				if (k < -32767) k = -32767;
				g->receive_gain[j] = linear_to_alaw(k);
			} else {
				g->receive_gain[j] = j;
			}
			if (txgain) {
				k = (int) (((float) alaw_to_linear(j)) * linear_txgain);
				if (k > 32767) k = 32767;
				if (k < -32767) k = -32767;
				g->transmit_gain[j] = linear_to_alaw(k);
			} else {
				g->transmit_gain[j] = j;
			}
		}
		break;
	case FTDM_CODEC_ULAW:
		for (j = 0; j < (sizeof(g->receive_gain) / sizeof(g->receive_gain[0])); j++) {
			if (rxgain) {
				k = (int) (((float) ulaw_to_linear(j)) * linear_rxgain);
				if (k > 32767) k = 32767;
				if (k < -32767) k = -32767;
				g->receive_gain[j] = linear_to_ulaw(k);
			} else {
				g->receive_gain[j] = j;
			}
			if (txgain) {
				k = (int) (((float) ulaw_to_linear(j)) * linear_txgain);
				if (k > 32767) k = 32767;
				if (k < -32767) k = -32767;
				g->transmit_gain[j] = linear_to_ulaw(k);
			} else {
				g->transmit_gain[j] = j;
			}
		}
		break;
	}
}

/**
 * \brief Initialises a range of Zaptel/DAHDI channels
 * \param span FreeTDM span
 * \param start Initial wanpipe channel number
 * \param end Final wanpipe channel number
 * \param type FreeTDM channel type
 * \param name FreeTDM span name
 * \param number FreeTDM span number
 * \param cas_bits CAS bits
 * \return number of spans configured
 */
static unsigned zt_open_range(ftdm_span_t *span, unsigned start, unsigned end, ftdm_chan_type_t type, char *name, char *number, unsigned char cas_bits)
{
	unsigned configured = 0, x;
	zt_params_t ztp;
	zt_tone_mode_t mode = 0;

	memset(&ztp, 0, sizeof(ztp));

	if (type == FTDM_CHAN_TYPE_CAS) {
		ftdm_log(FTDM_LOG_DEBUG, "Configuring CAS channels with abcd == 0x%X\n", cas_bits);
	}	
	for(x = start; x < end; x++) {
		ftdm_channel_t *ftdmchan;
		ftdm_socket_t sockfd = ZT_INVALID_SOCKET;
		int len;

		sockfd = open(chanpath, O_RDWR);
		if (sockfd != ZT_INVALID_SOCKET && ftdm_span_add_channel(span, sockfd, type, &ftdmchan) == FTDM_SUCCESS) {

			if (ioctl(sockfd, codes.SPECIFY, &x)) {
				ftdm_log(FTDM_LOG_ERROR, "failure configuring device %s chan %d fd %d (%s)\n", chanpath, x, sockfd, strerror(errno));
				close(sockfd);
				continue;
			}

			if (ftdmchan->type == FTDM_CHAN_TYPE_DQ921) {
				struct zt_bufferinfo binfo;
				memset(&binfo, 0, sizeof(binfo));
				binfo.txbufpolicy = 0;
				binfo.rxbufpolicy = 0;
				binfo.numbufs = 32;
				binfo.bufsize = 1024;
				if (ioctl(sockfd, codes.SET_BUFINFO, &binfo)) {
					ftdm_log(FTDM_LOG_ERROR, "failure configuring device %s as FreeTDM device %d:%d fd:%d\n", chanpath, ftdmchan->span_id, ftdmchan->chan_id, sockfd);
					close(sockfd);
					continue;
				}
			}

			if (type == FTDM_CHAN_TYPE_FXS || type == FTDM_CHAN_TYPE_FXO) {
				struct zt_chanconfig cc;
				memset(&cc, 0, sizeof(cc));
				cc.chan = cc.master = x;
				
				switch(type) {
				case FTDM_CHAN_TYPE_FXS:
					{
						switch(span->start_type) {
						case FTDM_ANALOG_START_KEWL:
							cc.sigtype = ZT_SIG_FXOKS;
							break;
						case FTDM_ANALOG_START_LOOP:
							cc.sigtype = ZT_SIG_FXOLS;
							break;
						case FTDM_ANALOG_START_GROUND:
							cc.sigtype = ZT_SIG_FXOGS;
							break;
						default:
							break;
						}
					}
					break;
				case FTDM_CHAN_TYPE_FXO:
					{
						switch(span->start_type) {
						case FTDM_ANALOG_START_KEWL:
							cc.sigtype = ZT_SIG_FXSKS;
							break;
						case FTDM_ANALOG_START_LOOP:
							cc.sigtype = ZT_SIG_FXSLS;
							break;
						case FTDM_ANALOG_START_GROUND:
							cc.sigtype = ZT_SIG_FXSGS;
							break;
						default:
							break;
						}
					}
					break;
				default:
					break;
				}
				
				if (ioctl(CONTROL_FD, codes.CHANCONFIG, &cc)) {
					ftdm_log(FTDM_LOG_WARNING, "this ioctl fails in older zaptel but is harmless if you used ztcfg\n[device %s chan %d fd %d (%s)]\n", chanpath, x, CONTROL_FD, strerror(errno));
				}
			}

			if (type == FTDM_CHAN_TYPE_CAS) {
				struct zt_chanconfig cc;
				memset(&cc, 0, sizeof(cc));
				cc.chan = cc.master = x;
				cc.sigtype = ZT_SIG_CAS;
				cc.idlebits = cas_bits;
				if (ioctl(CONTROL_FD, codes.CHANCONFIG, &cc)) {
					ftdm_log(FTDM_LOG_ERROR, "failure configuring device %s as FreeTDM device %d:%d fd:%d err:%s\n", chanpath, ftdmchan->span_id, ftdmchan->chan_id, sockfd, strerror(errno));
					close(sockfd);
					continue;
				}
			}

			if (ftdmchan->type != FTDM_CHAN_TYPE_DQ921 && ftdmchan->type != FTDM_CHAN_TYPE_DQ931) {
				len = zt_globals.codec_ms * 8;
				if (ioctl(ftdmchan->sockfd, codes.SET_BLOCKSIZE, &len)) {
					ftdm_log(FTDM_LOG_ERROR, "failure configuring device %s as FreeTDM device %d:%d fd:%d err:%s\n", 
							chanpath, ftdmchan->span_id, ftdmchan->chan_id, sockfd, strerror(errno));
					close(sockfd);
					continue;
				}

				ftdmchan->packet_len = len;
				ftdmchan->effective_interval = ftdmchan->native_interval = ftdmchan->packet_len / 8;
			
				if (ftdmchan->effective_codec == FTDM_CODEC_SLIN) {
					ftdmchan->packet_len *= 2;
				}
			}
			
			if (ioctl(sockfd, codes.GET_PARAMS, &ztp) < 0) {
				ftdm_log(FTDM_LOG_ERROR, "failure configuring device %s as FreeTDM device %d:%d fd:%d\n", chanpath, ftdmchan->span_id, ftdmchan->chan_id, sockfd);
				close(sockfd);
				continue;
			}

			if (ftdmchan->type == FTDM_CHAN_TYPE_DQ921) {
				if (
					(ztp.sig_type != ZT_SIG_HDLCRAW) &&
					(ztp.sig_type != ZT_SIG_HDLCFCS) &&
					(ztp.sig_type != ZT_SIG_HARDHDLC)
					) {
					ftdm_log(FTDM_LOG_ERROR, "Failure configuring device %s as FreeTDM device %d:%d fd:%d, hardware signaling is not HDLC, fix your Zap/DAHDI configuration!\n", chanpath, ftdmchan->span_id, ftdmchan->chan_id, sockfd);
					close(sockfd);
					continue;
				}
			}

			ftdm_log(FTDM_LOG_INFO, "configuring device %s channel %d as FreeTDM device %d:%d fd:%d\n", chanpath, x, ftdmchan->span_id, ftdmchan->chan_id, sockfd);
			
			ftdmchan->rate = 8000;
			ftdmchan->physical_span_id = ztp.span_no;
			ftdmchan->physical_chan_id = ztp.chan_no;
			
			if (type == FTDM_CHAN_TYPE_FXS || type == FTDM_CHAN_TYPE_FXO || type == FTDM_CHAN_TYPE_EM || type == FTDM_CHAN_TYPE_B) {
				if (ztp.g711_type == ZT_G711_ALAW) {
					ftdmchan->native_codec = ftdmchan->effective_codec = FTDM_CODEC_ALAW;
				} else if (ztp.g711_type == ZT_G711_MULAW) {
					ftdmchan->native_codec = ftdmchan->effective_codec = FTDM_CODEC_ULAW;
				} else {
					int type;

					if (ftdmchan->span->trunk_type == FTDM_TRUNK_E1) {
						type = FTDM_CODEC_ALAW;
					} else {
						type = FTDM_CODEC_ULAW;
					}

					ftdmchan->native_codec = ftdmchan->effective_codec = type;

				}
			}

			ztp.wink_time = zt_globals.wink_ms;
			ztp.flash_time = zt_globals.flash_ms;

			if (ioctl(sockfd, codes.SET_PARAMS, &ztp) < 0) {
				ftdm_log(FTDM_LOG_ERROR, "failure configuring device %s as FreeTDM device %d:%d fd:%d\n", chanpath, ftdmchan->span_id, ftdmchan->chan_id, sockfd);
				close(sockfd);
				continue;
			}

			mode = ZT_TONEDETECT_ON | ZT_TONEDETECT_MUTE;
			if (ioctl(sockfd, codes.TONEDETECT, &mode)) {
				ftdm_log(FTDM_LOG_DEBUG, "HW DTMF not available on FreeTDM device %d:%d fd:%d\n", ftdmchan->span_id, ftdmchan->chan_id, sockfd);
			} else {
				ftdm_log(FTDM_LOG_DEBUG, "HW DTMF available on FreeTDM device %d:%d fd:%d\n", ftdmchan->span_id, ftdmchan->chan_id, sockfd);
				ftdm_channel_set_feature(ftdmchan, FTDM_CHANNEL_FEATURE_DTMF_DETECT);
				mode = 0;
				ioctl(sockfd, codes.TONEDETECT, &mode);
			}

			if (!ftdm_strlen_zero(name)) {
				ftdm_copy_string(ftdmchan->chan_name, name, sizeof(ftdmchan->chan_name));
			}
			if (!ftdm_strlen_zero(number)) {
				ftdm_copy_string(ftdmchan->chan_number, number, sizeof(ftdmchan->chan_number));
			}

			configured++;
		} else {
			ftdm_log(FTDM_LOG_ERROR, "failure configuring device %s\n", chanpath);
		}
	}
	


	return configured;
}

/**
 * \brief Initialises a freetdm Zaptel/DAHDI span from a configuration string
 * \param span FreeTDM span
 * \param str Configuration string
 * \param type FreeTDM span type
 * \param name FreeTDM span name
 * \param number FreeTDM span number
 * \return Success or failure
 */
static FIO_CONFIGURE_SPAN_FUNCTION(zt_configure_span)
{

	int items, i;
	char *mydata, *item_list[10];
	char *ch, *mx;
	unsigned char cas_bits = 0;
	int channo;
	int top = 0;
	unsigned configured = 0;

	assert(str != NULL);
	

	mydata = ftdm_strdup(str);
	assert(mydata != NULL);


	items = ftdm_separate_string(mydata, ',', item_list, (sizeof(item_list) / sizeof(item_list[0])));

	for(i = 0; i < items; i++) {
		ch = item_list[i];

		if (!(ch)) {
			ftdm_log(FTDM_LOG_ERROR, "Invalid input\n");
			continue;
		}

		channo = atoi(ch);
		
		if (channo < 0) {
			ftdm_log(FTDM_LOG_ERROR, "Invalid channel number %d\n", channo);
			continue;
		}

		if ((mx = strchr(ch, '-'))) {
			mx++;
			top = atoi(mx) + 1;
		} else {
			top = channo + 1;
		}
		
		
		if (top < 0) {
			ftdm_log(FTDM_LOG_ERROR, "Invalid range number %d\n", top);
			continue;
		}
		if (FTDM_CHAN_TYPE_CAS == type && ftdm_config_get_cas_bits(ch, &cas_bits)) {
			ftdm_log(FTDM_LOG_ERROR, "Failed to get CAS bits in CAS channel\n");
			continue;
		}
		configured += zt_open_range(span, channo, top, type, name, number, cas_bits);

	}
	
	ftdm_safe_free(mydata);

	return configured;

}

/**
 * \brief Process configuration variable for a Zaptel/DAHDI profile
 * \param category Wanpipe profile name
 * \param var Variable name
 * \param val Variable value
 * \param lineno Line number from configuration file
 * \return Success
 */
static FIO_CONFIGURE_FUNCTION(zt_configure)
{

	int num;
    float fnum;

	if (!strcasecmp(category, "defaults")) {
		if (!strcasecmp(var, "codec_ms")) {
			num = atoi(val);
			if (num < 10 || num > 60) {
				ftdm_log(FTDM_LOG_WARNING, "invalid codec ms at line %d\n", lineno);
			} else {
				zt_globals.codec_ms = num;
			}
		} else if (!strcasecmp(var, "wink_ms")) {
			num = atoi(val);
			if (num < 50 || num > 3000) {
				ftdm_log(FTDM_LOG_WARNING, "invalid wink ms at line %d\n", lineno);
			} else {
				zt_globals.wink_ms = num;
			}
		} else if (!strcasecmp(var, "flash_ms")) {
			num = atoi(val);
			if (num < 50 || num > 3000) {
				ftdm_log(FTDM_LOG_WARNING, "invalid flash ms at line %d\n", lineno);
			} else {
				zt_globals.flash_ms = num;
			}
		} else if (!strcasecmp(var, "echo_cancel_level")) {
			num = atoi(val);
			if (num < 0 || num > 1024) {
				ftdm_log(FTDM_LOG_WARNING, "invalid echo can val at line %d\n", lineno);
			} else {
				zt_globals.eclevel = num;
			}
		} else if (!strcasecmp(var, "echo_train_level")) {
			if (zt_globals.eclevel <  1) {
				ftdm_log(FTDM_LOG_WARNING, "can't set echo train level without setting echo cancel level first at line %d\n", lineno);
			} else {
				num = atoi(val);
				if (num < 0 || num > 256) {
					ftdm_log(FTDM_LOG_WARNING, "invalid echo train val at line %d\n", lineno);
				} else {
					zt_globals.etlevel = num;
				}
			}
		} else if (!strcasecmp(var, "rxgain")) {
			fnum = (float)atof(val);
			if (fnum < -100.0 || fnum > 100.0) {
				ftdm_log(FTDM_LOG_WARNING, "invalid rxgain val at line %d\n", lineno);
			} else {
				zt_globals.rxgain = fnum;
				ftdm_log(FTDM_LOG_INFO, "Setting rxgain val to %f\n", fnum);
			}
		} else if (!strcasecmp(var, "txgain")) {
			fnum = (float)atof(val);
			if (fnum < -100.0 || fnum > 100.0) {
				ftdm_log(FTDM_LOG_WARNING, "invalid txgain val at line %d\n", lineno);
			} else {
				zt_globals.txgain = fnum;
				ftdm_log(FTDM_LOG_INFO, "Setting txgain val to %f\n", fnum);
			}
		} else {
				ftdm_log(FTDM_LOG_WARNING, "Ignoring unknown setting '%s'\n", var);
		}
	}

	return FTDM_SUCCESS;
}

/**
 * \brief Opens a Zaptel/DAHDI channel
 * \param ftdmchan Channel to open
 * \return Success or failure
 */
static FIO_OPEN_FUNCTION(zt_open) 
{
	ftdm_channel_set_feature(ftdmchan, FTDM_CHANNEL_FEATURE_INTERVAL);

	if (ftdmchan->type == FTDM_CHAN_TYPE_DQ921 || ftdmchan->type == FTDM_CHAN_TYPE_DQ931) {
		ftdmchan->native_codec = ftdmchan->effective_codec = FTDM_CODEC_NONE;
	} else {
		int blocksize = zt_globals.codec_ms * (ftdmchan->rate / 1000);
		int err;
		if ((err = ioctl(ftdmchan->sockfd, codes.SET_BLOCKSIZE, &blocksize))) {
			snprintf(ftdmchan->last_error, sizeof(ftdmchan->last_error), "%s", strerror(errno));
			return FTDM_FAIL;
		} else {
			ftdmchan->effective_interval = ftdmchan->native_interval;
			ftdmchan->packet_len = blocksize;
			ftdmchan->native_codec = ftdmchan->effective_codec;
		}
		
		if (ftdmchan->type == FTDM_CHAN_TYPE_B) {
			int one = 1;
			if (ioctl(ftdmchan->sockfd, codes.AUDIOMODE, &one)) {
				snprintf(ftdmchan->last_error, sizeof(ftdmchan->last_error), "%s", strerror(errno));
				ftdm_log(FTDM_LOG_ERROR, "%s\n", ftdmchan->last_error);
				return FTDM_FAIL;
			}
		}
		if (zt_globals.rxgain || zt_globals.txgain) {
			struct zt_gains gains;
			memset(&gains, 0, sizeof(gains));

			gains.chan_no = ftdmchan->physical_chan_id;
			zt_build_gains(&gains, zt_globals.rxgain, zt_globals.txgain, ftdmchan->native_codec);

			if (zt_globals.rxgain)
				ftdm_log(FTDM_LOG_INFO, "Setting rxgain to %f on channel %d\n", zt_globals.rxgain, gains.chan_no);

			if (zt_globals.txgain)
				ftdm_log(FTDM_LOG_INFO, "Setting txgain to %f on channel %d\n", zt_globals.txgain, gains.chan_no);

			if (ioctl(ftdmchan->sockfd, codes.SETGAINS, &gains) < 0) {
				ftdm_log(FTDM_LOG_ERROR, "failure configuring device %s as FreeTDM device %d:%d fd:%d\n", chanpath, ftdmchan->span_id, ftdmchan->chan_id, ftdmchan->sockfd);
			}
		}

		if (zt_globals.eclevel >= 0) {
			int len = zt_globals.eclevel;
			if (len) {
				ftdm_log(FTDM_LOG_INFO, "Setting echo cancel to %d taps for %d:%d\n", len, ftdmchan->span_id, ftdmchan->chan_id);
			} else {
				ftdm_log(FTDM_LOG_INFO, "Disable echo cancel for %d:%d\n", ftdmchan->span_id, ftdmchan->chan_id);
			}
			if (ioctl(ftdmchan->sockfd, codes.ECHOCANCEL, &len)) {
				ftdm_log(FTDM_LOG_WARNING, "Echo cancel not available for %d:%d\n", ftdmchan->span_id, ftdmchan->chan_id);
			} else if (zt_globals.etlevel > 0) {
				len = zt_globals.etlevel;
				if (ioctl(ftdmchan->sockfd, codes.ECHOTRAIN, &len)) {
					ftdm_log(FTDM_LOG_WARNING, "Echo training not available for %d:%d\n", ftdmchan->span_id, ftdmchan->chan_id);
				}
			}
		}
	}
	return FTDM_SUCCESS;
}

/**
 * \brief Closes Zaptel/DAHDI channel
 * \param ftdmchan Channel to close
 * \return Success
 */
static FIO_CLOSE_FUNCTION(zt_close)
{
	if (ftdmchan->type == FTDM_CHAN_TYPE_B) {
		int value = 0;	/* disable audio mode */
		if (ioctl(ftdmchan->sockfd, codes.AUDIOMODE, &value)) {
			snprintf(ftdmchan->last_error, sizeof(ftdmchan->last_error), "%s", strerror(errno));
			ftdm_log(FTDM_LOG_ERROR, "%s\n", ftdmchan->last_error);
			return FTDM_FAIL;
		}
	}
	return FTDM_SUCCESS;
}

/**
 * \brief Executes a FreeTDM command on a Zaptel/DAHDI channel
 * \param ftdmchan Channel to execute command on
 * \param command FreeTDM command to execute
 * \param obj Object (unused)
 * \return Success or failure
 */
static FIO_COMMAND_FUNCTION(zt_command)
{
	zt_params_t ztp;
	int err = 0;

	memset(&ztp, 0, sizeof(ztp));

	switch(command) {
	case FTDM_COMMAND_ENABLE_ECHOCANCEL:
		{
			int level = FTDM_COMMAND_OBJ_INT;
			err = ioctl(ftdmchan->sockfd, codes.ECHOCANCEL, &level);
			FTDM_COMMAND_OBJ_INT = level;
		}
	case FTDM_COMMAND_DISABLE_ECHOCANCEL:
		{
			int level = 0;
			err = ioctl(ftdmchan->sockfd, codes.ECHOCANCEL, &level);
			FTDM_COMMAND_OBJ_INT = level;
		}
		break;
	case FTDM_COMMAND_ENABLE_ECHOTRAIN:
		{
			int level = FTDM_COMMAND_OBJ_INT;
			err = ioctl(ftdmchan->sockfd, codes.ECHOTRAIN, &level);
			FTDM_COMMAND_OBJ_INT = level;
		}
	case FTDM_COMMAND_DISABLE_ECHOTRAIN:
		{
			int level = 0;
			err = ioctl(ftdmchan->sockfd, codes.ECHOTRAIN, &level);
			FTDM_COMMAND_OBJ_INT = level;
		}
		break;
	case FTDM_COMMAND_OFFHOOK:
		{
			int command = ZT_OFFHOOK;
			if (ioctl(ftdmchan->sockfd, codes.HOOK, &command)) {
				ftdm_log_chan_msg(ftdmchan, FTDM_LOG_ERROR, "OFFHOOK Failed");
				return FTDM_FAIL;
			}
			ftdm_log_chan_msg(ftdmchan, FTDM_LOG_DEBUG, "Channel is now offhook\n");
			ftdm_set_flag_locked(ftdmchan, FTDM_CHANNEL_OFFHOOK);
		}
		break;
	case FTDM_COMMAND_ONHOOK:
		{
			int command = ZT_ONHOOK;
			if (ioctl(ftdmchan->sockfd, codes.HOOK, &command)) {
				ftdm_log_chan_msg(ftdmchan, FTDM_LOG_ERROR, "ONHOOK Failed");
				return FTDM_FAIL;
			}
			ftdm_log_chan_msg(ftdmchan, FTDM_LOG_DEBUG, "Channel is now onhook\n");
			ftdm_clear_flag_locked(ftdmchan, FTDM_CHANNEL_OFFHOOK);
		}
		break;
	case FTDM_COMMAND_FLASH:
		{
			int command = ZT_FLASH;
			if (ioctl(ftdmchan->sockfd, codes.HOOK, &command)) {
				ftdm_log_chan_msg(ftdmchan, FTDM_LOG_ERROR, "FLASH Failed");
				return FTDM_FAIL;
			}
		}
		break;
	case FTDM_COMMAND_WINK:
		{
			int command = ZT_WINK;
			if (ioctl(ftdmchan->sockfd, codes.HOOK, &command)) {
				ftdm_log_chan_msg(ftdmchan, FTDM_LOG_ERROR, "WINK Failed");
				return FTDM_FAIL;
			}
		}
		break;
	case FTDM_COMMAND_GENERATE_RING_ON:
		{
			int command = ZT_RING;
			if (ioctl(ftdmchan->sockfd, codes.HOOK, &command)) {
				ftdm_log_chan_msg(ftdmchan, FTDM_LOG_ERROR, "RING Failed");
				return FTDM_FAIL;
			}
			ftdm_set_flag_locked(ftdmchan, FTDM_CHANNEL_RINGING);
		}
		break;
	case FTDM_COMMAND_GENERATE_RING_OFF:
		{
			int command = ZT_RINGOFF;
			if (ioctl(ftdmchan->sockfd, codes.HOOK, &command)) {
				ftdm_log_chan_msg(ftdmchan, FTDM_LOG_ERROR, "Ring-off Failed");
				return FTDM_FAIL;
			}
			ftdm_clear_flag_locked(ftdmchan, FTDM_CHANNEL_RINGING);
		}
		break;
	case FTDM_COMMAND_GET_INTERVAL:
		{

			if (!(err = ioctl(ftdmchan->sockfd, codes.GET_BLOCKSIZE, &ftdmchan->packet_len))) {
				ftdmchan->native_interval = ftdmchan->packet_len / 8;
				if (ftdmchan->effective_codec == FTDM_CODEC_SLIN) {
					ftdmchan->packet_len *= 2;
				}
				FTDM_COMMAND_OBJ_INT = ftdmchan->native_interval;
			} 			
		}
		break;
	case FTDM_COMMAND_SET_INTERVAL: 
		{
			int interval = FTDM_COMMAND_OBJ_INT;
			int len = interval * 8;

			if (!(err = ioctl(ftdmchan->sockfd, codes.SET_BLOCKSIZE, &len))) {
				ftdmchan->packet_len = len;
				ftdmchan->effective_interval = ftdmchan->native_interval = ftdmchan->packet_len / 8;

				if (ftdmchan->effective_codec == FTDM_CODEC_SLIN) {
					ftdmchan->packet_len *= 2;
				}
			}
		}
		break;
	case FTDM_COMMAND_SET_CAS_BITS:
		{
			int bits = FTDM_COMMAND_OBJ_INT;
			err = ioctl(ftdmchan->sockfd, codes.SETTXBITS, &bits);
		}
		break;
	case FTDM_COMMAND_GET_CAS_BITS:
		{
			err = ioctl(ftdmchan->sockfd, codes.GETRXBITS, &ftdmchan->rx_cas_bits);
			if (!err) {
				FTDM_COMMAND_OBJ_INT = ftdmchan->rx_cas_bits;
			}
		}
		break;
	case FTDM_COMMAND_FLUSH_TX_BUFFERS:
		{
			int flushmode = ZT_FLUSH_WRITE;
			err = ioctl(ftdmchan->sockfd, codes.FLUSH, &flushmode);
		}
		break;
	case FTDM_COMMAND_SET_POLARITY:
		{
			ftdm_polarity_t polarity = FTDM_COMMAND_OBJ_INT;
			err = ioctl(ftdmchan->sockfd, codes.SETPOLARITY, polarity);
			if (!err) {
				ftdmchan->polarity = polarity;
			}
		}
		break;
	case FTDM_COMMAND_FLUSH_RX_BUFFERS:
		{
			int flushmode = ZT_FLUSH_READ;
			err = ioctl(ftdmchan->sockfd, codes.FLUSH, &flushmode);
		}
		break;
	case FTDM_COMMAND_FLUSH_BUFFERS:
		{
			int flushmode = ZT_FLUSH_BOTH;
			err = ioctl(ftdmchan->sockfd, codes.FLUSH, &flushmode);
		}
		break;
	case FTDM_COMMAND_SET_RX_QUEUE_SIZE:
	case FTDM_COMMAND_SET_TX_QUEUE_SIZE:
		/* little white lie ... eventually we can implement this, in the meantime, not worth the effort
		   and this is only used by some sig modules such as ftmod_r2 to behave bettter under load */
		err = 0;
		break;
	case FTDM_COMMAND_ENABLE_DTMF_DETECT:
		{
			zt_tone_mode_t mode = ZT_TONEDETECT_ON | ZT_TONEDETECT_MUTE;
			err = ioctl(ftdmchan->sockfd, codes.TONEDETECT, &mode);
		}
		break;
	case FTDM_COMMAND_DISABLE_DTMF_DETECT:
		{
			zt_tone_mode_t mode = 0;
			err = ioctl(ftdmchan->sockfd, codes.TONEDETECT, &mode);
		}
		break;
	default:
		err = FTDM_NOTIMPL;
		break;
	};

	if (err && err != FTDM_NOTIMPL) {
		snprintf(ftdmchan->last_error, sizeof(ftdmchan->last_error), "%s", strerror(errno));
		return FTDM_FAIL;
	}


	return err == 0 ? FTDM_SUCCESS : err;
}

/**
 * \brief Gets alarms from a Zaptel/DAHDI channel
 * \param ftdmchan Channel to get alarms from
 * \return Success or failure
 */
static FIO_GET_ALARMS_FUNCTION(zt_get_alarms)
{
	struct zt_spaninfo info;
	zt_params_t params;

	memset(&info, 0, sizeof(info));
	info.span_no = ftdmchan->physical_span_id;

	memset(&params, 0, sizeof(params));

	if (ioctl(CONTROL_FD, codes.SPANSTAT, &info)) {
		snprintf(ftdmchan->last_error, sizeof(ftdmchan->last_error), "ioctl failed (%s)", strerror(errno));
		snprintf(ftdmchan->span->last_error, sizeof(ftdmchan->span->last_error), "ioctl failed (%s)", strerror(errno));
		return FTDM_FAIL;
	}

	ftdmchan->alarm_flags = info.alarms;

	/* get channel alarms if span has no alarms */
	if (info.alarms == FTDM_ALARM_NONE) {
		if (ioctl(ftdmchan->sockfd, codes.GET_PARAMS, &params)) {
			snprintf(ftdmchan->last_error, sizeof(ftdmchan->last_error), "ioctl failed (%s)", strerror(errno));
			snprintf(ftdmchan->span->last_error, sizeof(ftdmchan->span->last_error), "ioctl failed (%s)", strerror(errno));
			return FTDM_FAIL;
		}

		if (params.chan_alarms > 0) {
			if (params.chan_alarms == DAHDI_ALARM_YELLOW) {
				ftdmchan->alarm_flags = FTDM_ALARM_YELLOW;
			}
			else if (params.chan_alarms == DAHDI_ALARM_BLUE) {
				ftdmchan->alarm_flags = FTDM_ALARM_BLUE;
			}
			else {
				ftdmchan->alarm_flags = FTDM_ALARM_RED;
			}
		}
	}

	return FTDM_SUCCESS;
}

#define ftdm_zt_set_event_pending(fchan) \
	do { \
		ftdm_set_io_flag(fchan, FTDM_CHANNEL_IO_EVENT); \
		fchan->last_event_time = ftdm_current_time_in_ms(); \
	} while (0);

#define ftdm_zt_store_chan_event(fchan, revent) \
	do { \
		if (fchan->io_data) { \
			ftdm_log_chan(fchan, FTDM_LOG_WARNING, "Dropping event %d, not retrieved on time\n", revent); \
		} \
		fchan->io_data = (void *)zt_event_id; \
		ftdm_zt_set_event_pending(fchan); \
	} while (0);

/**
 * \brief Waits for an event on a Zaptel/DAHDI channel
 * \param ftdmchan Channel to open
 * \param flags Type of event to wait for
 * \param to Time to wait (in ms)
 * \return Success, failure or timeout
 */
static FIO_WAIT_FUNCTION(zt_wait)
{
	int32_t inflags = 0;
	int result;
	struct pollfd pfds[1];

	if (*flags & FTDM_READ) {
		inflags |= POLLIN;
	}

	if (*flags & FTDM_WRITE) {
		inflags |= POLLOUT;
	}

	if (*flags & FTDM_EVENTS) {
		inflags |= POLLPRI;
	}

pollagain:
	memset(&pfds[0], 0, sizeof(pfds[0]));
	pfds[0].fd = ftdmchan->sockfd;
	pfds[0].events = inflags;
	result = poll(pfds, 1, to);
	*flags = FTDM_NO_FLAGS;

	if (result < 0 && errno == EINTR) {
		ftdm_log_chan_msg(ftdmchan, FTDM_LOG_DEBUG, "DAHDI wait got interrupted, trying again\n");
		goto pollagain;
	}

	if (pfds[0].revents & POLLERR) {
		ftdm_log_chan_msg(ftdmchan, FTDM_LOG_ERROR, "DAHDI device got POLLERR\n");
		result = -1;
	}

	if (result > 0) {
		inflags = pfds[0].revents;
	}

	if (result < 0){
		snprintf(ftdmchan->last_error, sizeof(ftdmchan->last_error), "Poll failed");
		ftdm_log_chan(ftdmchan, FTDM_LOG_ERROR, "Failed to poll DAHDI device: %s\n", strerror(errno));
		return FTDM_FAIL;
	}

	if (result == 0) {
		return FTDM_TIMEOUT;
	}

	if (inflags & POLLIN) {
		*flags |= FTDM_READ;
	}

	if (inflags & POLLOUT) {
		*flags |= FTDM_WRITE;
	}

	if ((inflags & POLLPRI) || (ftdmchan->io_data && (*flags & FTDM_EVENTS))) {
		*flags |= FTDM_EVENTS;
	}

	return FTDM_SUCCESS;

}

/**
 * \brief Checks for events on a Zaptel/DAHDI span
 * \param span Span to check for events
 * \param ms Time to wait for event
 * \return Success if event is waiting or failure if not
 */
FIO_SPAN_POLL_EVENT_FUNCTION(zt_poll_event)
{
	struct pollfd pfds[FTDM_MAX_CHANNELS_SPAN];
	uint32_t i, j = 0, k = 0;
	int r;
	
	for(i = 1; i <= span->chan_count; i++) {
		memset(&pfds[j], 0, sizeof(pfds[j]));
		pfds[j].fd = span->channels[i]->sockfd;
		pfds[j].events = POLLPRI;
		j++;
	}

	r = poll(pfds, j, ms);

	if (r == 0) {
		return FTDM_TIMEOUT;
	} else if (r < 0) {
		snprintf(span->last_error, sizeof(span->last_error), "%s", strerror(errno));
		return FTDM_FAIL;
	}

	for(i = 1; i <= span->chan_count; i++) {

		ftdm_channel_lock(span->channels[i]);

 		if (pfds[i-1].revents & POLLERR) {
			ftdm_log_chan(span->channels[i], FTDM_LOG_ERROR, "POLLERR, flags=%d\n", pfds[i-1].events);

			ftdm_channel_unlock(span->channels[i]);

			continue;
		}
		if ((pfds[i-1].revents & POLLPRI) || (span->channels[i]->io_data)) {
			ftdm_zt_set_event_pending(span->channels[i]);
			k++;
		}
		if (pfds[i-1].revents & POLLIN) {
			ftdm_set_io_flag(span->channels[i], FTDM_CHANNEL_IO_READ);
		}
		if (pfds[i-1].revents & POLLOUT) {
			ftdm_set_io_flag(span->channels[i], FTDM_CHANNEL_IO_WRITE);
		}

		ftdm_channel_unlock(span->channels[i]);

	}

	if (!k) {
		snprintf(span->last_error, sizeof(span->last_error), "no matching descriptor");
	}

	return k ? FTDM_SUCCESS : FTDM_FAIL;
}

static __inline__ int handle_dtmf_event(ftdm_channel_t *fchan, zt_event_t zt_event_id)
{
	if ((zt_event_id & ZT_EVENT_DTMFUP)) {
		int digit = (zt_event_id & (~ZT_EVENT_DTMFUP));
		char tmp_dtmf[2] = { digit, 0 };
		ftdm_log_chan(fchan, FTDM_LOG_DEBUG, "DTMF UP [%d]\n", digit);
		ftdm_channel_queue_dtmf(fchan, tmp_dtmf);
		return 0;
	} else if ((zt_event_id & ZT_EVENT_DTMFDOWN)) {
		int digit = (zt_event_id & (~ZT_EVENT_DTMFDOWN));
		ftdm_log_chan(fchan, FTDM_LOG_DEBUG, "DTMF DOWN [%d]\n", digit);
		return 0;
	} else {
		return -1;
	}
}

/**
 * \brief Process an event from a ftdmchan and set the proper OOB event_id. The channel must be locked.
 * \param fchan Channel to retrieve event from
 * \param event_id Pointer to OOB event id
 * \param zt_event_id Zaptel event id
 * \return FTDM_SUCCESS or FTDM_FAIL
 */
static __inline__ ftdm_status_t zt_channel_process_event(ftdm_channel_t *fchan, ftdm_oob_event_t *event_id, zt_event_t zt_event_id)
{
	ftdm_log_chan(fchan, FTDM_LOG_DEBUG, "Processing zap hardware event %d\n", zt_event_id);
	switch(zt_event_id) {
	case ZT_EVENT_RINGEROFF:
		{
			ftdm_log_chan_msg(fchan, FTDM_LOG_DEBUG, "ZT RINGER OFF\n");
			*event_id = FTDM_OOB_NOOP;
		}
		break;
	case ZT_EVENT_RINGERON:
		{
			ftdm_log_chan_msg(fchan, FTDM_LOG_DEBUG, "ZT RINGER ON\n");
			*event_id = FTDM_OOB_NOOP;
		}
		break;
	case ZT_EVENT_RINGBEGIN:
		{
			*event_id = FTDM_OOB_RING_START;
		}
		break;
	case ZT_EVENT_ONHOOK:
		{
			*event_id = FTDM_OOB_ONHOOK;
		}
		break;
	case ZT_EVENT_WINKFLASH:
		{
			if (fchan->state == FTDM_CHANNEL_STATE_DOWN || fchan->state == FTDM_CHANNEL_STATE_DIALING) {
				*event_id = FTDM_OOB_WINK;
			} else {
				*event_id = FTDM_OOB_FLASH;
			}
		}
		break;
	case ZT_EVENT_RINGOFFHOOK:
		{
			*event_id = FTDM_OOB_NOOP;
			if (fchan->type == FTDM_CHAN_TYPE_FXS || (fchan->type == FTDM_CHAN_TYPE_EM && fchan->state != FTDM_CHANNEL_STATE_UP)) {
				if (fchan->type != FTDM_CHAN_TYPE_EM) {
					/* In E&M we're supposed to set this flag only when the local side goes offhook, not the remote */
					ftdm_set_flag_locked(fchan, FTDM_CHANNEL_OFFHOOK);
				}

				/* For E&M let's count the ring count (it seems sometimes we receive RINGOFFHOOK once before the other end
				 * answers, then another RINGOFFHOOK when the other end answers?? anyways, now we count rings before delivering the
				 * offhook event ... the E&M signaling code in ftmod_analog_em also polls the RBS bits looking for answer, just to
				 * be safe and not rely on this event, so even if this event does not arrive, when there is answer supervision
				 * the analog signaling code should detect the cas persistance pattern and answer */
				if (fchan->type == FTDM_CHAN_TYPE_EM && ftdm_test_flag(fchan, FTDM_CHANNEL_OUTBOUND)) {
					fchan->ring_count++;
					/* perhaps some day we'll make this configurable, but since I am not even sure what the hell is going on
					 * no point in making a configuration option for something that may not be technically correct */
					if (fchan->ring_count == 2) {
						*event_id = FTDM_OOB_OFFHOOK;
					}
				} else {
					*event_id = FTDM_OOB_OFFHOOK;
				}
			} else if (fchan->type == FTDM_CHAN_TYPE_FXO) {
				*event_id = FTDM_OOB_RING_START;
			}
		}
		break;
	case ZT_EVENT_ALARM:
		{
			*event_id = FTDM_OOB_ALARM_TRAP;
		}
		break;
	case ZT_EVENT_NOALARM:
		{
			*event_id = FTDM_OOB_ALARM_CLEAR;
		}
		break;
	case ZT_EVENT_BITSCHANGED:
		{
			*event_id = FTDM_OOB_CAS_BITS_CHANGE;
			int bits = 0;
			int err = ioctl(fchan->sockfd, codes.GETRXBITS, &bits);
			if (err) {
				return FTDM_FAIL;
			}
			fchan->rx_cas_bits = bits;
		}
		break;
	case ZT_EVENT_BADFCS:
		{
			ftdm_log_chan_msg(fchan, FTDM_LOG_ERROR, "Bad frame checksum (ZT_EVENT_BADFCS)\n");
			*event_id = FTDM_OOB_NOOP;	/* What else could we do? */
		}
		break;
	case ZT_EVENT_OVERRUN:
		{
			ftdm_log_chan_msg(fchan, FTDM_LOG_ERROR, "HDLC frame overrun (ZT_EVENT_OVERRUN)\n");
			*event_id = FTDM_OOB_NOOP;	/* What else could we do? */
		}
		break;
	case ZT_EVENT_ABORT:
		{
			ftdm_log_chan_msg(fchan, FTDM_LOG_ERROR, "HDLC abort frame received (ZT_EVENT_ABORT)\n");
			*event_id = FTDM_OOB_NOOP;	/* What else could we do? */
		}
		break;
	case ZT_EVENT_POLARITY:
		{
			ftdm_log_chan_msg(fchan, FTDM_LOG_ERROR, "Got polarity reverse (ZT_EVENT_POLARITY)\n");
			*event_id = FTDM_OOB_POLARITY_REVERSE;
		}
		break;
	case ZT_EVENT_NONE:
		{
			ftdm_log_chan_msg(fchan, FTDM_LOG_DEBUG, "No event\n");
			*event_id = FTDM_OOB_NOOP;
		}
		break;
	default:
		{
			if (handle_dtmf_event(fchan, zt_event_id)) {
				ftdm_log_chan(fchan, FTDM_LOG_WARNING, "Unhandled event %d\n", zt_event_id);
				*event_id = FTDM_OOB_INVALID;
			} else {
				*event_id = FTDM_OOB_NOOP;
			}
		}
		break;
	}
	return FTDM_SUCCESS;
}

/**
 * \brief Retrieves an event from a ftdm channel
 * \param ftdmchan Channel to retrieve event from
 * \param event FreeTDM event to return
 * \return Success or failure
 */
FIO_CHANNEL_NEXT_EVENT_FUNCTION(zt_channel_next_event)
{
	uint32_t event_id = FTDM_OOB_INVALID;
	zt_event_t zt_event_id = 0;
	ftdm_span_t *span = ftdmchan->span;

	if (ftdm_test_io_flag(ftdmchan, FTDM_CHANNEL_IO_EVENT)) {
		ftdm_clear_io_flag(ftdmchan, FTDM_CHANNEL_IO_EVENT);
	}

	if (ftdmchan->io_data) {
		zt_event_id = (zt_event_t)ftdmchan->io_data;
		ftdmchan->io_data = NULL;
	} else if (ioctl(ftdmchan->sockfd, codes.GETEVENT, &zt_event_id) == -1) {
		ftdm_log_chan(ftdmchan, FTDM_LOG_ERROR, "Failed retrieving event from channel: %s\n",
				strerror(errno));
		return FTDM_FAIL;
	}

	/* the core already locked the channel for us, so it's safe to call zt_channel_process_event() here */
	if ((zt_channel_process_event(ftdmchan, &event_id, zt_event_id)) != FTDM_SUCCESS) {
		ftdm_log_chan(ftdmchan, FTDM_LOG_ERROR, "Failed to process DAHDI event %d from channel\n", zt_event_id);
		return FTDM_FAIL;
	}

	ftdmchan->last_event_time = 0;
	span->event_header.e_type = FTDM_EVENT_OOB;
	span->event_header.enum_id = event_id;
	span->event_header.channel = ftdmchan;
	*event = &span->event_header;
	return FTDM_SUCCESS;
}

/**
 * \brief Retrieves an event from a Zaptel/DAHDI span
 * \param span Span to retrieve event from
 * \param event FreeTDM event to return
 * \return Success or failure
 */
FIO_SPAN_NEXT_EVENT_FUNCTION(zt_next_event)
{
	uint32_t i, event_id = FTDM_OOB_INVALID;
	zt_event_t zt_event_id = 0;

	for (i = 1; i <= span->chan_count; i++) {
		ftdm_channel_t *fchan = span->channels[i];

		ftdm_channel_lock(fchan);

		if (!ftdm_test_io_flag(fchan, FTDM_CHANNEL_IO_EVENT)) {

			ftdm_channel_unlock(fchan);

			continue;
		}

		ftdm_clear_io_flag(fchan, FTDM_CHANNEL_IO_EVENT);

		if (fchan->io_data) {
			zt_event_id = (zt_event_t)fchan->io_data;
			fchan->io_data = NULL;
		} else if (ioctl(fchan->sockfd, codes.GETEVENT, &zt_event_id) == -1) {
			ftdm_log_chan(fchan, FTDM_LOG_ERROR, "Failed to retrieve DAHDI event from channel: %s\n", strerror(errno));

			ftdm_channel_unlock(fchan);

			continue;
		}

		if ((zt_channel_process_event(fchan, &event_id, zt_event_id)) != FTDM_SUCCESS) {
			ftdm_log_chan(fchan, FTDM_LOG_ERROR, "Failed to process DAHDI event %d from channel\n", zt_event_id);

			ftdm_channel_unlock(fchan);

			return FTDM_FAIL;
		}

		fchan->last_event_time = 0;
		span->event_header.e_type = FTDM_EVENT_OOB;
		span->event_header.enum_id = event_id;
		span->event_header.channel = fchan;
		*event = &span->event_header;

		ftdm_channel_unlock(fchan);

		return FTDM_SUCCESS;
	}

	return FTDM_FAIL;
}

/**
 * \brief Reads data from a Zaptel/DAHDI channel
 * \param ftdmchan Channel to read from
 * \param data Data buffer
 * \param datalen Size of data buffer
 * \return Success, failure or timeout
 */
static FIO_READ_FUNCTION(zt_read)
{
	ftdm_ssize_t r = 0;
	int read_errno = 0;
	int errs = 0;

	while (errs++ < 30) {
		r = read(ftdmchan->sockfd, data, *datalen);
		if (r > 0) {
			/* successful read, bail out now ... */
			break;
		}

		/* Timeout ... retry after a bit */
		if (r == 0) {
			ftdm_sleep(10);
			if (errs) errs--;
			continue;
		}

		/* This gotta be an error, save errno in case we do printf(), ioctl() or other operations which may reset it */
		read_errno = errno;
		if (read_errno == EAGAIN || read_errno == EINTR) {
			/* Reasonable to retry under those errors */
			continue;
		}

		/* When ELAST is returned, it means DAHDI has an out of band event ready and we won't be able to read anything until
		 * we retrieve the event using an ioctl(), so we try to retrieve it here ... */
		if (read_errno == ELAST) {
			zt_event_t zt_event_id = 0;
			if (ioctl(ftdmchan->sockfd, codes.GETEVENT, &zt_event_id) == -1) {
				ftdm_log_chan(ftdmchan, FTDM_LOG_ERROR, "Failed retrieving event after ELAST on read: %s\n", strerror(errno));
				r = -1;
				break;
			}

			if (handle_dtmf_event(ftdmchan, zt_event_id)) {
				/* Enqueue this event for later */
				ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Deferring event %d to be able to read data\n", zt_event_id);
				ftdm_zt_store_chan_event(ftdmchan, zt_event_id);
			} else {
				ftdm_log_chan_msg(ftdmchan, FTDM_LOG_DEBUG, "Skipping one IO read cycle due to DTMF event processing\n");
			}
			continue;
		}

		/* Read error, keep going unless to many errors force us to abort ...*/
		ftdm_log(FTDM_LOG_ERROR, "IO read failed: %s\n", strerror(read_errno));
	}

	if (r > 0) {
		*datalen = r;
		if (ftdmchan->type == FTDM_CHAN_TYPE_DQ921) {
			*datalen -= 2;
		}
		return FTDM_SUCCESS;
	}
	else if (read_errno == ELAST) {
		return FTDM_SUCCESS;
	}
	return r == 0 ? FTDM_TIMEOUT : FTDM_FAIL;
}

/**
 * \brief Writes data to a Zaptel/DAHDI channel
 * \param ftdmchan Channel to write to
 * \param data Data buffer
 * \param datalen Size of data buffer
 * \return Success or failure
 */
static FIO_WRITE_FUNCTION(zt_write)
{
	ftdm_ssize_t w = 0;
	ftdm_size_t bytes = *datalen;

	if (ftdmchan->type == FTDM_CHAN_TYPE_DQ921) {
		memset(data+bytes, 0, 2);
		bytes += 2;
	}

tryagain:
	w = write(ftdmchan->sockfd, data, bytes);
	
	if (w >= 0) {
		*datalen = w;
		return FTDM_SUCCESS;
	}

	if (errno == ELAST) {
		zt_event_t zt_event_id = 0;
		if (ioctl(ftdmchan->sockfd, codes.GETEVENT, &zt_event_id) == -1) {
			ftdm_log_chan(ftdmchan, FTDM_LOG_ERROR, "Failed retrieving event after ELAST on write: %s\n", strerror(errno));
			return FTDM_FAIL;
		}

		if (handle_dtmf_event(ftdmchan, zt_event_id)) {
			/* Enqueue this event for later */
			ftdm_log_chan(ftdmchan, FTDM_LOG_DEBUG, "Deferring event %d to be able to write data\n", zt_event_id);
			ftdm_zt_store_chan_event(ftdmchan, zt_event_id);
		}

		goto tryagain;
	}

	return FTDM_FAIL;
}

/**
 * \brief Destroys a Zaptel/DAHDI Channel
 * \param ftdmchan Channel to destroy
 * \return Success
 */
static FIO_CHANNEL_DESTROY_FUNCTION(zt_channel_destroy)
{
	close(ftdmchan->sockfd);
	ftdmchan->sockfd = ZT_INVALID_SOCKET;
	return FTDM_SUCCESS;
}

/**
 * \brief Global FreeTDM IO interface for Zaptel/DAHDI
 */
static ftdm_io_interface_t zt_interface;

/**
 * \brief Loads Zaptel/DAHDI IO module
 * \param fio FreeTDM IO interface
 * \return Success or failure
 */
static FIO_IO_LOAD_FUNCTION(zt_init)
{
	assert(fio != NULL);
    struct stat statbuf;
	memset(&zt_interface, 0, sizeof(zt_interface));
	memset(&zt_globals, 0, sizeof(zt_globals));

    if (!stat(zt_ctlpath, &statbuf)) {
        ftdm_log(FTDM_LOG_NOTICE, "Using Zaptel control device\n");
        ctlpath = zt_ctlpath;
        chanpath = zt_chanpath;
        memcpy(&codes, &zt_ioctl_codes, sizeof(codes));
    } else if (!stat(dahdi_ctlpath, &statbuf)) {
        ftdm_log(FTDM_LOG_NOTICE, "Using DAHDI control device\n");
        ctlpath = dahdi_ctlpath;
        chanpath = dahdi_chanpath;
        memcpy(&codes, &dahdi_ioctl_codes, sizeof(codes));
    } else {
		ftdm_log(FTDM_LOG_ERROR, "No DAHDI or Zap control device found in /dev/\n");
		return FTDM_FAIL;
    }
	if ((CONTROL_FD = open(ctlpath, O_RDWR)) < 0) {
		ftdm_log(FTDM_LOG_ERROR, "Cannot open control device %s: %s\n", ctlpath, strerror(errno));
		return FTDM_FAIL;
	}

	zt_globals.codec_ms = 20;
	zt_globals.wink_ms = 150;
	zt_globals.flash_ms = 750;
	zt_globals.eclevel = 0;
	zt_globals.etlevel = 0;
	
	zt_interface.name = "zt";
	zt_interface.configure = zt_configure;
	zt_interface.configure_span = zt_configure_span;
	zt_interface.open = zt_open;
	zt_interface.close = zt_close;
	zt_interface.command = zt_command;
	zt_interface.wait = zt_wait;
	zt_interface.read = zt_read;
	zt_interface.write = zt_write;
	zt_interface.poll_event = zt_poll_event;
	zt_interface.next_event = zt_next_event;
	zt_interface.channel_next_event = zt_channel_next_event;
	zt_interface.channel_destroy = zt_channel_destroy;
	zt_interface.get_alarms = zt_get_alarms;
	*fio = &zt_interface;

	return FTDM_SUCCESS;
}

/**
 * \brief Unloads Zaptel/DAHDI IO module
 * \return Success
 */
static FIO_IO_UNLOAD_FUNCTION(zt_destroy)
{
	close(CONTROL_FD);
	memset(&zt_interface, 0, sizeof(zt_interface));
	return FTDM_SUCCESS;
}

/**
 * \brief FreeTDM Zaptel/DAHDI IO module definition
 */
ftdm_module_t ftdm_module = { 
	"zt",
	zt_init,
	zt_destroy,
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
