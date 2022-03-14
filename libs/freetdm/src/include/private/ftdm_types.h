/*
 * Copyright (c) 2007-2014, Anthony Minessale II
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
 *
 */

#ifndef FTDM_TYPES_H
#define FTDM_TYPES_H

#include "freetdm.h"

#include "fsk.h"

#ifdef WIN32
typedef intptr_t ftdm_ssize_t;
typedef int ftdm_filehandle_t;
#else
#include <sys/types.h>
#include <sys/ioctl.h>
#include <stdarg.h>
typedef ssize_t ftdm_ssize_t;
typedef int ftdm_filehandle_t;
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define FTDM_COMMAND_OBJ_SIZE *((ftdm_size_t *)obj)
#define FTDM_COMMAND_OBJ_INT *((int *)obj)
#define FTDM_COMMAND_OBJ_CHAR_P (char *)obj
#define FTDM_COMMAND_OBJ_FLOAT *(float *)obj
#define FTDM_FSK_MOD_FACTOR 0x10000
#define FTDM_DEFAULT_DTMF_ON 250
#define FTDM_DEFAULT_DTMF_OFF 50

typedef enum {
	FTDM_ENDIAN_BIG = 1,
	FTDM_ENDIAN_LITTLE = -1
} ftdm_endian_t;

typedef enum {
	FTDM_CID_TYPE_SDMF = 0x04,
	FTDM_CID_TYPE_MDMF = 0x80
} ftdm_cid_type_t;

typedef enum {
	MDMF_DATETIME = 1,
	MDMF_PHONE_NUM = 2,
	MDMF_DDN = 3,
	MDMF_NO_NUM = 4,
	MDMF_PHONE_NAME = 7,
	MDMF_NO_NAME = 8,
	MDMF_ALT_ROUTE = 9,
	MDMF_INVALID = 10
} ftdm_mdmf_type_t;
#define MDMF_STRINGS "X", "DATETIME", "PHONE_NUM", "DDN", "NO_NUM", "X", "X", "PHONE_NAME", "NO_NAME", "ALT_ROUTE", "INVALID"
FTDM_STR2ENUM_P(ftdm_str2ftdm_mdmf_type, ftdm_mdmf_type2str, ftdm_mdmf_type_t)

#define FTDM_TONEMAP_LEN 128
typedef enum {
	FTDM_TONEMAP_NONE,
	FTDM_TONEMAP_DIAL,
	FTDM_TONEMAP_RING,
	FTDM_TONEMAP_BUSY,
	FTDM_TONEMAP_FAIL1,
	FTDM_TONEMAP_FAIL2,
	FTDM_TONEMAP_FAIL3,
	FTDM_TONEMAP_ATTN,
	FTDM_TONEMAP_CALLWAITING_CAS,
	FTDM_TONEMAP_CALLWAITING_SAS,
	FTDM_TONEMAP_CALLWAITING_ACK,
	FTDM_TONEMAP_INVALID
} ftdm_tonemap_t;
#define TONEMAP_STRINGS "NONE", "DIAL", "RING", "BUSY", "FAIL1", "FAIL2", "FAIL3", "ATTN", "CALLWAITING-CAS", "CALLWAITING-SAS", "CALLWAITING-ACK", "INVALID"
FTDM_STR2ENUM_P(ftdm_str2ftdm_tonemap, ftdm_tonemap2str, ftdm_tonemap_t)

typedef enum {
	FTDM_ANALOG_START_KEWL,
	FTDM_ANALOG_START_LOOP,
	FTDM_ANALOG_START_GROUND,
	FTDM_ANALOG_START_WINK,
	FTDM_ANALOG_START_NA
} ftdm_analog_start_type_t;
#define START_TYPE_STRINGS "KEWL", "LOOP", "GROUND", "WINK", "NA"
FTDM_STR2ENUM_P(ftdm_str2ftdm_analog_start_type, ftdm_analog_start_type2str, ftdm_analog_start_type_t)

typedef enum {
	FTDM_OOB_NOOP,
	FTDM_OOB_ONHOOK,
	FTDM_OOB_OFFHOOK,
	FTDM_OOB_WINK,
	FTDM_OOB_FLASH,
	FTDM_OOB_RING_START,
	FTDM_OOB_RING_STOP,
	FTDM_OOB_ALARM_TRAP,
	FTDM_OOB_ALARM_CLEAR,
	FTDM_OOB_CAS_BITS_CHANGE,
	FTDM_OOB_POLARITY_REVERSE,
	FTDM_OOB_INVALID
} ftdm_oob_event_t;
#define OOB_STRINGS "NOOP", "ONHOOK", "OFFHOOK", "WINK", "FLASH", "RING_START", "RING_STOP", "ALARM_TRAP", "ALARM_CLEAR", "CAS_BITS_CHANGE", "POLARITY_REVERSE", "INVALID"
FTDM_STR2ENUM_P(ftdm_str2ftdm_oob_event, ftdm_oob_event2str, ftdm_oob_event_t)

