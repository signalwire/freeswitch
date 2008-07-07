/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005/2006, Anthony Minessale II <anthmct@yahoo.com>
 *
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is
 * Anthony Minessale II <anthmct@yahoo.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Brian West <brian@freeswitch.org>
 *
 * mod_pocketsphinx - Pocket Sphinx
 * 
 *
 */

#include <switch.h>
#include <pocketsphinx.h>
#define MODELDIR "/usr/local/share/pocketsphinx/model"

SWITCH_MODULE_LOAD_FUNCTION(mod_pocketsphinx_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_pocketsphinx_shutdown);
SWITCH_MODULE_DEFINITION(mod_pocketsphinx, mod_pocketsphinx_load, mod_pocketsphinx_shutdown, NULL);

static struct {
	char *model;
} globals;

typedef enum {
	PSFLAG_HAS_TEXT = (1 << 0),
	PSFLAG_READY = (1 << 1),
	PSFLAG_BARGE = (1 << 2),
	PSFLAG_ALLOCATED = (1 << 3)
} psflag_t;

typedef struct {
	ps_decoder_t *ps;
	uint32_t flags;
	switch_mutex_t *flag_mutex;
	uint32_t org_silence_hits;
	uint32_t thresh;
    uint32_t silence_hits;
	uint32_t listen_hits;
	uint32_t listening;
	uint32_t countdown;
	char *hyp;
	char *grammar;
	int32_t score;
	cmd_ln_t *config;
} pocketsphinx_t;

/*! function to open the asr interface */
static switch_status_t pocketsphinx_asr_open(switch_asr_handle_t *ah, const char *codec, int rate, const char *dest, switch_asr_flag_t *flags)
{

	pocketsphinx_t *ps;

	if (!(ps = (pocketsphinx_t *) switch_core_alloc(ah->memory_pool, sizeof(*ps)))) {
		return SWITCH_STATUS_MEMERR;
	}
	

	switch_mutex_init(&ps->flag_mutex, SWITCH_MUTEX_NESTED, ah->memory_pool);
	ah->private_info = ps;

	codec = "L16";
	ah->rate = 8000;
	ah->codec = switch_core_strdup(ah->memory_pool, codec); 

	
	ps->thresh = 400;
	ps->silence_hits = 35;
	ps->org_silence_hits = ps->silence_hits;
	ps->listen_hits = 5;

	return SWITCH_STATUS_SUCCESS;
}

/*! function to load a grammar to the asr interface */
static switch_status_t pocketsphinx_asr_load_grammar(switch_asr_handle_t *ah, const char *grammar, const char *path)
{
	char *lm, *dic, *model;
	pocketsphinx_t *ps = (pocketsphinx_t *) ah->private_info;
	switch_status_t status = SWITCH_STATUS_FALSE;

	if (switch_test_flag(ps, PSFLAG_READY)) {
		ps_end_utt(ps->ps);
		switch_clear_flag(ps, PSFLAG_READY);
	}

	if (switch_is_file_path(grammar)) {
		lm = switch_mprintf("%s%s%s.lm", grammar, SWITCH_PATH_SEPARATOR, grammar);
		dic = switch_mprintf("%s%s%s.dic", grammar, SWITCH_PATH_SEPARATOR, grammar);
	} else {
		lm = switch_mprintf("%s%s%s%s%s.lm", SWITCH_GLOBAL_dirs.grammar_dir, SWITCH_PATH_SEPARATOR, grammar, SWITCH_PATH_SEPARATOR, grammar);
		dic = switch_mprintf("%s%s%s%s%s.dic", SWITCH_GLOBAL_dirs.grammar_dir, SWITCH_PATH_SEPARATOR, grammar, SWITCH_PATH_SEPARATOR, grammar);
	}

	model = switch_mprintf("%s%smodel%s%s", SWITCH_GLOBAL_dirs.grammar_dir, SWITCH_PATH_SEPARATOR, SWITCH_PATH_SEPARATOR, globals.model);

	switch_assert(lm && dic && model);
	
	ps->config = cmd_ln_init(ps->config, ps_args(), TRUE,
							 "-samprate", "8000",
							 "-hmm", model,
							 "-lm", lm, 
							 "-dict", dic,
							 NULL);
	  
	if (ps->config == NULL) {
		return SWITCH_STATUS_GENERR;
	}

	switch_mutex_lock(ps->flag_mutex);
	if (switch_test_flag(ps, PSFLAG_ALLOCATED)) {
		ps_reinit(ps->ps, ps->config);
	} else {
		if (!(ps->ps = ps_init(ps->config))) {
			switch_mutex_unlock(ps->flag_mutex);
			goto end;
		}
		switch_set_flag(ps, PSFLAG_ALLOCATED);
	}
	switch_mutex_unlock(ps->flag_mutex);

	ps_start_utt(ps->ps, NULL);
	switch_set_flag(ps, PSFLAG_READY);	
	switch_safe_free(ps->grammar);
	ps->grammar = strdup(grammar);

	status = SWITCH_STATUS_SUCCESS;

 end:

	
	switch_safe_free(lm);
	switch_safe_free(dic);
	switch_safe_free(model);
	
	return status;
}

