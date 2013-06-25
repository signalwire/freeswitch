/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2013, Anthony Minessale II <anthm@freeswitch.org>
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
 * Anthony Minessale II <anthm@freeswitch.org>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Brian West <brian@freeswitch.org>
 * Christopher Rienzo <chris.rienzo@grasshopper.com>
 *
 * mod_pocketsphinx - Pocket Sphinx
 *
 *
 */

#include <switch.h>
#include <pocketsphinx.h>
#include <sphinxbase/err.h>
#include <sphinxbase/logmath.h>

SWITCH_MODULE_LOAD_FUNCTION(mod_pocketsphinx_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_pocketsphinx_shutdown);
SWITCH_MODULE_DEFINITION(mod_pocketsphinx, mod_pocketsphinx_load, mod_pocketsphinx_shutdown, NULL);

static switch_mutex_t *MUTEX = NULL;
static switch_event_node_t *NODE = NULL;

static struct {
	char *model8k;
	char *model16k;
	char *dictionary;
	char *language_weight;
	uint32_t thresh;
	int no_input_timeout;
	int speech_timeout;
	switch_bool_t start_input_timers;
	int confidence_threshold;
	uint32_t silence_hits;
	uint32_t listen_hits;
	int auto_reload;
	switch_memory_pool_t *pool;
} globals;

typedef enum {
	PSFLAG_HAS_TEXT = (1 << 0),
	PSFLAG_READY = (1 << 1),
	PSFLAG_BARGE = (1 << 2),
	PSFLAG_ALLOCATED = (1 << 3),
	PSFLAG_INPUT_TIMERS = (1 << 4),
	PSFLAG_START_OF_SPEECH = (1 << 5),
	PSFLAG_NOINPUT_TIMEOUT = (1 << 6),
	PSFLAG_SPEECH_TIMEOUT = (1 << 7),
	PSFLAG_NOINPUT = (1 << 8),
	PSFLAG_NOMATCH = (1 << 9)
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
	int no_input_timeout;
	int speech_timeout;
	switch_bool_t start_input_timers;
	switch_time_t silence_time;
	int confidence_threshold;
	char *hyp;
	char *grammar;
	int32_t score;
	int32_t confidence;
	char const *uttid;
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

	if (rate == 8000) {
		ah->rate = 8000;
	} else if (rate == 16000) {
		ah->rate = 16000;
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid rate %d. Only 8000 and 16000 are supported.\n", rate);
	}

	codec = "L16";

	ah->codec = switch_core_strdup(ah->memory_pool, codec);


	ps->thresh = globals.thresh;
	ps->silence_hits = globals.silence_hits;
	ps->listen_hits = globals.listen_hits;
	ps->org_silence_hits = ps->silence_hits;
	ps->start_input_timers = globals.start_input_timers;
	ps->no_input_timeout = globals.no_input_timeout;
	ps->speech_timeout = globals.speech_timeout;
	ps->confidence_threshold = globals.confidence_threshold;

	return SWITCH_STATUS_SUCCESS;
}

