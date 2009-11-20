/*
 * Copyright 2008 Arsen Chaloyan
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

#include "mpf_termination.h"
#include "mpf_rtp_termination_factory.h"
#include "mpf_rtp_stream.h"
#include "apt_log.h"

typedef struct rtp_termination_factory_t rtp_termination_factory_t;
struct rtp_termination_factory_t {
	mpf_termination_factory_t base;
	mpf_rtp_config_t         *config;
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
		rtp_termination_factory_t *termination_factory = (rtp_termination_factory_t*)termination->termination_factory;
		audio_stream = mpf_rtp_stream_create(termination,termination_factory->config,termination->pool);
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
	return mpf_termination_base_create(termination_factory,obj,&rtp_vtable,NULL,NULL,pool);
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
	rtp_termination_factory->config = rtp_config;
	apt_log(APT_LOG_MARK,APT_PRIO_NOTICE,"Create RTP Termination Factory %s:[%hu,%hu]",
									rtp_config->ip.buf,
									rtp_config->rtp_port_min,
									rtp_config->rtp_port_max);
	return &rtp_termination_factory->base;
}
