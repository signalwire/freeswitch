/*
 *  zap_m3ua.c
 *  openzap
 *
 *  Created by Shane Burrell on 4/3/08.
 *  Copyright 2008 Shane Burrell. All rights reserved.
 *

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
//#include "m3ua_client.h"
#include "zap_m3ua.h"

struct general_config {
	uint32_t region;
};
typedef struct general_config general_config_t;


struct m3ua_channel_profile {
	char name[80];
	int cust_span;
};
typedef struct m3ua_channel_profile m3ua_channel_profile_t;

static struct {
	zap_hash_t *profile_hash;
	general_config_t general_config;
} globals;

struct m3ua_span_data {
	uint32_t boardno;
	uint32_t flags;
};
typedef struct m3ua_span_data m3ua_span_data_t;

struct m3ua_chan_data {
	zap_buffer_t *digit_buffer;
	zap_mutex_t *digit_mutex;
	zap_size_t dtmf_len;
	uint32_t flags;
	uint32_t hdlc_bytes;
};
typedef struct m3ua_chan_data m3ua_chan_data_t;






static ZIO_CONFIGURE_FUNCTION(m3ua_configure)
{
	m3ua_channel_profile_t *profile = NULL;
	int ok = 1;

	if (!(profile = (m3ua_channel_profile_t *) hashtable_search(globals.profile_hash, (char *)category))) {
		profile = malloc(sizeof(*profile));
		memset(profile, 0, sizeof(*profile));
		zap_set_string(profile->name, category);
		//profile->play_config = globals.play_config;
		hashtable_insert(globals.profile_hash, (void *)profile->name, profile);
		zap_log(ZAP_LOG_INFO, "creating profile [%s]\n", category);
	}

	
	if (!strcasecmp(var, "local_ip")) {
		//profile->span_config.framing = pika_str2span(val);
		profile->cust_span++;
	} 
	ok = 0;
	

	if (ok) {
		zap_log(ZAP_LOG_INFO, "setting param [%s]=[%s] for profile [%s]\n", var, val, category);
	} else {
		zap_log(ZAP_LOG_ERROR, "unknown param [%s]\n", var);
	}

	return ZAP_SUCCESS;
}

static ZIO_CONFIGURE_SPAN_FUNCTION(m3ua_configure_span)
{

	return ZAP_FAIL;
}

static ZIO_OPEN_FUNCTION(m3ua_open) 
{
	
	return ZAP_FAIL;
}

static ZIO_CLOSE_FUNCTION(m3ua_close)
{
	
	return ZAP_FAIL;
}

/*static ZIO_SET_INTERVAL_FUNCTION(m3ua_set_interval)
{
	
	return 0;
}*/

static ZIO_WAIT_FUNCTION(m3ua_wait)
{
	
	return ZAP_FAIL;
}

static ZIO_READ_FUNCTION(m3ua_read)
{
	
	return ZAP_FAIL;
}

static ZIO_WRITE_FUNCTION(m3ua_write)
{
	
	return ZAP_FAIL;
}

static ZIO_COMMAND_FUNCTION(m3ua_command)
{
	return ZAP_FAIL;
}

static ZIO_SPAN_POLL_EVENT_FUNCTION(m3ua_poll_event)
{
	return ZAP_FAIL;
}

static ZIO_SPAN_NEXT_EVENT_FUNCTION(m3ua_next_event)
{
	return ZAP_FAIL;
}


static ZIO_SPAN_DESTROY_FUNCTION(m3ua_span_destroy)
{
	m3ua_span_data_t *span_data = (m3ua_span_data_t *) span->mod_data;
	
	if (span_data) {
		free(span_data);
	}
	
	return ZAP_SUCCESS;
}
static ZIO_CHANNEL_DESTROY_FUNCTION(m3ua_channel_destroy)
{
	m3ua_chan_data_t *chan_data = (m3ua_chan_data_t *) zchan->mod_data;
	m3ua_span_data_t *span_data = (m3ua_span_data_t *) zchan->span->mod_data;
	
	if (!chan_data) {
		return ZAP_FAIL;
	}

	
		



	zap_mutex_destroy(&chan_data->digit_mutex);
	zap_buffer_destroy(&chan_data->digit_buffer);


	zap_safe_free(chan_data);
	
	if (span_data) {
		free(span_data);
	}
	
			
	return ZAP_SUCCESS;
}



static ZIO_GET_ALARMS_FUNCTION(m3ua_get_alarms)
{
	return ZAP_FAIL;
}

static zap_io_interface_t m3ua_interface;

zap_status_t m3ua_init(zap_io_interface_t **zint)
{
	assert(zint != NULL);
	memset(&m3ua_interface, 0, sizeof(m3ua_interface));

	m3ua_interface.name = "m3ua";
	m3ua_interface.configure =  m3ua_configure;
	m3ua_interface.configure_span =  m3ua_configure_span;
	m3ua_interface.open = m3ua_open;
	m3ua_interface.close = m3ua_close;
	m3ua_interface.wait = m3ua_wait;
	m3ua_interface.read = m3ua_read;
	m3ua_interface.write = m3ua_write;
	m3ua_interface.command = m3ua_command;
	m3ua_interface.poll_event = m3ua_poll_event;
	m3ua_interface.next_event = m3ua_next_event;
	m3ua_interface.channel_destroy = m3ua_channel_destroy;
	m3ua_interface.span_destroy = m3ua_span_destroy;
	m3ua_interface.get_alarms = m3ua_get_alarms;
	*zint = &m3ua_interface;

	return ZAP_FAIL;
}

zap_status_t m3ua_destroy(void)
{
	return ZAP_FAIL;
}

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
*/