/*! function to load a grammar to the asr interface */
static switch_status_t pocketsphinx_asr_load_grammar(switch_asr_handle_t *ah, const char *grammar, const char *name)
{
	char *jsgf, *dic, *model, *rate = NULL;
	pocketsphinx_t *ps = (pocketsphinx_t *) ah->private_info;
	switch_status_t status = SWITCH_STATUS_FALSE;

	if (switch_test_flag(ps, PSFLAG_READY)) {
		ps_end_utt(ps->ps);
		switch_clear_flag(ps, PSFLAG_READY);
	}

	if (switch_is_file_path(grammar)) {
		char *dot = strrchr(grammar, '.');
		if (dot && !strcmp(dot, ".gram")) {
			jsgf = strdup(grammar);
		} else {
			jsgf = switch_mprintf("%s.gram", grammar);
		}
	} else {
		jsgf = switch_mprintf("%s%s%s.gram", SWITCH_GLOBAL_dirs.grammar_dir, SWITCH_PATH_SEPARATOR, grammar);
	}

	if (ah->rate == 8000) {
		model = switch_mprintf("%s%smodel%s%s", SWITCH_GLOBAL_dirs.grammar_dir, SWITCH_PATH_SEPARATOR, SWITCH_PATH_SEPARATOR, globals.model8k);
	} else {
		model = switch_mprintf("%s%smodel%s%s", SWITCH_GLOBAL_dirs.grammar_dir, SWITCH_PATH_SEPARATOR, SWITCH_PATH_SEPARATOR, globals.model16k);
	}

	dic = switch_mprintf("%s%s%s", SWITCH_GLOBAL_dirs.grammar_dir, SWITCH_PATH_SEPARATOR, globals.dictionary);

	if (switch_file_exists(dic, ah->memory_pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't open dictionary %s.\n", dic);
		goto end;
	}

	if (switch_file_exists(model, ah->memory_pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Can't open speech model %s.\n", model);
		goto end;
	}

	if (switch_file_exists(jsgf, ah->memory_pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Can't open grammar file %s.\n", jsgf);
		goto end;
	}

	rate = switch_mprintf("%d", ah->rate);

	switch_assert(jsgf && dic && model);

	ps->config = cmd_ln_init(ps->config, ps_args(), FALSE,
							 "-samprate", rate,
							 "-hmm", model, "-jsgf", jsgf, "-lw", globals.language_weight, "-dict", dic, "-frate", "50", "-silprob", "0.005", NULL);

	if (ps->config == NULL) {
		status = SWITCH_STATUS_GENERR;
		goto end;
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
	ps->silence_time = switch_micro_time_now();
	switch_clear_flag(ps, PSFLAG_START_OF_SPEECH);
	switch_clear_flag(ps, PSFLAG_NOINPUT_TIMEOUT);
	switch_clear_flag(ps, PSFLAG_NOINPUT);
	switch_clear_flag(ps, PSFLAG_NOMATCH);
	switch_clear_flag(ps, PSFLAG_SPEECH_TIMEOUT);
	if (ps->start_input_timers) {
		switch_set_flag(ps, PSFLAG_INPUT_TIMERS);
	} else {
		switch_clear_flag(ps, PSFLAG_INPUT_TIMERS);
	}
	switch_set_flag(ps, PSFLAG_READY);
	switch_safe_free(ps->grammar);
	ps->grammar = strdup(grammar);

	status = SWITCH_STATUS_SUCCESS;

  end:

	switch_safe_free(rate);
	switch_safe_free(jsgf);
	switch_safe_free(dic);
	switch_safe_free(model);

	return status;
}

/*! function to unload a grammar to the asr interface */
static switch_status_t pocketsphinx_asr_unload_grammar(switch_asr_handle_t *ah, const char *name)
{
	return SWITCH_STATUS_SUCCESS;
}

/*! function to close the asr interface */
static switch_status_t pocketsphinx_asr_close(switch_asr_handle_t *ah, switch_asr_flag_t *flags)
{
	pocketsphinx_t *ps = (pocketsphinx_t *) ah->private_info;

	switch_mutex_lock(ps->flag_mutex);
	if (switch_test_flag(ps, PSFLAG_ALLOCATED)) {
		if (switch_test_flag(ps, PSFLAG_READY)) {
			ps_end_utt(ps->ps);
		}
		ps_free(ps->ps);
		ps->ps = NULL;
	}
	switch_safe_free(ps->grammar);
	switch_mutex_unlock(ps->flag_mutex);
	switch_clear_flag(ps, PSFLAG_HAS_TEXT);
	switch_clear_flag(ps, PSFLAG_READY);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Port Closed.\n");
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

	/* Do simple energy threshold for VAD */
	for (count = 0; count < samples; count++) {
		energy += abs(data[j]);
	}

	score = (uint32_t) (energy / samples);

	if (score >= ps->thresh) {
		if (++ps->listening == 1) {
			switch_mutex_lock(ps->flag_mutex);
			switch_set_flag(ps, PSFLAG_BARGE);
			switch_set_flag(ps, PSFLAG_START_OF_SPEECH);
			switch_mutex_unlock(ps->flag_mutex);
		}
		ps->silence_time = 0;
	} else if (!ps->silence_time) {
		ps->silence_time = switch_micro_time_now();
	}

	/* Check silence timeouts */
	if (ps->silence_time && switch_test_flag(ps, PSFLAG_INPUT_TIMERS)) {
		int elapsed_ms = (switch_micro_time_now() - ps->silence_time) / 1000;
		if (switch_test_flag(ps, PSFLAG_START_OF_SPEECH)) {
			if (ps->speech_timeout > 0 && !switch_test_flag(ps, PSFLAG_SPEECH_TIMEOUT) && elapsed_ms >= ps->speech_timeout) {
				switch_set_flag_locked(ps, PSFLAG_SPEECH_TIMEOUT);
				ps->listening = 0;
				return SWITCH_TRUE;
			}
		} else {
			if (ps->no_input_timeout > 0 && !switch_test_flag(ps, PSFLAG_NOINPUT_TIMEOUT) && elapsed_ms >= ps->no_input_timeout) {
				switch_mutex_lock(ps->flag_mutex);
				switch_set_flag(ps, PSFLAG_NOINPUT_TIMEOUT);
				switch_set_flag(ps, PSFLAG_NOINPUT);
				switch_mutex_unlock(ps->flag_mutex);
				ps->listening = 0;
				return SWITCH_TRUE;
			}
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
	pocketsphinx_t *ps = (pocketsphinx_t *) ah->private_info;
	int rv = 0;

	if (switch_test_flag(ah, SWITCH_ASR_FLAG_CLOSED))
		return SWITCH_STATUS_BREAK;

	if (!switch_test_flag(ps, PSFLAG_NOMATCH) && !switch_test_flag(ps, PSFLAG_NOINPUT) && !switch_test_flag(ps, PSFLAG_HAS_TEXT) && switch_test_flag(ps, PSFLAG_READY)) {
		if (stop_detect(ps, (int16_t *) data, len / 2)) {
			char const *hyp;

			switch_mutex_lock(ps->flag_mutex);
			if ((hyp = ps_get_hyp(ps->ps, &ps->score, &ps->uttid))) {
				if (!zstr(hyp)) {
					ps_end_utt(ps->ps);
					switch_clear_flag(ps, PSFLAG_READY);
					if ((hyp = ps_get_hyp(ps->ps, &ps->score, &ps->uttid))) {
						if (zstr(hyp)) {
							if (!switch_test_flag(ps, PSFLAG_SPEECH_TIMEOUT)) {
								switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Lost the text, never mind....\n");
								ps_start_utt(ps->ps, NULL);
								switch_set_flag(ps, PSFLAG_READY);
							}
						} else {
							/* get match and confidence */
							int32_t conf;

							conf = ps_get_prob(ps->ps, &ps->uttid);

							ps->confidence = (conf + 20000) / 200;

							if (ps->confidence < 0) {
								ps->confidence = 0;
							}

							if (ps->confidence_threshold <= 0 || ps->confidence >= ps->confidence_threshold) {
								ps->hyp = switch_core_strdup(ah->memory_pool, hyp);
								switch_set_flag(ps, PSFLAG_HAS_TEXT);
							} else {
								/* have match, but below confidence threshold */
								switch_set_flag(ps, PSFLAG_NOMATCH);
							}
						}
					}
				}
			}
			if (switch_test_flag(ps, PSFLAG_SPEECH_TIMEOUT) && !switch_test_flag(ps, PSFLAG_HAS_TEXT)) {
				/* heard something, but doesn't match anything */
				switch_clear_flag(ps, PSFLAG_READY);
				switch_set_flag(ps, PSFLAG_NOMATCH);
			}
			switch_mutex_unlock(ps->flag_mutex);
		}

		/* only feed ps_process_raw when we are listening */
		if (ps->listening) {
			switch_mutex_lock(ps->flag_mutex);
			rv = ps_process_raw(ps->ps, (int16 *) data, len / 2, FALSE, FALSE);
			switch_mutex_unlock(ps->flag_mutex);
		}

		if (rv < 0) {
			return SWITCH_STATUS_FALSE;
		}
	} else if (switch_test_flag(ps, PSFLAG_NOINPUT_TIMEOUT)) {
		/* never heard anything */
		switch_clear_flag_locked(ps, PSFLAG_READY);
	}

	return SWITCH_STATUS_SUCCESS;
}

/*! function to pause recognizer */
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
	ps->silence_time = switch_micro_time_now();
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

	return (switch_test_flag(ps, PSFLAG_NOINPUT) || switch_test_flag(ps, PSFLAG_NOMATCH) || switch_test_flag(ps, PSFLAG_HAS_TEXT) || switch_test_flag(ps, PSFLAG_BARGE)) ? SWITCH_STATUS_SUCCESS : SWITCH_STATUS_FALSE;
}

/*! function to read results from the ASR */
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

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Recognized: %s, Confidence: %d, Confidence-Threshold: %d\n", ps->hyp, ps->confidence, ps->confidence_threshold);
		switch_mutex_unlock(ps->flag_mutex);

		*xmlstr = switch_mprintf("<?xml version=\"1.0\"?>\n"
								 "<result grammar=\"%s\">\n"
								 "  <interpretation grammar=\"%s\" confidence=\"%d\">\n"
								 "    <input mode=\"speech\">%s</input>\n"
								 "  </interpretation>\n" "</result>\n", ps->grammar, ps->grammar, ps->confidence, ps->hyp);

		if (!switch_test_flag(ps, PSFLAG_INPUT_TIMERS) && switch_test_flag(ah, SWITCH_ASR_FLAG_AUTO_RESUME)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Auto Resuming\n");
			switch_set_flag(ps, PSFLAG_READY);

			ps_start_utt(ps->ps, NULL);
		}

		status = SWITCH_STATUS_SUCCESS;
	} else if (switch_test_flag(ps, PSFLAG_NOINPUT)) {
		switch_clear_flag_locked(ps, PSFLAG_NOINPUT);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "NO INPUT\n");

		*xmlstr = switch_mprintf("<?xml version=\"1.0\"?>\n"
								 "<result grammar=\"%s\">\n"
								 "  <interpretation>\n"
								 "    <input mode=\"speech\"><noinput/></input>\n"
								 "  </interpretation>\n"
								 "</result>\n", ps->grammar);

		status = SWITCH_STATUS_SUCCESS;
	} else if (switch_test_flag(ps, PSFLAG_NOMATCH)) {
		switch_clear_flag_locked(ps, PSFLAG_NOMATCH);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "NO MATCH\n");

		*xmlstr = switch_mprintf("<?xml version=\"1.0\"?>\n"
								 "<result grammar=\"%s\">\n"
								 "  <interpretation>\n"
								 "    <input mode=\"speech\"><nomatch/></input>\n"
								 "  </interpretation>\n"
								 "</result>\n", ps->grammar);

		status = SWITCH_STATUS_SUCCESS;
	}

	return status;
}

/*! function to start input timeouts */
static switch_status_t pocketsphinx_asr_start_input_timers(switch_asr_handle_t *ah)
{
	pocketsphinx_t *ps = (pocketsphinx_t *) ah->private_info;
	switch_set_flag_locked(ps, PSFLAG_INPUT_TIMERS);
	return SWITCH_STATUS_SUCCESS;
}

/*! set text parameter */
static void pocketsphinx_asr_text_param(switch_asr_handle_t *ah, char *param, const char *val)
{
	pocketsphinx_t *ps = (pocketsphinx_t *) ah->private_info;
	if (!zstr(param) && !zstr(val)) {
		if (!strcasecmp("no-input-timeout", param) && switch_is_number(val)) {
			ps->no_input_timeout = atoi(val);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "no-input-timeout = %d\n", ps->no_input_timeout);
		} else if (!strcasecmp("speech-timeout", param) && switch_is_number(val)) {
			ps->speech_timeout = atoi(val);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "speech-timeout = %d\n", ps->speech_timeout);
		} else if (!strcasecmp("start-input-timers", param)) {
			ps->start_input_timers = switch_true(val);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "start-input-timers = %d\n", ps->start_input_timers);
		} else if (!strcasecmp("confidence-threshold", param) && switch_is_number(val)) {
			ps->confidence_threshold = atoi(val);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "confidence-threshold = %d\n", ps->confidence_threshold);
		}
	}
}

/*! set numeric parameter */
static void pocketsphinx_asr_numeric_param(switch_asr_handle_t *ah, char *param, int val)
{
	char *val_str = switch_mprintf("%d", val);
	pocketsphinx_asr_text_param(ah, param, val_str);
	switch_safe_free(val_str);
}

/*! set float parameter */
static void pocketsphinx_asr_float_param(switch_asr_handle_t *ah, char *param, double val)
{
	char *val_str = switch_mprintf("%f", val);
	pocketsphinx_asr_text_param(ah, param, val_str);
	switch_safe_free(val_str);
}

static switch_status_t load_config(void)
{
	char *cf = "pocketsphinx.conf";
	switch_xml_t cfg, xml = NULL, param, settings;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	/* Set defaults */
	globals.thresh = 400;
	globals.silence_hits = 35;
	globals.listen_hits = 1;
	globals.auto_reload = 1;
	globals.start_input_timers = SWITCH_FALSE;
	globals.no_input_timeout = 4000;
	globals.speech_timeout = 1000;
	globals.confidence_threshold = 0;

	if (!(xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Open of %s failed\n", cf);
		status = SWITCH_STATUS_FALSE;
		goto done;
	}

	if ((settings = switch_xml_child(cfg, "settings"))) {
		for (param = switch_xml_child(settings, "param"); param; param = param->next) {
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");
			if (!strcasecmp(var, "threshold")) {
				globals.thresh = atoi(val);
			} else if (!strcasecmp(var, "start-input-timers")) {
				globals.start_input_timers = switch_true(val);
			} else if (!strcasecmp(var, "no-input-timeout")) {
				globals.no_input_timeout = atoi(val);
			} else if (!strcasecmp(var, "speech-timeout")) {
				globals.speech_timeout = atoi(val);
			} else if (!strcasecmp(var, "confidence_threshold")) {
				globals.confidence_threshold = atoi(val);
			} else if (!strcasecmp(var, "silence-hits")) {
				globals.silence_hits = atoi(val);
			} else if (!strcasecmp(var, "language-weight")) {
				globals.language_weight = switch_core_strdup(globals.pool, val);
			} else if (!strcasecmp(var, "listen-hits")) {
				globals.listen_hits = atoi(val);
			} else if (!strcasecmp(var, "auto-reload")) {
				globals.auto_reload = switch_true(val);
			} else if (!strcasecmp(var, "narrowband-model")) {
				globals.model8k = switch_core_strdup(globals.pool, val);
			} else if (!strcasecmp(var, "wideband-model")) {
				globals.model16k = switch_core_strdup(globals.pool, val);
			} else if (!strcasecmp(var, "dictionary")) {
				globals.dictionary = switch_core_strdup(globals.pool, val);
			}
		}
	}

	if (!globals.model8k) {
		globals.model8k = switch_core_strdup(globals.pool, "communicator");
	}

	if (!globals.model16k) {
		globals.model16k = switch_core_strdup(globals.pool, "wsj1");
	}

	if (!globals.dictionary) {
		globals.dictionary = switch_core_strdup(globals.pool, "default.dic");
	}

	if (!globals.language_weight) {
		globals.language_weight = switch_core_strdup(globals.pool, "6.5");
	}

  done:
	if (xml) {
		switch_xml_free(xml);
	}

	return status;
}

static void do_load(void)
{
	switch_mutex_lock(MUTEX);
	load_config();
	switch_mutex_unlock(MUTEX);
}

static void event_handler(switch_event_t *event)
{
	if (globals.auto_reload) {
		do_load();
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "PocketSphinx Reloaded\n");
	}
}

SWITCH_MODULE_LOAD_FUNCTION(mod_pocketsphinx_load)
{
	switch_asr_interface_t *asr_interface;

	switch_mutex_init(&MUTEX, SWITCH_MUTEX_NESTED, pool);

	globals.pool = pool;

	if ((switch_event_bind_removable(modname, SWITCH_EVENT_RELOADXML, NULL, event_handler, NULL, &NODE) != SWITCH_STATUS_SUCCESS)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind!\n");
	}

	do_load();

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
	asr_interface->asr_start_input_timers = pocketsphinx_asr_start_input_timers;
	asr_interface->asr_text_param = pocketsphinx_asr_text_param;
	asr_interface->asr_numeric_param = pocketsphinx_asr_numeric_param;
	asr_interface->asr_float_param = pocketsphinx_asr_float_param;

	err_set_logfp(NULL);

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_pocketsphinx_shutdown)
{
	switch_event_unbind(&NODE);
	return SWITCH_STATUS_UNLOAD;
}


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
