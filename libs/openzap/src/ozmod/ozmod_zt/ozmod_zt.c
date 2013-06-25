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
 */


#include "openzap.h"
#include "ozmod_zt.h"

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

/**
 * \brief General IOCTL codes
 */
struct ioctl_codes {
    int GET_BLOCKSIZE;
    int SET_BLOCKSIZE;
    int FLUSH;
    int SYNC;
    int GET_PARAMS;
    int SET_PARAMS;
    int HOOK;
    int GETEVENT;
    int IOMUX;
    int SPANSTAT;
    int MAINT;
    int GETCONF;
    int SETCONF;
    int CONFLINK;
    int CONFDIAG;
    int GETGAINS;
    int SETGAINS;
    int SPANCONFIG;
    int CHANCONFIG;
    int SET_BUFINFO;
    int GET_BUFINFO;
    int AUDIOMODE;
    int ECHOCANCEL;
    int HDLCRAWMODE;
    int HDLCFCSMODE;
    int SPECIFY;
    int SETLAW;
    int SETLINEAR;
    int GETCONFMUTE;
    int ECHOTRAIN;
    int SETTXBITS;
    int GETRXBITS;
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
    .GETRXBITS = ZT_GETRXBITS
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
    .GETRXBITS = DAHDI_GETRXBITS
};

#define ZT_INVALID_SOCKET -1
static struct ioctl_codes codes;
static const char *ctlpath = NULL;
static const char *chanpath = NULL;

static const char dahdi_ctlpath[] = "/dev/dahdi/ctl";
static const char dahdi_chanpath[] = "/dev/dahdi/channel";

static const char zt_ctlpath[] = "/dev/zap/ctl";
static const char zt_chanpath[] = "/dev/zap/channel";

static zap_socket_t CONTROL_FD = ZT_INVALID_SOCKET;

ZIO_SPAN_NEXT_EVENT_FUNCTION(zt_next_event);
ZIO_SPAN_POLL_EVENT_FUNCTION(zt_poll_event);

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
	case ZAP_CODEC_ALAW:
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
	case ZAP_CODEC_ULAW:
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
 * \brief Initialises a range of zaptel channels
 * \param span Openzap span
 * \param start Initial wanpipe channel number
 * \param end Final wanpipe channel number
 * \param type Openzap channel type
 * \param name Openzap span name
 * \param number Openzap span number
 * \param cas_bits CAS bits
 * \return number of spans configured
 */