/*! function to unload a grammar to the asr interface */
static switch_status_t pocketsphinx_asr_unload_grammar(switch_asr_handle_t *ah, const char *grammar)
{
	// not sure an unload exists.
	return SWITCH_STATUS_SUCCESS;
}

/*! function to close the asr interface */
static switch_status_t pocketsphinx_asr_close(switch_asr_handle_t *ah, switch_asr_flag_t *flags)
{
	char const *hyp, *uttid;
	int32_t score;	
	pocketsphinx_t *ps = (pocketsphinx_t *) ah->private_info;

	switch_mutex_lock(ps->flag_mutex);
	if (switch_test_flag(ps, PSFLAG_ALLOCATED)) {
		if (switch_test_flag(ps, PSFLAG_READY)) {
			ps_end_utt(ps->ps);
			hyp = ps_get_hyp(ps->ps, &score, &uttid);
		}
		ps_free(ps->ps);
		ps->ps = NULL;
	}
	switch_safe_free(ps->grammar);
	switch_mutex_unlock(ps->flag_mutex);
	switch_clear_flag(ps, PSFLAG_HAS_TEXT);
	switch_clear_flag(ps, PSFLAG_READY);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Port Closed.\n"); 
	switch_set_flag(ah, SWITCH_ASR_FLAG_CLOSED);
	return SWITCH_STATUS_SUCCESS;
}

static switch_bool_t stop_detect(pocketsphinx_t *ps, int16_t *data, unsigned int samples)
{
	uint32_t score, count = 0, j = 0;
	double energy = 0;
	
	if (ps->countdown) {
		if (!--ps->countdown) {
			ps->silence_hits = ps->org_silence_hits;
			ps->listening = 0;
			return SWITCH_TRUE;
		}
		return SWITCH_FALSE;
	}
	

	for (count = 0; count < samples; count++) {
		energy += abs(data[j]);
	}
	
	score = (uint32_t) (energy / samples);

	if (score >= ps->thresh) {
		if (++ps->listening == 1) {
			switch_set_flag_locked(ps, PSFLAG_BARGE);
		}
	}

	if (ps->listening > ps->listen_hits && score < ps->thresh) {
		if (!--ps->silence_hits) {
			ps->countdown = 12;
		}
	} else {
		ps->silence_hits = ps->org_silence_hits;
	}


	return SWITCH_FALSE;
}


/*! function to feed audio to the ASR */
static switch_status_t pocketsphinx_asr_feed(switch_asr_handle_t *ah, void *data, unsigned int len, switch_asr_flag_t *flags)
{
	// we really should put ps_start_utt(ps->ps, NULL); here when we start to feed it.
	pocketsphinx_t *ps = (pocketsphinx_t *) ah->private_info;
	int rv = 0;

	if (switch_test_flag(ah, SWITCH_ASR_FLAG_CLOSED)) return SWITCH_STATUS_BREAK; 
	
	if (!switch_test_flag(ps, PSFLAG_HAS_TEXT) && switch_test_flag(ps, PSFLAG_READY)) {

		switch_mutex_lock(ps->flag_mutex);
		rv = ps_process_raw(ps->ps, (int16 *)data, len / 2 , FALSE, FALSE);
		switch_mutex_unlock(ps->flag_mutex);

		if (rv < 0) {
			return SWITCH_STATUS_FALSE;
		}

		if (stop_detect(ps, (int16_t *)data, len / 2)) {
			char const *hyp, *uttid;

			switch_mutex_lock(ps->flag_mutex); 
			if ((hyp = ps_get_hyp(ps->ps, &ps->score, &uttid))) {
				if (!switch_strlen_zero(hyp)) {
					ps_end_utt(ps->ps);
					switch_clear_flag(ps, PSFLAG_READY);
					if ((hyp = ps_get_hyp(ps->ps, &ps->score, &uttid))) {
						if (switch_strlen_zero(hyp)) {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Lost the text, nevermind....\n");   
							ps_start_utt(ps->ps, NULL);
							switch_set_flag(ps, PSFLAG_READY);
						} else {
							ps->hyp = switch_core_strdup(ah->memory_pool, hyp);
							switch_set_flag(ps, PSFLAG_HAS_TEXT);
						}
					}
				}
			}
			switch_mutex_unlock(ps->flag_mutex);
		}
	}

	return SWITCH_STATUS_SUCCESS;
}

/*! funciton to pause recognizer */
static switch_status_t pocketsphinx_asr_pause(switch_asr_handle_t *ah)
{
	pocketsphinx_t *ps = (pocketsphinx_t *) ah->private_info;
	switch_status_t status = SWITCH_STATUS_FALSE;

	switch_mutex_lock(ps->flag_mutex);		
	if (switch_test_flag(ps, PSFLAG_READY)) {
		ps_end_utt(ps->ps);
		switch_clear_flag(ps, PSFLAG_READY);
		status = SWITCH_STATUS_SUCCESS;
	}
	switch_mutex_unlock(ps->flag_mutex);		

	return status;
}

/*! function to resume recognizer */
static switch_status_t pocketsphinx_asr_resume(switch_asr_handle_t *ah)
{
	pocketsphinx_t *ps = (pocketsphinx_t *) ah->private_info;
	switch_status_t status = SWITCH_STATUS_FALSE;
	
	switch_mutex_lock(ps->flag_mutex);		
	switch_clear_flag(ps, PSFLAG_HAS_TEXT);
	if (!switch_test_flag(ps, PSFLAG_READY)) { 
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Manually Resuming\n");   
		
		if (ps_start_utt(ps->ps, NULL)) {
			status = SWITCH_STATUS_GENERR;
		} else {
			switch_set_flag(ps, PSFLAG_READY); 
		}
	}
	switch_mutex_unlock(ps->flag_mutex);		


	return status;
}

/*! function to read results from the ASR*/
static switch_status_t pocketsphinx_asr_check_results(switch_asr_handle_t *ah, switch_asr_flag_t *flags)
{
	pocketsphinx_t *ps = (pocketsphinx_t *) ah->private_info;

	return (switch_test_flag(ps, PSFLAG_HAS_TEXT) || switch_test_flag(ps, PSFLAG_BARGE)) ? SWITCH_STATUS_SUCCESS : SWITCH_STATUS_FALSE;
}

/*! function to read results from the ASR*/
static switch_status_t pocketsphinx_asr_get_results(switch_asr_handle_t *ah, char **xmlstr, switch_asr_flag_t *flags)
{
	pocketsphinx_t *ps = (pocketsphinx_t *) ah->private_info; 
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if (switch_test_flag(ps, PSFLAG_BARGE)) {
		switch_clear_flag_locked(ps, PSFLAG_BARGE);
		status = SWITCH_STATUS_BREAK;
	}

	if (switch_test_flag(ps, PSFLAG_HAS_TEXT)) {
		switch_mutex_lock(ps->flag_mutex); 
		switch_clear_flag(ps, PSFLAG_HAS_TEXT);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Recognized: %s, Score: %d\n", ps->hyp, ps->score);
		switch_mutex_unlock(ps->flag_mutex); 
		//*xmlstr = strdup((char *)ps->hyp ); 
		
		*xmlstr = switch_mprintf("<interpretation grammar=\"%s\" score=\"%d\">\n"
								 "  <result name=\"%s\">%s</result>\n"
								 "  <input>%s</input>\n"
								 "</interpretation>",
								 
								 ps->grammar, ps->score,
 								 "match", 
								 ps->hyp, 
								 ps->hyp
								 );

		
		if (switch_test_flag(ps, SWITCH_ASR_FLAG_AUTO_RESUME)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Auto Resuming\n");
			switch_set_flag(ps, PSFLAG_READY);
		
			ps_start_utt(ps->ps, NULL);
		}

		status = SWITCH_STATUS_SUCCESS;
	}


	return status;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_pocketsphinx_load)
{
	switch_asr_interface_t *asr_interface;
	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	asr_interface = switch_loadable_module_create_interface(*module_interface, SWITCH_ASR_INTERFACE);
	asr_interface->interface_name = "pocketsphinx";
	asr_interface->asr_open = pocketsphinx_asr_open;
	asr_interface->asr_load_grammar = pocketsphinx_asr_load_grammar;
	asr_interface->asr_unload_grammar = pocketsphinx_asr_unload_grammar;
	asr_interface->asr_close = pocketsphinx_asr_close;
	asr_interface->asr_feed = pocketsphinx_asr_feed;
	asr_interface->asr_resume = pocketsphinx_asr_resume;
	asr_interface->asr_pause = pocketsphinx_asr_pause;
	asr_interface->asr_check_results = pocketsphinx_asr_check_results;
	asr_interface->asr_get_results = pocketsphinx_asr_get_results;

	globals.model = switch_core_strdup(pool, "communicator");
	
	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_pocketsphinx_shutdown)
{
	return SWITCH_STATUS_UNLOAD;
}