/*! \brief Event types */
typedef enum {
	FTDM_EVENT_NONE,
	/* DTMF digit was just detected */
	FTDM_EVENT_DTMF,
	/* Out of band event */
	FTDM_EVENT_OOB,
	FTDM_EVENT_COUNT
} ftdm_event_type_t;

/*! \brief Generic event data type */
struct ftdm_event {
	ftdm_event_type_t e_type;
	uint32_t enum_id;
	ftdm_channel_t *channel;
	void *data;
};

typedef enum {
	FTDM_SIGTYPE_NONE,
	FTDM_SIGTYPE_ISDN,
	FTDM_SIGTYPE_RBS,
	FTDM_SIGTYPE_ANALOG,
	FTDM_SIGTYPE_SANGOMABOOST,
	FTDM_SIGTYPE_M3UA,
	FTDM_SIGTYPE_M2UA,
	FTDM_SIGTYPE_R2,
	FTDM_SIGTYPE_SS7,
	FTDM_SIGTYPE_GSM
} ftdm_signal_type_t;

typedef enum {
	FTDM_SPAN_CONFIGURED = (1 << 0),
	FTDM_SPAN_STARTED = (1 << 1),
	FTDM_SPAN_STATE_CHANGE = (1 << 2),
	FTDM_SPAN_SUSPENDED = (1 << 3),
	FTDM_SPAN_IN_THREAD = (1 << 4),
	FTDM_SPAN_STOP_THREAD = (1 << 5),
	/*! Signaling modules set this flag to use fchan->pendingchans queue instead
	 *  of the FTDM_SPAN_STATE_CHANGE flag to detect when there is channels with
	 *  a state change pending in the span. If you set this member you can't rely
	 *  on FTDM_SPAN_STATE_CHANGE anymore and must use the queue only instead. This
	 *  is the new way of detecting state changes, new modules should always set this
	 *  flag, the old modules still relying on FTDM_SPAN_STATE_CHANGE should be updated */
	FTDM_SPAN_USE_CHAN_QUEUE = (1 << 6),
	FTDM_SPAN_SUGGEST_CHAN_ID = (1 << 7),
	FTDM_SPAN_USE_AV_RATE = (1 << 8),
	FTDM_SPAN_PWR_SAVING = (1 << 9),
	/* If you use this flag, you MUST call ftdm_span_trigger_signals to deliver the user signals
	 * after having called ftdm_send_span_signal(), which with this flag it will just enqueue the signal
	 * for later delivery */
	FTDM_SPAN_USE_SIGNALS_QUEUE = (1 << 10),
	/* If this flag is set, channel will be moved to proceed state when calls goes to routing */
	FTDM_SPAN_USE_PROCEED_STATE = (1 << 11),
	/* If this flag is set, the signalling module supports jumping directly to state up, without
		going through PROGRESS/PROGRESS_MEDIA */
	FTDM_SPAN_USE_SKIP_STATES = (1 << 12),
	/* If this flag is set, then this span cannot be stopped individually, it can only be stopped
		on freetdm unload */
	FTDM_SPAN_NON_STOPPABLE = (1 << 13),
	/* If this flag is set, then this span supports TRANSFER state */
	FTDM_SPAN_USE_TRANSFER = (1 << 14),
	/* This is the last flag, no more flags bigger than this */
	FTDM_SPAN_MAX_FLAG = (1 << 15),
} ftdm_span_flag_t;