static unsigned zt_open_range(zap_span_t *span, unsigned start, unsigned end, zap_chan_type_t type, char *name, char *number, unsigned char cas_bits)
{
	unsigned configured = 0, x;
	zt_params_t ztp;

	memset(&ztp, 0, sizeof(ztp));

	if (type == ZAP_CHAN_TYPE_CAS) {
		zap_log(ZAP_LOG_DEBUG, "Configuring CAS channels with abcd == 0x%X\n", cas_bits);
	}	
	for(x = start; x < end; x++) {
		zap_channel_t *zchan;
		zap_socket_t sockfd = ZT_INVALID_SOCKET;
		int len;

		sockfd = open(chanpath, O_RDWR);
		if (sockfd != ZT_INVALID_SOCKET && zap_span_add_channel(span, sockfd, type, &zchan) == ZAP_SUCCESS) {

			if (ioctl(sockfd, codes.SPECIFY, &x)) {
				zap_log(ZAP_LOG_ERROR, "failure configuring device %s chan %d fd %d (%s)\n", chanpath, x, sockfd, strerror(errno));
				close(sockfd);
				continue;
			}

			if (zchan->type == ZAP_CHAN_TYPE_DQ921) {
				struct zt_bufferinfo binfo;
				memset(&binfo, 0, sizeof(binfo));
				binfo.txbufpolicy = 0;
				binfo.rxbufpolicy = 0;
				binfo.numbufs = 32;
				binfo.bufsize = 1024;
				if (ioctl(sockfd, codes.SET_BUFINFO, &binfo)) {
					zap_log(ZAP_LOG_ERROR, "failure configuring device %s as OpenZAP device %d:%d fd:%d\n", chanpath, zchan->span_id, zchan->chan_id, sockfd);
					close(sockfd);
					continue;
				}
			}

			if (type == ZAP_CHAN_TYPE_FXS || type == ZAP_CHAN_TYPE_FXO) {
				struct zt_chanconfig cc;
				memset(&cc, 0, sizeof(cc));
				cc.chan = cc.master = x;
				
				switch(type) {
				case ZAP_CHAN_TYPE_FXS:
					{
						switch(span->start_type) {
						case ZAP_ANALOG_START_KEWL:
							cc.sigtype = ZT_SIG_FXOKS;
							break;
						case ZAP_ANALOG_START_LOOP:
							cc.sigtype = ZT_SIG_FXOLS;
							break;
						case ZAP_ANALOG_START_GROUND:
							cc.sigtype = ZT_SIG_FXOGS;
							break;
						default:
							break;
						}
					}
					break;
				case ZAP_CHAN_TYPE_FXO:
					{
						switch(span->start_type) {
						case ZAP_ANALOG_START_KEWL:
							cc.sigtype = ZT_SIG_FXSKS;
							break;
						case ZAP_ANALOG_START_LOOP:
							cc.sigtype = ZT_SIG_FXSLS;
							break;
						case ZAP_ANALOG_START_GROUND:
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
					zap_log(ZAP_LOG_WARNING, "this ioctl fails on older zaptel but is harmless if you used ztcfg\n[device %s chan %d fd %d (%s)]\n", chanpath, x, CONTROL_FD, strerror(errno));
				}
			}

			if (type == ZAP_CHAN_TYPE_CAS) {
				struct zt_chanconfig cc;
				memset(&cc, 0, sizeof(cc));
				cc.chan = cc.master = x;
				cc.sigtype = ZT_SIG_CAS;
				cc.idlebits = cas_bits;
				if (ioctl(CONTROL_FD, codes.CHANCONFIG, &cc)) {
					zap_log(ZAP_LOG_ERROR, "failure configuring device %s as OpenZAP device %d:%d fd:%d err:%s", chanpath, zchan->span_id, zchan->chan_id, sockfd, strerror(errno));
					close(sockfd);
					continue;
				}
			}

			if (zchan->type != ZAP_CHAN_TYPE_DQ921 && zchan->type != ZAP_CHAN_TYPE_DQ931) {
				len = zt_globals.codec_ms * 8;
				if (ioctl(zchan->sockfd, codes.SET_BLOCKSIZE, &len)) {
					zap_log(ZAP_LOG_ERROR, "failure configuring device %s as OpenZAP device %d:%d fd:%d err:%s\n", 
							chanpath, zchan->span_id, zchan->chan_id, sockfd, strerror(errno));
					close(sockfd);
					continue;
				}

				zchan->packet_len = len;
				zchan->effective_interval = zchan->native_interval = zchan->packet_len / 8;
			
				if (zchan->effective_codec == ZAP_CODEC_SLIN) {
					zchan->packet_len *= 2;
				}
			}
			
			if (ioctl(sockfd, codes.GET_PARAMS, &ztp) < 0) {
				zap_log(ZAP_LOG_ERROR, "failure configuring device %s as OpenZAP device %d:%d fd:%d\n", chanpath, zchan->span_id, zchan->chan_id, sockfd);
				close(sockfd);
				continue;
			}

			if (zchan->type == ZAP_CHAN_TYPE_DQ921) {
				if (
					(ztp.sig_type != ZT_SIG_HDLCRAW) &&
					(ztp.sig_type != ZT_SIG_HDLCFCS) &&
					(ztp.sig_type != ZT_SIG_HARDHDLC)
					) {
					zap_log(ZAP_LOG_ERROR, "Failure configuring device %s as OpenZAP device %d:%d fd:%d, hardware signaling is not HDLC, fix your Zap/DAHDI configuration!\n", chanpath, zchan->span_id, zchan->chan_id, sockfd);
					close(sockfd);
					continue;
				}
			}

			zap_log(ZAP_LOG_INFO, "configuring device %s channel %d as OpenZAP device %d:%d fd:%d\n", chanpath, x, zchan->span_id, zchan->chan_id, sockfd);
			
			zchan->rate = 8000;
			zchan->physical_span_id = ztp.span_no;
			zchan->physical_chan_id = ztp.chan_no;
			
			if (type == ZAP_CHAN_TYPE_FXS || type == ZAP_CHAN_TYPE_FXO || type == ZAP_CHAN_TYPE_EM || type == ZAP_CHAN_TYPE_B) {
				if (ztp.g711_type == ZT_G711_ALAW) {
					zchan->native_codec = zchan->effective_codec = ZAP_CODEC_ALAW;
				} else if (ztp.g711_type == ZT_G711_MULAW) {
					zchan->native_codec = zchan->effective_codec = ZAP_CODEC_ULAW;
				} else {
					int type;

					if (zchan->span->trunk_type == ZAP_TRUNK_E1) {
						type = ZAP_CODEC_ALAW;
					} else {
						type = ZAP_CODEC_ULAW;
					}

					zchan->native_codec = zchan->effective_codec = type;

				}
			}

			ztp.wink_time = zt_globals.wink_ms;
			ztp.flash_time = zt_globals.flash_ms;

			if (ioctl(sockfd, codes.SET_PARAMS, &ztp) < 0) {
				zap_log(ZAP_LOG_ERROR, "failure configuring device %s as OpenZAP device %d:%d fd:%d\n", chanpath, zchan->span_id, zchan->chan_id, sockfd);
				close(sockfd);
				continue;
			}

			if (!zap_strlen_zero(name)) {
				zap_copy_string(zchan->chan_name, name, sizeof(zchan->chan_name));
			}
			if (!zap_strlen_zero(number)) {
				zap_copy_string(zchan->chan_number, number, sizeof(zchan->chan_number));
			}
			configured++;
		} else {
			zap_log(ZAP_LOG_ERROR, "failure configuring device %s\n", chanpath);
		}
	}
	


	return configured;
}

/**
 * \brief Initialises an openzap zaptel span from a configuration string
 * \param span Openzap span
 * \param str Configuration string
 * \param type Openzap span type
 * \param name Openzap span name
 * \param number Openzap span number
 * \return Success or failure
 */
static ZIO_CONFIGURE_SPAN_FUNCTION(zt_configure_span)
{

	int items, i;
	char *mydata, *item_list[10];
	char *ch, *mx;
	unsigned char cas_bits = 0;
	int channo;
	int top = 0;
	unsigned configured = 0;

	assert(str != NULL);
	

	mydata = strdup(str);
	assert(mydata != NULL);


	items = zap_separate_string(mydata, ',', item_list, (sizeof(item_list) / sizeof(item_list[0])));

	for(i = 0; i < items; i++) {
		ch = item_list[i];

		if (!(ch)) {
			zap_log(ZAP_LOG_ERROR, "Invalid input\n");
			continue;
		}

		channo = atoi(ch);
		
		if (channo < 0) {
			zap_log(ZAP_LOG_ERROR, "Invalid channel number %d\n", channo);
			continue;
		}

		if ((mx = strchr(ch, '-'))) {
			mx++;
			top = atoi(mx) + 1;
		} else {
			top = channo + 1;
		}
		
		
		if (top < 0) {
			zap_log(ZAP_LOG_ERROR, "Invalid range number %d\n", top);
			continue;
		}
		if (ZAP_CHAN_TYPE_CAS == type && zap_config_get_cas_bits(ch, &cas_bits)) {
			zap_log(ZAP_LOG_ERROR, "Failed to get CAS bits in CAS channel\n");
			continue;
		}
		configured += zt_open_range(span, channo, top, type, name, number, cas_bits);

	}
	
	free(mydata);

	return configured;

}

/**
 * \brief Process configuration variable for a zaptel profile
 * \param category Wanpipe profile name
 * \param var Variable name
 * \param val Variable value
 * \param lineno Line number from configuration file
 * \return Success
 */
static ZIO_CONFIGURE_FUNCTION(zt_configure)
{

	int num;
    float fnum;

	if (!strcasecmp(category, "defaults")) {
		if (!strcasecmp(var, "codec_ms")) {
			num = atoi(val);
			if (num < 10 || num > 60) {
				zap_log(ZAP_LOG_WARNING, "invalid codec ms at line %d\n", lineno);
			} else {
				zt_globals.codec_ms = num;
			}
		} else if (!strcasecmp(var, "wink_ms")) {
			num = atoi(val);
			if (num < 50 || num > 3000) {
				zap_log(ZAP_LOG_WARNING, "invalid wink ms at line %d\n", lineno);
			} else {
				zt_globals.wink_ms = num;
			}
		} else if (!strcasecmp(var, "flash_ms")) {
			num = atoi(val);
			if (num < 50 || num > 3000) {
				zap_log(ZAP_LOG_WARNING, "invalid flash ms at line %d\n", lineno);
			} else {
				zt_globals.flash_ms = num;
			}
		} else if (!strcasecmp(var, "echo_cancel_level")) {
			num = atoi(val);
			if (num < 0 || num > 256) {
                zap_log(ZAP_LOG_WARNING, "invalid echo can val at line %d\n", lineno);
            } else {
                zt_globals.eclevel = num;
            }
		} else if (!strcasecmp(var, "echo_train_level")) {
			if (zt_globals.eclevel < 1) {
				zap_log(ZAP_LOG_WARNING, "can't set echo train level without setting echo cancel level first at line %d\n", lineno);
			} else {
				num = atoi(val);
				if (num < 0 || num > 256) {
					zap_log(ZAP_LOG_WARNING, "invalid echo can val at line %d\n", lineno);
				} else {
					zt_globals.etlevel = num;
				}
			}
		} else if (!strcasecmp(var, "rxgain")) {
			fnum = (float)atof(val);
			if (fnum < -100.0 || fnum > 100.0) {
                zap_log(ZAP_LOG_WARNING, "invalid rxgain val at line %d\n", lineno);
            } else {
                zt_globals.rxgain = fnum;
                zap_log(ZAP_LOG_INFO, "Setting rxgain val to %f\n", fnum);
            }
			
		} else if (!strcasecmp(var, "txgain")) {
			fnum = (float)atof(val);
			if (fnum < -100.0 || fnum > 100.0) {
                zap_log(ZAP_LOG_WARNING, "invalid txgain val at line %d\n", lineno);
            } else {
                zt_globals.txgain = fnum;
                zap_log(ZAP_LOG_INFO, "Setting txgain val to %f\n", fnum);
            }
			
		}
	}

	return ZAP_SUCCESS;
}

/**
 * \brief Opens a zaptel channel
 * \param zchan Channel to open
 * \return Success or failure
 */
static ZIO_OPEN_FUNCTION(zt_open) 
{
	zap_channel_set_feature(zchan, ZAP_CHANNEL_FEATURE_INTERVAL);

	if (zchan->type == ZAP_CHAN_TYPE_DQ921 || zchan->type == ZAP_CHAN_TYPE_DQ931) {
		zchan->native_codec = zchan->effective_codec = ZAP_CODEC_NONE;
	} else {
		int blocksize = zt_globals.codec_ms * (zchan->rate / 1000);
		int err;
		if ((err = ioctl(zchan->sockfd, codes.SET_BLOCKSIZE, &blocksize))) {
			snprintf(zchan->last_error, sizeof(zchan->last_error), "%s", strerror(errno));
			return ZAP_FAIL;
		} else {
			zchan->effective_interval = zchan->native_interval;
			zchan->packet_len = blocksize;
			zchan->native_codec = zchan->effective_codec;
		}
		
		if (zchan->type == ZAP_CHAN_TYPE_B) {
			int one = 1;
			if (ioctl(zchan->sockfd, codes.AUDIOMODE, &one)) {
				snprintf(zchan->last_error, sizeof(zchan->last_error), "%s", strerror(errno));
				zap_log(ZAP_LOG_ERROR, "%s\n", zchan->last_error);
				return ZAP_FAIL;
			}
		}

        if (zt_globals.rxgain || zt_globals.txgain) {
            struct zt_gains gains;
            memset(&gains, 0, sizeof(gains));

            gains.chan_no = zchan->physical_chan_id;
            zt_build_gains(&gains, zt_globals.rxgain, zt_globals.txgain, zchan->native_codec);

            if(zt_globals.rxgain)
                zap_log(ZAP_LOG_INFO, "Setting rxgain to %f on channel %d\n", zt_globals.rxgain, gains.chan_no);

            if(zt_globals.txgain)
                zap_log(ZAP_LOG_INFO, "Setting txgain to %f on channel %d\n", zt_globals.txgain, gains.chan_no);

			if (ioctl(zchan->sockfd, codes.SETGAINS, &gains) < 0) {
				zap_log(ZAP_LOG_ERROR, "failure configuring device %s as OpenZAP device %d:%d fd:%d\n", chanpath, zchan->span_id, zchan->chan_id, zchan->sockfd);
   			}
        }

		if (zt_globals.eclevel >= 0) {
			int len = zt_globals.eclevel;

			if (len) {
				zap_log(ZAP_LOG_INFO, "Setting echo cancel to %d taps for %d:%d\n", len, zchan->span_id, zchan->chan_id);
			} else {
				zap_log(ZAP_LOG_INFO, "Disable echo cancel for %d:%d\n", zchan->span_id, zchan->chan_id);
			}

			if (ioctl(zchan->sockfd, codes.ECHOCANCEL, &len)) {
                		zap_log(ZAP_LOG_WARNING, "Echo cancel not available for %d:%d\n", zchan->span_id, zchan->chan_id);
			} else if (zt_globals.etlevel > 0) {
				len = zt_globals.etlevel;
				if (ioctl(zchan->sockfd, codes.ECHOTRAIN, &len)) {
							zap_log(ZAP_LOG_WARNING, "Echo training not available for %d:%d\n", zchan->span_id, zchan->chan_id);
				}
			}
		}
		
	}
	return ZAP_SUCCESS;
}

/**
 * \brief Closes zaptel channel
 * \param zchan Channel to close
 * \return Success
 */
static ZIO_CLOSE_FUNCTION(zt_close)
{
	return ZAP_SUCCESS;
}

/**
 * \brief Executes an Openzap command on a zaptel channel
 * \param zchan Channel to execute command on
 * \param command Openzap command to execute
 * \param obj Object (unused)
 * \return Success or failure
 */
static ZIO_COMMAND_FUNCTION(zt_command)
{
	zt_params_t ztp;
	int err = 0;

	memset(&ztp, 0, sizeof(ztp));

	switch(command) {
	case ZAP_COMMAND_ENABLE_ECHOCANCEL:
		{
			int level = ZAP_COMMAND_OBJ_INT;
			err = ioctl(zchan->sockfd, codes.ECHOCANCEL, &level);
			ZAP_COMMAND_OBJ_INT = level;
		}
	case ZAP_COMMAND_DISABLE_ECHOCANCEL:
		{
			int level = 0;
			err = ioctl(zchan->sockfd, codes.ECHOCANCEL, &level);
			ZAP_COMMAND_OBJ_INT = level;
		}
		break;
	case ZAP_COMMAND_ENABLE_ECHOTRAIN:
		{
			int level = ZAP_COMMAND_OBJ_INT;
			err = ioctl(zchan->sockfd, codes.ECHOTRAIN, &level);
			ZAP_COMMAND_OBJ_INT = level;
		}
	case ZAP_COMMAND_DISABLE_ECHOTRAIN:
		{
			int level = 0;
			err = ioctl(zchan->sockfd, codes.ECHOTRAIN, &level);
			ZAP_COMMAND_OBJ_INT = level;
		}
		break;
	case ZAP_COMMAND_OFFHOOK:
		{
			int command = ZT_OFFHOOK;
			if (ioctl(zchan->sockfd, codes.HOOK, &command)) {
				snprintf(zchan->last_error, sizeof(zchan->last_error), "OFFHOOK Failed");
				return ZAP_FAIL;
			}
			zap_set_flag_locked(zchan, ZAP_CHANNEL_OFFHOOK);
		}
		break;
	case ZAP_COMMAND_ONHOOK:
		{
			int command = ZT_ONHOOK;
			if (ioctl(zchan->sockfd, codes.HOOK, &command)) {
				snprintf(zchan->last_error, sizeof(zchan->last_error), "ONHOOK Failed");
				return ZAP_FAIL;
			}
			zap_clear_flag_locked(zchan, ZAP_CHANNEL_OFFHOOK);
		}
		break;
	case ZAP_COMMAND_FLASH:
		{
			int command = ZT_FLASH;
			if (ioctl(zchan->sockfd, codes.HOOK, &command)) {
				snprintf(zchan->last_error, sizeof(zchan->last_error), "FLASH Failed");
				return ZAP_FAIL;
			}
		}
		break;
	case ZAP_COMMAND_WINK:
		{
			int command = ZT_WINK;
			if (ioctl(zchan->sockfd, codes.HOOK, &command)) {
				snprintf(zchan->last_error, sizeof(zchan->last_error), "WINK Failed");
				return ZAP_FAIL;
			}
		}
		break;
	case ZAP_COMMAND_GENERATE_RING_ON:
		{
			int command = ZT_RING;
			if (ioctl(zchan->sockfd, codes.HOOK, &command)) {
				snprintf(zchan->last_error, sizeof(zchan->last_error), "Ring Failed");
				return ZAP_FAIL;
			}
			zap_set_flag_locked(zchan, ZAP_CHANNEL_RINGING);
		}
		break;
	case ZAP_COMMAND_GENERATE_RING_OFF:
		{
			int command = ZT_RINGOFF;
			if (ioctl(zchan->sockfd, codes.HOOK, &command)) {
				snprintf(zchan->last_error, sizeof(zchan->last_error), "Ring-off failed");
				return ZAP_FAIL;
			}
			zap_clear_flag_locked(zchan, ZAP_CHANNEL_RINGING);
		}
		break;
	case ZAP_COMMAND_GET_INTERVAL:
		{

			if (!(err = ioctl(zchan->sockfd, codes.GET_BLOCKSIZE, &zchan->packet_len))) {
				zchan->native_interval = zchan->packet_len / 8;
				if (zchan->effective_codec == ZAP_CODEC_SLIN) {
					zchan->packet_len *= 2;
				}
				ZAP_COMMAND_OBJ_INT = zchan->native_interval;
			} 			
		}
		break;
	case ZAP_COMMAND_SET_INTERVAL: 
		{
			int interval = ZAP_COMMAND_OBJ_INT;
			int len = interval * 8;

			if (!(err = ioctl(zchan->sockfd, codes.SET_BLOCKSIZE, &len))) {
				zchan->packet_len = len;
				zchan->effective_interval = zchan->native_interval = zchan->packet_len / 8;

				if (zchan->effective_codec == ZAP_CODEC_SLIN) {
					zchan->packet_len *= 2;
				}
			}
		}
		break;
	case ZAP_COMMAND_SET_CAS_BITS:
		{
			int bits = ZAP_COMMAND_OBJ_INT;
			err = ioctl(zchan->sockfd, codes.SETTXBITS, &bits);
		}
		break;
	case ZAP_COMMAND_GET_CAS_BITS:
		{
			err = ioctl(zchan->sockfd, codes.GETRXBITS, &zchan->rx_cas_bits);
			if (!err) {
				ZAP_COMMAND_OBJ_INT = zchan->rx_cas_bits;
			}
		}
		break;
	case ZAP_COMMAND_FLUSH_TX_BUFFERS:
		{
			int flushmode = ZT_FLUSH_WRITE;
			err = ioctl(zchan->sockfd, codes.FLUSH, &flushmode);
		}
		break;
	case ZAP_COMMAND_FLUSH_RX_BUFFERS:
		{
			int flushmode = ZT_FLUSH_READ;
			err = ioctl(zchan->sockfd, codes.FLUSH, &flushmode);
		}
		break;
	case ZAP_COMMAND_FLUSH_BUFFERS:
		{
			int flushmode = ZT_FLUSH_BOTH;
			err = ioctl(zchan->sockfd, codes.FLUSH, &flushmode);
		}
		break;
	default:
		err = ZAP_NOTIMPL;
		break;
	};

	if (err && err != ZAP_NOTIMPL) {
		snprintf(zchan->last_error, sizeof(zchan->last_error), "%s", strerror(errno));
		return ZAP_FAIL;
	}


	return err == 0 ? ZAP_SUCCESS : err;
}

/**
 * \brief Gets alarms from a zaptel Channel
 * \param zchan Channel to get alarms from
 * \return Success or failure
 */
static ZIO_GET_ALARMS_FUNCTION(zt_get_alarms)
{
	struct zt_spaninfo info;

	memset(&info, 0, sizeof(info));
	info.span_no = zchan->physical_span_id;

	if (ioctl(CONTROL_FD, codes.SPANSTAT, &info)) {
		snprintf(zchan->last_error, sizeof(zchan->last_error), "ioctl failed (%s)", strerror(errno));
		snprintf(zchan->span->last_error, sizeof(zchan->span->last_error), "ioctl failed (%s)", strerror(errno));
		return ZAP_FAIL;
	}

	zchan->alarm_flags = info.alarms;

	return ZAP_SUCCESS;
}

/**
 * \brief Waits for an event on a zaptel channel
 * \param zchan Channel to open
 * \param flags Type of event to wait for
 * \param to Time to wait (in ms)
 * \return Success, failure or timeout
 */
static ZIO_WAIT_FUNCTION(zt_wait)
{
	int32_t inflags = 0;
	int result;
    struct pollfd pfds[1];

	if (*flags & ZAP_READ) {
		inflags |= POLLIN;
	}

	if (*flags & ZAP_WRITE) {
		inflags |= POLLOUT;
	}

	if (*flags & ZAP_EVENTS) {
		inflags |= POLLPRI;
	}


    memset(&pfds[0], 0, sizeof(pfds[0]));
    pfds[0].fd = zchan->sockfd;
    pfds[0].events = inflags;
    result = poll(pfds, 1, to);
	*flags = 0;

	if (pfds[0].revents & POLLERR) {
		result = -1;
	}

	if (result > 0) {
		inflags = pfds[0].revents;
	}

	*flags = ZAP_NO_FLAGS;

	if (result < 0){
		snprintf(zchan->last_error, sizeof(zchan->last_error), "Poll failed");
		return ZAP_FAIL;
	}

	if (result == 0) {
		return ZAP_TIMEOUT;
	}

	if (inflags & POLLIN) {
		*flags |= ZAP_READ;
	}

	if (inflags & POLLOUT) {
		*flags |= ZAP_WRITE;
	}

	if (inflags & POLLPRI) {
		*flags |= ZAP_EVENTS;
	}

	return ZAP_SUCCESS;

}

/**
 * \brief Checks for events on a zaptel span
 * \param span Span to check for events
 * \param ms Time to wait for event
 * \return Success if event is waiting or failure if not
 */
ZIO_SPAN_POLL_EVENT_FUNCTION(zt_poll_event)
{
	struct pollfd pfds[ZAP_MAX_CHANNELS_SPAN];
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
		return ZAP_TIMEOUT;
	} else if (r < 0 || (pfds[i-1].revents & POLLERR)) {
		snprintf(span->last_error, sizeof(span->last_error), "%s", strerror(errno));
		return ZAP_FAIL;
	}
	
	for(i = 1; i <= span->chan_count; i++) {
		if (pfds[i-1].revents & POLLPRI) {
			zap_set_flag(span->channels[i], ZAP_CHANNEL_EVENT);
			span->channels[i]->last_event_time = zap_current_time_in_ms();
			k++;
		}
	}

	if (!k) {
		snprintf(span->last_error, sizeof(span->last_error), "no matching descriptor");
	}

	return k ? ZAP_SUCCESS : ZAP_FAIL;
}

/**
 * \brief Retrieves an event from a zaptel span
 * \param span Span to retrieve event from
 * \param event Openzap event to return
 * \return Success or failure
 */
ZIO_SPAN_NEXT_EVENT_FUNCTION(zt_next_event)
{
	uint32_t i, event_id = ZAP_OOB_INVALID;
	zt_event_t zt_event_id = 0;

	for(i = 1; i <= span->chan_count; i++) {
		if (zap_test_flag(span->channels[i], ZAP_CHANNEL_EVENT)) {
			zap_clear_flag(span->channels[i], ZAP_CHANNEL_EVENT);
			if (ioctl(span->channels[i]->sockfd, codes.GETEVENT, &zt_event_id) == -1) {
				snprintf(span->last_error, sizeof(span->last_error), "%s", strerror(errno));
				return ZAP_FAIL;
			}

			switch(zt_event_id) {
			case ZT_EVENT_RINGEROFF:
				{
					return ZAP_FAIL;
				}
				break;
			case ZT_EVENT_RINGERON:
				{
					return ZAP_FAIL;
				}
				break;
			case ZT_EVENT_RINGBEGIN:
				{
					event_id = ZAP_OOB_RING_START;
				}
				break;
			case ZT_EVENT_ONHOOK:
				{
					event_id = ZAP_OOB_ONHOOK;
				}
				break;
			case ZT_EVENT_WINKFLASH:
				{
					if (span->channels[i]->state == ZAP_CHANNEL_STATE_DOWN || span->channels[i]->state == ZAP_CHANNEL_STATE_DIALING) {
						event_id = ZAP_OOB_WINK;
					} else {
						event_id = ZAP_OOB_FLASH;
					}
				}
				break;
			case ZT_EVENT_RINGOFFHOOK:
				{
					if (span->channels[i]->type == ZAP_CHAN_TYPE_FXS || (span->channels[i]->type == ZAP_CHAN_TYPE_EM && span->channels[i]->state != ZAP_CHANNEL_STATE_UP)) {
						zap_set_flag_locked(span->channels[i], ZAP_CHANNEL_OFFHOOK);
						event_id = ZAP_OOB_OFFHOOK;
					} else if (span->channels[i]->type == ZAP_CHAN_TYPE_FXO) {
						event_id = ZAP_OOB_RING_START;
					} else {
						event_id = ZAP_OOB_NOOP;
					}
				}
				break;
			case ZT_EVENT_ALARM:
				{
					zap_sigmsg_t sigmsg;
					zap_channel_t *zchan = span->channels[i];
					event_id = ZAP_OOB_ALARM_TRAP;
					memset(&sigmsg, 0, sizeof(sigmsg));
					sigmsg.chan_id = zchan->chan_id;
					sigmsg.span_id = zchan->span_id;
					sigmsg.channel = zchan;
					sigmsg.event_id = ZAP_SIGEVENT_ALARM_TRAP;
					zap_span_send_signal(zchan->span, &sigmsg);
				}
				break;
			case ZT_EVENT_NOALARM:
				{
					zap_sigmsg_t sigmsg;
					zap_channel_t *zchan = span->channels[i];
					event_id = ZAP_OOB_ALARM_CLEAR;
					memset(&sigmsg, 0, sizeof(sigmsg));
					sigmsg.chan_id = zchan->chan_id;
					sigmsg.span_id = zchan->span_id;
					sigmsg.channel = zchan;
					sigmsg.event_id = ZAP_SIGEVENT_ALARM_CLEAR;
					zap_span_send_signal(zchan->span, &sigmsg);
				}
				break;
			case ZT_EVENT_BITSCHANGED:
				{
					event_id = ZAP_OOB_CAS_BITS_CHANGE;
					int bits = 0;
					int err = ioctl(span->channels[i]->sockfd, codes.GETRXBITS, &bits);
					if (err) {
						return ZAP_FAIL;
					}
					span->channels[i]->rx_cas_bits = bits;
				}
				break;
			default:
				{
					zap_log(ZAP_LOG_WARNING, "Unhandled event %d for %d:%d\n", zt_event_id, span->span_id, i);
					event_id = ZAP_OOB_INVALID;
				}
				break;
			}

			span->channels[i]->last_event_time = 0;
			span->event_header.e_type = ZAP_EVENT_OOB;
			span->event_header.enum_id = event_id;
			span->event_header.channel = span->channels[i];
			*event = &span->event_header;
			return ZAP_SUCCESS;
		}
	}

	return ZAP_FAIL;
	
}

/**
 * \brief Reads data from a zaptel channel
 * \param zchan Channel to read from
 * \param data Data buffer
 * \param datalen Size of data buffer
 * \return Success, failure or timeout
 */
static ZIO_READ_FUNCTION(zt_read)
{
	zap_ssize_t r = 0;
	int errs = 0;

	while (errs++ < 30) {
		if ((r = read(zchan->sockfd, data, *datalen)) > 0) {
			break;
		}
		zap_sleep(10);
		if (r == 0) {
			errs--;
		}
	}

	if (r > 0) {
		*datalen = r;
		if (zchan->type == ZAP_CHAN_TYPE_DQ921) {
			*datalen -= 2;
		}
		return ZAP_SUCCESS;
	}

	return r == 0 ? ZAP_TIMEOUT : ZAP_FAIL;
}

/**
 * \brief Writes data to a zaptel channel
 * \param zchan Channel to write to
 * \param data Data buffer
 * \param datalen Size of data buffer
 * \return Success or failure
 */
static ZIO_WRITE_FUNCTION(zt_write)
{
	zap_ssize_t w = 0;
	zap_size_t bytes = *datalen;

	if (zchan->type == ZAP_CHAN_TYPE_DQ921) {
		memset(data+bytes, 0, 2);
		bytes += 2;
	}

	w = write(zchan->sockfd, data, bytes);
	
	if (w >= 0) {
		*datalen = w;
		return ZAP_SUCCESS;
	}

	return ZAP_FAIL;
}

/**
 * \brief Destroys a zaptel Channel
 * \param zchan Channel to destroy
 * \return Success
 */
static ZIO_CHANNEL_DESTROY_FUNCTION(zt_channel_destroy)
{
	close(zchan->sockfd);
	zchan->sockfd = ZT_INVALID_SOCKET;

	return ZAP_SUCCESS;
}

/**
 * \brief Global Openzap IO interface for zaptel
 */
static zap_io_interface_t zt_interface;

/**
 * \brief Loads zaptel IO module
 * \param zio Openzap IO interface
 * \return Success or failure
 */
static ZIO_IO_LOAD_FUNCTION(zt_init)
{
	assert(zio != NULL);
    struct stat statbuf;
	memset(&zt_interface, 0, sizeof(zt_interface));
	memset(&zt_globals, 0, sizeof(zt_globals));

    if (!stat(zt_ctlpath, &statbuf)) {
        zap_log(ZAP_LOG_NOTICE, "Using Zaptel control device\n");
        ctlpath = zt_ctlpath;
        chanpath = zt_chanpath;
        memcpy(&codes, &zt_ioctl_codes, sizeof(codes));
    } else if (!stat(dahdi_ctlpath, &statbuf)) {
        zap_log(ZAP_LOG_NOTICE, "Using DAHDI control device\n");
        ctlpath = dahdi_ctlpath;
        chanpath = dahdi_chanpath;
        memcpy(&codes, &dahdi_ioctl_codes, sizeof(codes));
    } else {
		zap_log(ZAP_LOG_ERROR, "No DAHDI or Zap control device found in /dev/\n");
		return ZAP_FAIL;
    }
	if ((CONTROL_FD = open(ctlpath, O_RDWR)) < 0) {
		zap_log(ZAP_LOG_ERROR, "Cannot open control device %s: %s\n", ctlpath, strerror(errno));
		return ZAP_FAIL;
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
	zt_interface.channel_destroy = zt_channel_destroy;
	zt_interface.get_alarms = zt_get_alarms;
	*zio = &zt_interface;

	return ZAP_SUCCESS;
}

/**
 * \brief Unloads zaptel IO module
 * \return Success
 */
static ZIO_IO_UNLOAD_FUNCTION(zt_destroy)
{
	close(CONTROL_FD);
	memset(&zt_interface, 0, sizeof(zt_interface));
	return ZAP_SUCCESS;
}

/**
 * \brief Openzap zaptel IO module definition
 */
zap_module_t zap_module = { 
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
