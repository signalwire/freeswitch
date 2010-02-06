/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2010, Anthony Minessale II <anthm@freeswitch.org>
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
 *
 * mod_flite.c -- Flite Interface
 *
 */

#include <switch.h>
#include <flite.h>

cst_voice *register_cmu_us_awb(void);
void unregister_cmu_us_awb(cst_voice * v);

cst_voice *register_cmu_us_kal(void);
void unregister_cmu_us_kal(cst_voice * v);

cst_voice *register_cmu_us_rms(void);
void unregister_cmu_us_rms(cst_voice * v);

cst_voice *register_cmu_us_slt(void);
void unregister_cmu_us_slt(cst_voice * v);

SWITCH_MODULE_LOAD_FUNCTION(mod_flite_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_flite_shutdown);
SWITCH_MODULE_DEFINITION(mod_flite, mod_flite_load, mod_flite_shutdown, NULL);

static struct {
	cst_voice *awb;
	cst_voice *kal;
	cst_voice *rms;
	cst_voice *slt;
} globals;

struct flite_data {
	cst_voice *v;
	cst_wave *w;
	switch_buffer_t *audio_buffer;
};

typedef struct flite_data flite_t;

#define free_wave(w) if (w) {delete_wave(w) ; w = NULL; }
#define FLITE_BLOCK_SIZE 1024 * 32

static switch_status_t flite_speech_open(switch_speech_handle_t *sh, const char *voice_name, int rate, switch_speech_flag_t *flags)
{
	flite_t *flite = switch_core_alloc(sh->memory_pool, sizeof(*flite));

	sh->native_rate = 16000;

	if (!strcasecmp(voice_name, "awb")) {
		flite->v = globals.awb;
	} else if (!strcasecmp(voice_name, "kal")) {
		flite->v = globals.kal;
	} else if (!strcasecmp(voice_name, "rms")) {
		flite->v = globals.rms;
	} else if (!strcasecmp(voice_name, "slt")) {
		flite->v = globals.slt;
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Valid voice names are awb, kal, rms or slt.\n");
	}

	if (flite->v) {
		sh->private_info = flite;
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_FALSE;
}

static switch_status_t flite_speech_close(switch_speech_handle_t *sh, switch_speech_flag_t *flags)
{
	flite_t *flite = (flite_t *) sh->private_info;

	if (flite->audio_buffer) {
		switch_buffer_destroy(&flite->audio_buffer);
	}

	free_wave(flite->w);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t flite_speech_feed_tts(switch_speech_handle_t *sh, char *text, switch_speech_flag_t *flags)
{
	flite_t *flite = (flite_t *) sh->private_info;

	flite->w = flite_text_to_wave(text, flite->v);

	return SWITCH_STATUS_SUCCESS;
}

static void flite_speech_flush_tts(switch_speech_handle_t *sh)
{
	flite_t *flite = (flite_t *) sh->private_info;

	if (flite->audio_buffer) {
		switch_buffer_zero(flite->audio_buffer);
	}
	free_wave(flite->w);
}

static switch_status_t flite_speech_read_tts(switch_speech_handle_t *sh, void *data, size_t *datalen, switch_speech_flag_t *flags)
{
	size_t bytes_read;
	flite_t *flite = (flite_t *) sh->private_info;

	if (!flite->audio_buffer) {
		int32_t len;

		if (flite->w) {
			len = flite->w->num_samples * 2;
		} else {
			len = FLITE_BLOCK_SIZE;
		}

		switch_buffer_create_dynamic(&flite->audio_buffer, FLITE_BLOCK_SIZE, len, 0);
		switch_assert(flite->audio_buffer);
	}

	if (flite->w) {
		switch_buffer_write(flite->audio_buffer, flite->w->samples, flite->w->num_samples * 2);
		free_wave(flite->w);
	}

	if ((bytes_read = switch_buffer_read(flite->audio_buffer, data, *datalen))) {
		*datalen = bytes_read;
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_FALSE;
}

static void flite_text_param_tts(switch_speech_handle_t *sh, char *param, const char *val)
{

}

static void flite_numeric_param_tts(switch_speech_handle_t *sh, char *param, int val)
{

}

static void flite_float_param_tts(switch_speech_handle_t *sh, char *param, double val)
{

}

SWITCH_MODULE_LOAD_FUNCTION(mod_flite_load)
{
	switch_speech_interface_t *speech_interface;

	flite_init();
	globals.awb = register_cmu_us_awb();
	globals.kal = register_cmu_us_kal();
	globals.rms = register_cmu_us_rms();
	globals.slt = register_cmu_us_slt();

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	speech_interface = switch_loadable_module_create_interface(*module_interface, SWITCH_SPEECH_INTERFACE);
	speech_interface->interface_name = "flite";
	speech_interface->speech_open = flite_speech_open;
	speech_interface->speech_close = flite_speech_close;
	speech_interface->speech_feed_tts = flite_speech_feed_tts;
	speech_interface->speech_read_tts = flite_speech_read_tts;
	speech_interface->speech_flush_tts = flite_speech_flush_tts;
	speech_interface->speech_text_param_tts = flite_text_param_tts;
	speech_interface->speech_numeric_param_tts = flite_numeric_param_tts;
	speech_interface->speech_float_param_tts = flite_float_param_tts;

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_flite_shutdown)
{
	unregister_cmu_us_awb(globals.awb);
	unregister_cmu_us_kal(globals.kal);
	unregister_cmu_us_rms(globals.rms);
	unregister_cmu_us_slt(globals.slt);

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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */
