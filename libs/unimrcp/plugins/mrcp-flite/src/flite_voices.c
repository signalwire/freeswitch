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

#include "flite_voices.h"
#include "mrcp_synth_header.h"

typedef struct flite_voice_t flite_voice_t;

/** Declaration of flite voice */
struct flite_voice_t {
	const char *name;
	cst_voice  *self;
	cst_voice* (*register_voice)(void);
	void (*unregister_voice)(cst_voice *);
};

struct flite_voices_t {
	apr_hash_t *table;
	apr_pool_t *pool;
};


/* declarations for flite voices */
cst_voice *register_cmu_us_awb(void);
cst_voice *register_cmu_us_kal(void);
cst_voice *register_cmu_us_rms(void);
cst_voice *register_cmu_us_slt(void);
void unregister_cmu_us_awb(cst_voice * v);
void unregister_cmu_us_kal(cst_voice * v);
void unregister_cmu_us_rms(cst_voice * v);
void unregister_cmu_us_slt(cst_voice * v);


static apt_bool_t flite_voices_init(flite_voices_t *voices, apr_pool_t *pool)
{
	flite_voice_t *voice;

	voice = apr_palloc(pool,sizeof(flite_voice_t));
	voice->name = "awb";
	voice->self = NULL;
	voice->register_voice = register_cmu_us_awb;
	voice->unregister_voice = unregister_cmu_us_awb;
	apr_hash_set(voices->table,voice->name,APR_HASH_KEY_STRING,voice);

	voice = apr_palloc(pool,sizeof(flite_voice_t));
	voice->name = "kal";
	voice->self = NULL;
	voice->register_voice = register_cmu_us_kal;
	voice->unregister_voice = unregister_cmu_us_kal;
	apr_hash_set(voices->table,voice->name,APR_HASH_KEY_STRING,voice);

	voice = apr_palloc(pool,sizeof(flite_voice_t));
	voice->name = "rms";
	voice->self = NULL;
	voice->register_voice = register_cmu_us_rms;
	voice->unregister_voice = unregister_cmu_us_rms;
	apr_hash_set(voices->table,voice->name,APR_HASH_KEY_STRING,voice);

	voice = apr_palloc(pool,sizeof(flite_voice_t));
	voice->name = "slt";
	voice->self = NULL;
	voice->register_voice = register_cmu_us_slt;
	voice->unregister_voice = unregister_cmu_us_slt;
	apr_hash_set(voices->table,voice->name,APR_HASH_KEY_STRING,voice);

	return TRUE;
}


flite_voices_t* flite_voices_load(apr_pool_t *pool)
{
	flite_voice_t *voice;
	apr_hash_index_t *it;
	void *val;

	flite_voices_t *voices = apr_palloc(pool,sizeof(flite_voices_t));
	voices->pool = pool;
	voices->table = apr_hash_make(pool);

	/* init voices */
	flite_voices_init(voices,pool);

	/* register voices */
	it = apr_hash_first(pool,voices->table);
	/* walk through the voices and register them */
	for(; it; it = apr_hash_next(it)) {
		apr_hash_this(it,NULL,NULL,&val);
		voice = val;
		if(voice) {
			voice->self = voice->register_voice();
		}
	}

	return voices;
}

void flite_voices_unload(flite_voices_t *voices)
{
	flite_voice_t *voice;
	apr_hash_index_t *it;
	void *val;

	/* unregister voices */
	it = apr_hash_first(voices->pool,voices->table);
	/* walk through the voices and register them */
	for(; it; it = apr_hash_next(it)) {
		apr_hash_this(it,NULL,NULL,&val);
		voice = val;
		if(voice && voice->self) {
			voice->unregister_voice(voice->self);
		}
	}
}

cst_voice* flite_voices_best_match_get(flite_voices_t *voices, mrcp_message_t *message)
{
	cst_voice *voice = NULL;
	const char *voice_name = NULL;
	mrcp_synth_header_t *synth_header = mrcp_resource_header_get(message);
	if(synth_header) {
		if(mrcp_resource_header_property_check(message,SYNTHESIZER_HEADER_VOICE_NAME) == TRUE) {
			voice_name = synth_header->voice_param.name.buf;
		}
	}

	if(voice_name) {
		/* get voice by name */
		flite_voice_t *flite_voice;
		flite_voice = apr_hash_get(voices->table,voice_name,APR_HASH_KEY_STRING);
		if(flite_voice) {
			voice = flite_voice->self;
		}
	}

	if(!voice) {
		/* still no voice found, get the default one */
		flite_voice_t *flite_voice = NULL;
		void *val;
		apr_hash_index_t *it = apr_hash_first(voices->pool,voices->table);
		apr_hash_this(it,NULL,NULL,&val);
		if(val) {
			flite_voice = val;
			voice = flite_voice->self;
		}
	}
	return voice;
}
