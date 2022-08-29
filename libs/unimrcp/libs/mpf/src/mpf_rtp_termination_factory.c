/*
 * Copyright 2008-2015 Arsen Chaloyan
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <apr_tables.h>
#include "mpf_termination.h"
#include "mpf_rtp_termination_factory.h"
#include "mpf_rtp_stream.h"
#include "apt_log.h"

typedef struct media_engine_slot_t media_engine_slot_t;
typedef struct rtp_termination_factory_t rtp_termination_factory_t;

struct media_engine_slot_t {
	mpf_engine_t     *media_engine;
	mpf_rtp_config_t *rtp_config;
};

struct rtp_termination_factory_t {
	mpf_termination_factory_t base;

	mpf_rtp_config_t         *config;
	apr_array_header_t       *media_engine_slots;
	apr_pool_t               *pool;
};

static apt_bool_t mpf_rtp_termination_destroy(mpf_termination_t *termination)
{
	return TRUE;
}

static apt_bool_t mpf_rtp_termination_add(mpf_termination_t *termination, void *descriptor)
{
	apt_bool_t status = TRUE;
	mpf_rtp_termination_descriptor_t *rtp_descriptor = descriptor;
	mpf_audio_stream_t *audio_stream = termination->audio_stream;
	if(!audio_stream) {
		int i;
		media_engine_slot_t *slot;
		rtp_termination_factory_t *rtp_termination_factory = (rtp_termination_factory_t*)termination->termination_factory;
		mpf_rtp_config_t *rtp_config = rtp_termination_factory->config;
		for(i=0; i<rtp_termination_factory->media_engine_slots->nelts; i++) {
			slot = &APR_ARRAY_IDX(rtp_termination_factory->media_engine_slots,i,media_engine_slot_t);
			if(slot->media_engine == termination->media_engine) {
				rtp_config = slot->rtp_config;
				break;
			}
		}
		audio_stream = mpf_rtp_stream_create(
							termination,
							rtp_config,
							rtp_descriptor->audio.settings,
							termination->pool);
		if(!audio_stream) {
			return FALSE;
		}
		termination->audio_stream = audio_stream;
	}

	status = mpf_rtp_stream_add(audio_stream);
	if(rtp_descriptor) {
		status = mpf_rtp_stream_modify(audio_stream,&rtp_descriptor->audio);
	}
	return status;
}

static apt_bool_t mpf_rtp_termination_modify(mpf_termination_t *termination, void *descriptor)
{
	apt_bool_t status = TRUE;
	mpf_rtp_termination_descriptor_t *rtp_descriptor = descriptor;
	mpf_audio_stream_t *audio_stream = termination->audio_stream;
	if(!audio_stream) {
		return FALSE;
	}

	if(rtp_descriptor) {
		status = mpf_rtp_stream_modify(audio_stream,&rtp_descriptor->audio);
	}
	return status;
}

static apt_bool_t mpf_rtp_termination_subtract(mpf_termination_t *termination)
{
	mpf_audio_stream_t *audio_stream = termination->audio_stream;
	if(!audio_stream) {
		return FALSE;
	}
	
	return mpf_rtp_stream_remove(audio_stream);
}

static const mpf_termination_vtable_t rtp_vtable = {
	mpf_rtp_termination_destroy,
	mpf_rtp_termination_add,
	mpf_rtp_termination_modify,
	mpf_rtp_termination_subtract
};

static mpf_termination_t* mpf_rtp_termination_create(mpf_termination_factory_t *termination_factory, void *obj, apr_pool_t *pool)
{
	mpf_termination_t *termination = mpf_termination_base_create(termination_factory,obj,&rtp_vtable,NULL,NULL,pool);
	if(termination) {
		termination->name = "rtp-tm";
	}
	return termination;
}

static apt_bool_t mpf_rtp_factory_engine_assign(mpf_termination_factory_t *termination_factory, mpf_engine_t *media_engine)
{
	int i;
	media_engine_slot_t *slot;
	mpf_rtp_config_t *rtp_config;
	rtp_termination_factory_t *rtp_termination_factory;
	if(!termination_factory || !media_engine) {
		return FALSE;
	}
	
	rtp_termination_factory = (rtp_termination_factory_t *) termination_factory;
	for(i=0; i<rtp_termination_factory->media_engine_slots->nelts; i++) {
		slot = &APR_ARRAY_IDX(rtp_termination_factory->media_engine_slots,i,media_engine_slot_t);
		if(slot->media_engine == media_engine) {
			/* already exists, just return true */
			return TRUE;
		}
	}

	slot = apr_array_push(rtp_termination_factory->media_engine_slots);
	slot->media_engine = media_engine;
	rtp_config = mpf_rtp_config_alloc(rtp_termination_factory->pool);
	*rtp_config = *rtp_termination_factory->config;
	slot->rtp_config = rtp_config;

	if(rtp_termination_factory->media_engine_slots->nelts > 1) {
		mpf_rtp_config_t *rtp_config_prev;

		/* split RTP port range evenly among assigned media engines */
		apr_uint16_t ports_per_engine = (apr_uint16_t)((rtp_termination_factory->config->rtp_port_max - rtp_termination_factory->config->rtp_port_min) / 
														rtp_termination_factory->media_engine_slots->nelts);
		if(ports_per_engine % 2 != 0) {
			/* number of ports per engine should be even (RTP/RTCP pair)*/
			ports_per_engine--;
		}
		/* rewrite max RTP port for the first slot */
		slot = &APR_ARRAY_IDX(rtp_termination_factory->media_engine_slots,0,media_engine_slot_t);
		rtp_config_prev = slot->rtp_config;
		rtp_config_prev->rtp_port_max = rtp_config_prev->rtp_port_min + ports_per_engine;

		/* rewrite cur, min and max RTP ports for the slots between first and last, if any */
		for(i=1; i<rtp_termination_factory->media_engine_slots->nelts-1; i++) {
			slot = &APR_ARRAY_IDX(rtp_termination_factory->media_engine_slots,i,media_engine_slot_t);
			rtp_config = slot->rtp_config;
			rtp_config->rtp_port_min = rtp_config_prev->rtp_port_max;
			rtp_config->rtp_port_max = rtp_config->rtp_port_min + ports_per_engine;

			rtp_config->rtp_port_cur = rtp_config->rtp_port_min;
			
			rtp_config_prev = rtp_config;
		}

		/* rewrite cur and min but leave max RTP port for the last slot */
		slot = &APR_ARRAY_IDX(rtp_termination_factory->media_engine_slots,
				rtp_termination_factory->media_engine_slots->nelts-1,media_engine_slot_t);
		rtp_config = slot->rtp_config;
		rtp_config->rtp_port_min = rtp_config_prev->rtp_port_max;
		rtp_config->rtp_port_cur = rtp_config->rtp_port_min;
	}
	return TRUE;
}

MPF_DECLARE(mpf_termination_factory_t*) mpf_rtp_termination_factory_create(
											mpf_rtp_config_t *rtp_config,
											apr_pool_t *pool)
{
	rtp_termination_factory_t *rtp_termination_factory;
	if(!rtp_config) {
		return NULL;
	}
	rtp_config->rtp_port_cur = rtp_config->rtp_port_min;
	rtp_termination_factory = apr_palloc(pool,sizeof(rtp_termination_factory_t));
	rtp_termination_factory->base.create_termination = mpf_rtp_termination_create;
	rtp_termination_factory->base.assign_engine = mpf_rtp_factory_engine_assign;
	rtp_termination_factory->pool = pool;
	rtp_termination_factory->config = rtp_config;
	rtp_termination_factory->media_engine_slots = apr_array_make(pool,1,sizeof(media_engine_slot_t));
	apt_log(MPF_LOG_MARK,APT_PRIO_NOTICE,"Create RTP Termination Factory %s:[%hu,%hu]",
									rtp_config->ip.buf,
									rtp_config->rtp_port_min,
									rtp_config->rtp_port_max);
	return &rtp_termination_factory->base;
}