/*! \brief Channel supported features */
typedef enum {
	FTDM_CHANNEL_FEATURE_DTMF_DETECT = (1 << 0), /*!< Channel can detect DTMF (read-only) */
	FTDM_CHANNEL_FEATURE_DTMF_GENERATE = (1 << 1), /*!< Channel can generate DTMF (read-only) */
	FTDM_CHANNEL_FEATURE_CODECS = (1 << 2), /*!< Channel can do transcoding (read-only) */
	FTDM_CHANNEL_FEATURE_INTERVAL = (1 << 3), /*!< Channel support i/o interval configuration (read-only) */
	FTDM_CHANNEL_FEATURE_CALLERID = (1 << 4), /*!< Channel can detect caller id (read-only) */
	FTDM_CHANNEL_FEATURE_PROGRESS = (1 << 5), /*!< Channel can detect inband progress (read-only) */
	FTDM_CHANNEL_FEATURE_CALLWAITING = (1 << 6), /*!< Channel will allow call waiting (ie: FXS devices) (read/write) */
	FTDM_CHANNEL_FEATURE_HWEC = (1<<7), /*!< Channel has a hardware echo canceller */
	FTDM_CHANNEL_FEATURE_HWEC_DISABLED_ON_IDLE  = (1<<8), /*!< hardware echo canceller is disabled when there are no calls on this channel */
	FTDM_CHANNEL_FEATURE_IO_STATS = (1<<9), /*!< Channel supports IO statistics (HDLC channels only) */
	FTDM_CHANNEL_FEATURE_MF_GENERATE = (1<<10), /*!< Channel can generate R2 MF tones (read-only) */
} ftdm_channel_feature_t;

/*! \brief Channel IO pending flags */
typedef enum {
	FTDM_CHANNEL_IO_EVENT = (1 << 0),
	FTDM_CHANNEL_IO_READ = (1 << 1),
	FTDM_CHANNEL_IO_WRITE = (1 << 2),
} ftdm_channel_io_flags_t;

/*!< Channel flags. This used to be an enum but we reached the 32bit limit for enums, is safer this way */
#define FTDM_CHANNEL_CONFIGURED    (1ULL << 0)
#define FTDM_CHANNEL_READY         (1ULL << 1)
#define FTDM_CHANNEL_OPEN          (1ULL << 2)
#define FTDM_CHANNEL_DTMF_DETECT   (1ULL << 3)
#define FTDM_CHANNEL_SUPRESS_DTMF  (1ULL << 4)
#define FTDM_CHANNEL_TRANSCODE     (1ULL << 5)
#define FTDM_CHANNEL_BUFFER        (1ULL << 6)
#define FTDM_CHANNEL_INTHREAD      (1ULL << 8)
#define FTDM_CHANNEL_WINK          (1ULL << 9)
#define FTDM_CHANNEL_FLASH         (1ULL << 10)
#define FTDM_CHANNEL_STATE_CHANGE  (1ULL << 11)
#define FTDM_CHANNEL_HOLD          (1ULL << 12)
#define FTDM_CHANNEL_INUSE         (1ULL << 13)
#define FTDM_CHANNEL_OFFHOOK       (1ULL << 14)
#define FTDM_CHANNEL_RINGING       (1ULL << 15)
#define FTDM_CHANNEL_PROGRESS_DETECT (1ULL << 16)
#define FTDM_CHANNEL_CALLERID_DETECT (1ULL << 17)
#define FTDM_CHANNEL_OUTBOUND        (1ULL << 18)
#define FTDM_CHANNEL_SUSPENDED       (1ULL << 19)
#define FTDM_CHANNEL_3WAY            (1ULL << 20)
#define FTDM_CHANNEL_PROGRESS        (1ULL << 21)
/*!< There is media on the channel already */
#define FTDM_CHANNEL_MEDIA           (1ULL << 22)
/*!< The channel was answered */
#define FTDM_CHANNEL_ANSWERED        (1ULL << 23)
#define FTDM_CHANNEL_MUTE            (1ULL << 24)
#define FTDM_CHANNEL_USE_RX_GAIN     (1ULL << 25)
#define FTDM_CHANNEL_USE_TX_GAIN     (1ULL << 26)
#define FTDM_CHANNEL_IN_ALARM        (1ULL << 27)
#define FTDM_CHANNEL_SIG_UP          (1ULL << 28)
#define FTDM_CHANNEL_USER_HANGUP     (1ULL << 29)
#define FTDM_CHANNEL_RX_DISABLED     (1ULL << 30)
#define FTDM_CHANNEL_TX_DISABLED     (1ULL << 31)
/*!< The user knows about a call in this channel */
#define FTDM_CHANNEL_CALL_STARTED    (1ULL << 32)
/*!< The user wants non-blocking operations in the channel */
#define FTDM_CHANNEL_NONBLOCK        (1ULL << 33)
/*!< There is a pending acknowledge for an indication */
#define FTDM_CHANNEL_IND_ACK_PENDING (1ULL << 34)
/*!< There is someone blocking in the channel waiting for state completion */
#define FTDM_CHANNEL_BLOCKING        (1ULL << 35)
/*!< Media is digital */
#define FTDM_CHANNEL_DIGITAL_MEDIA   (1ULL << 36)
/*!< Native signaling bridge is enabled */
#define FTDM_CHANNEL_NATIVE_SIGBRIDGE (1ULL << 37)
/*!< Native signaling DTMF detection */
#define FTDM_CHANNEL_SIG_DTMF_DETECTION (1ULL << 38)

