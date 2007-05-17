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

#ifndef OPENZAP_H
#define OPENZAP_H

#define _XOPEN_SOURCE 500

#ifdef _MSC_VER
#if (_MSC_VER >= 1400)			// VC8+
#ifndef _CRT_SECURE_NO_DEPRECATE
#define _CRT_SECURE_NO_DEPRECATE
#endif
#ifndef _CRT_NONSTDC_NO_DEPRECATE
#define _CRT_NONSTDC_NO_DEPRECATE
#endif
#endif
#ifndef strcasecmp
#define strcasecmp(s1, s2) _stricmp(s1, s2)
#endif
#ifndef snprintf
#define snprintf _snprintf
#endif
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _MSC_VER
#include <strings.h>
#endif
#include <assert.h>
#include "hashtable.h"
#include "zap_config.h"

#define ZAP_MAX_CHANNELS_SPAN 513
#define ZAP_MAX_SPANS_INTERFACE 33

typedef int zap_socket_t;
typedef size_t zap_size_t;
struct zap_software_interface;

#define zap_copy_string(x,y,z) strncpy(x, y, z - 1) 

/*!
  \brief Test for the existance of a flag on an arbitary object
  \param obj the object to test
  \param flag the or'd list of flags to test
  \return true value if the object has the flags defined
*/
#define zap_test_flag(obj, flag) ((obj)->flags & flag)

/*!
  \brief Set a flag on an arbitrary object
  \param obj the object to set the flags on
  \param flag the or'd list of flags to set
*/
#define zap_set_flag(obj, flag) (obj)->flags |= (flag)

/*!
  \brief Clear a flag on an arbitrary object while locked
  \param obj the object to test
  \param flag the or'd list of flags to clear
*/
#define zap_clear_flag(obj, flag) (obj)->flags &= ~(flag)

/*!
  \brief Copy flags from one arbitrary object to another
  \param dest the object to copy the flags to
  \param src the object to copy the flags from
  \param flags the flags to copy
*/
#define zap_copy_flags(dest, src, flags) (dest)->flags &= ~(flags);	(dest)->flags |= ((src)->flags & (flags))

/*!
  \brief Free a pointer and set it to NULL unless it already is NULL
  \param it the pointer
*/
#define zap_safe_free(it) if (it) {free(it);it=NULL;}

typedef enum {
	ZAP_SUCCESS,
	ZAP_FAIL
} zap_status_t;

typedef enum {
	ZAP_READ =  (1 <<  0),
	ZAP_WRITE = (1 <<  1),
	ZAP_ERROR = (1 <<  2),
	ZAP_EVENT = (1 <<  3)
} zap_wait_flag_t;

typedef enum {
	ZAP_CODEC_ULAW = 0,
	ZAP_CODEC_ALAW = 8,
	ZAP_CODEC_SLIN = 10
} zap_codec_t;

typedef enum {
	ZAP_SPAN_CONFIGURED = (1 << 0),
	ZAP_SPAN_READY = (1 << 1)
} zap_span_flag_t;

typedef enum {
	ZAP_CHAN_TYPE_B,
	ZAP_CHAN_TYPE_DQ921,
	ZAP_CHAN_TYPE_DQ931,
	ZAP_CHAN_TYPE_FXS,
	ZAP_CHAN_TYPE_FXO
} zap_chan_type_t;

typedef enum {
	ZAP_CHANNEL_CONFIGURED = (1 << 0),
	ZAP_CHANNEL_READY = (1 << 1),
	ZAP_CHANNEL_OPEN = (1 << 2)
} zap_channel_flag_t;

struct zap_channel {
	unsigned span_id;
	unsigned chan_id;
	zap_chan_type_t type;
	zap_socket_t sockfd;
	zap_channel_flag_t flags;
	struct zap_software_interface *zint;
};
typedef struct zap_channel zap_channel_t;

struct zap_span {
	unsigned span_id;
	unsigned chan_count;
	zap_span_flag_t flags;
	struct zap_software_interface *zint;
	zap_channel_t channels[ZAP_MAX_CHANNELS_SPAN];
};
typedef struct zap_span zap_span_t;

#define ZINT_CONFIGURE_ARGS (struct zap_software_interface *zint)
#define ZINT_OPEN_ARGS (unsigned span_id, unsigned chan_id, zap_channel_t **zchan)
#define ZINT_CLOSE_ARGS (zap_channel_t **zchan)
#define ZINT_SET_CODEC_ARGS (zap_channel_t *zchan, zap_codec_t codec)
#define ZINT_SET_INTERVAL_ARGS (zap_channel_t *zchan, unsigned ms)
#define ZINT_WAIT_ARGS (zap_channel_t *zchan, zap_wait_flag_t flags, unsigned to)
#define ZINT_READ_ARGS (zap_channel_t *zchan, void *data, zap_size_t *datalen)
#define ZINT_WRITE_ARGS (zap_channel_t *zchan, void *data, zap_size_t *datalen)