/*!< This no more flags after this flag */
#define FTDM_CHANNEL_MAX_FLAG 	     (1ULL << 39)
/*!<When adding a new flag, need to update ftdm_io.c:channel_flag_strs */

#include "ftdm_state.h"

typedef enum ftdm_channel_hw_link_status {
	FTDM_HW_LINK_DISCONNECTED = 0,
	FTDM_HW_LINK_CONNECTED
} ftdm_channel_hw_link_status_t;

typedef ftdm_status_t (*ftdm_stream_handle_raw_write_function_t) (ftdm_stream_handle_t *handle, uint8_t *data, ftdm_size_t datalen);
typedef ftdm_status_t (*ftdm_stream_handle_write_function_t) (ftdm_stream_handle_t *handle, const char *fmt, ...);

#include "ftdm_dso.h"

#define FTDM_NODE_NAME_SIZE 50
struct ftdm_conf_node {
	/* node name */
	char name[FTDM_NODE_NAME_SIZE];

	/* total slots for parameters */
	unsigned int t_parameters;

	/* current number of parameters */
	unsigned int n_parameters;

	/* array of parameters */
	ftdm_conf_parameter_t *parameters;

	/* first node child */
	struct ftdm_conf_node *child;

	/* last node child */
	struct ftdm_conf_node *last;

	/* next node sibling */
	struct ftdm_conf_node *next;

	/* prev node sibling */
	struct ftdm_conf_node *prev;

	/* my parent if any */
	struct ftdm_conf_node *parent;
};

typedef struct ftdm_module {
	char name[256];
	fio_io_load_t io_load;
	fio_io_unload_t io_unload;
	fio_sig_load_t sig_load;
	fio_sig_configure_t sig_configure;
	fio_sig_unload_t sig_unload;
	/*! 
	  \brief configure a given span signaling 
	  \see sig_configure
	  This is just like sig_configure but receives
	  an an ftdm_conf_node_t instead
	  I'd like to deprecate sig_configure and move
	  all modules to use configure_span_signaling
	 */
	fio_configure_span_signaling_t configure_span_signaling;
	ftdm_dso_lib_t lib;
	char path[256];
} ftdm_module_t;

typedef struct ftdm_fsk_data_state ftdm_fsk_data_state_t;
typedef int (*ftdm_fsk_data_decoder_t)(ftdm_fsk_data_state_t *state);
typedef ftdm_status_t (*ftdm_fsk_write_sample_t)(int16_t *buf, ftdm_size_t buflen, void *user_data);
typedef struct hashtable ftdm_hash_t;
typedef struct hashtable_iterator ftdm_hash_iterator_t;
typedef struct key ftdm_hash_key_t;
typedef struct value ftdm_hash_val_t;
typedef struct ftdm_bitstream ftdm_bitstream_t;
typedef struct ftdm_fsk_modulator ftdm_fsk_modulator_t;
typedef ftdm_status_t (*ftdm_span_start_t)(ftdm_span_t *span);
typedef ftdm_status_t (*ftdm_span_stop_t)(ftdm_span_t *span);
typedef ftdm_status_t (*ftdm_span_destroy_t)(ftdm_span_t *span);
typedef ftdm_status_t (*ftdm_channel_sig_read_t)(ftdm_channel_t *ftdmchan, void *data, ftdm_size_t size);
typedef ftdm_status_t (*ftdm_channel_sig_write_t)(ftdm_channel_t *ftdmchan, void *data, ftdm_size_t size);
typedef ftdm_status_t (*ftdm_channel_sig_dtmf_t)(ftdm_channel_t *ftdmchan, const char *dtmf);

typedef enum {
	FTDM_ITERATOR_VARS = 1,
	FTDM_ITERATOR_CHANS,
	FTDM_ITERATOR_SPANS,
} ftdm_iterator_type_t;

struct ftdm_iterator {
	ftdm_iterator_type_t type;
	unsigned int allocated:1;
	union {
		struct {
			uint32_t index;
			const ftdm_span_t *span;
		} chaniter;
		ftdm_hash_iterator_t *hashiter;
	} pvt;
};

#ifdef __cplusplus
}
#endif

#endif

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