typedef zap_status_t (*zint_configure_t) ZINT_CONFIGURE_ARGS ;
typedef zap_status_t (*zint_open_t) ZINT_OPEN_ARGS ;
typedef zap_status_t (*zint_close_t) ZINT_CLOSE_ARGS ;
typedef zap_status_t (*zint_set_codec_t) ZINT_SET_CODEC_ARGS ;
typedef zap_status_t (*zint_set_interval_t) ZINT_SET_INTERVAL_ARGS ;
typedef zap_status_t (*zint_wait_t) ZINT_WAIT_ARGS ;
typedef zap_status_t (*zint_read_t) ZINT_READ_ARGS ;
typedef zap_status_t (*zint_write_t) ZINT_WRITE_ARGS ;

#define ZINT_CONFIGURE_FUNCTION(name) zap_status_t name ZINT_CONFIGURE_ARGS
#define ZINT_OPEN_FUNCTION(name) zap_status_t name ZINT_OPEN_ARGS
#define ZINT_CLOSE_FUNCTION(name) zap_status_t name ZINT_CLOSE_ARGS
#define ZINT_SET_CODEC_FUNCTION(name) zap_status_t name ZINT_SET_CODEC_ARGS
#define ZINT_SET_INTERVAL_FUNCTION(name) zap_status_t name ZINT_SET_INTERVAL_ARGS
#define ZINT_WAIT_FUNCTION(name) zap_status_t name ZINT_WAIT_ARGS
#define ZINT_READ_FUNCTION(name) zap_status_t name ZINT_READ_ARGS
#define ZINT_WRITE_FUNCTION(name) zap_status_t name ZINT_WRITE_ARGS

#define ZINT_CONFIGURE_MUZZLE assert(zint != NULL)
#define ZINT_OPEN_MUZZLE assert(span_id != 0); assert(chan_id != 0); assert(zchan != NULL)
#define ZINT_CLOSE_MUZZLE assert(zchan != NULL)
#define ZINT_SET_CODEC_MUZZLE assert(zchan != NULL); assert(codec != 0)
#define ZINT_SET_INTERVAL_MUZZLE assert(zchan != NULL); assert(ms != 0)
#define ZINT_WAIT_MUZZLE assert(zchan != NULL); assert(flags != 0); assert(to != 0)
#define ZINT_READ_MUZZLE assert(zchan != NULL); assert(data != NULL); assert(datalen != NULL)
#define ZINT_WRITE_MUZZLE assert(zchan != NULL); assert(data != NULL); assert(datalen != NULL)

struct zap_software_interface {
	const char *name;
	zint_configure_t configure;
	zint_open_t open;
	zint_close_t close;
	zint_set_codec_t set_codec;
	zint_set_interval_t set_interval;
	zint_wait_t wait;
	zint_read_t read;
	zint_write_t write;
	unsigned span_index;
	struct zap_span spans[ZAP_MAX_SPANS_INTERFACE];
};
typedef struct zap_software_interface zap_software_interface_t;

zap_status_t zap_span_create(zap_software_interface_t *zint, zap_span_t **span);
zap_status_t zap_span_add_channel(zap_span_t *span, zap_socket_t sockfd, zap_chan_type_t type, zap_channel_t **chan);
zap_status_t zap_span_destroy(zap_span_t **span);

zap_status_t zap_channel_open(const char *name, unsigned span_id, unsigned chan_id, zap_channel_t **zchan);
zap_status_t zap_channel_close(zap_channel_t **zchan);
zap_status_t zap_channel_set_codec(zap_channel_t *zchan, zap_codec_t codec);
zap_status_t zap_channel_set_interval(zap_channel_t *zchan, unsigned ms);
zap_status_t zap_channel_wait(zap_channel_t *zchan, zap_wait_flag_t flags, unsigned to);
zap_status_t zap_channel_read(zap_channel_t *zchan, void *data, zap_size_t *datalen);
zap_status_t zap_channel_write(zap_channel_t *zchan, void *data, zap_size_t *datalen);
zap_status_t zap_global_init(void);
zap_status_t zap_global_destroy(void);

typedef struct hashtable zap_hash_t;

#endif
