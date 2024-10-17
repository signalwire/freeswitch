/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2014, Anthony Minessale II <anthm@freeswitch.org>
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
 * Module Contributor(s):
 *  Konstantin Alexandrin <akscfx@gmail.com>
 *
 *
 * Provides the ability to use PIPER TTS in the Freeswitch
 * https://github.com/rhasspy/piper
 *
 *
 * Development repository:
 * https://github.com/akscf/mod_piper_tts
 *
 */
#include "mod_piper_tts.h"

static piper_globals_t globals;

SWITCH_MODULE_LOAD_FUNCTION(mod_piper_tts_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_piper_tts_shutdown);
SWITCH_MODULE_DEFINITION(mod_piper_tts, mod_piper_tts_load, mod_piper_tts_shutdown, NULL);


static piper_model_info_t *piper_lookup_model(const char *lang) {
    piper_model_info_t *model = NULL;

    if(!lang) {
        return NULL;
    }

    switch_mutex_lock(globals.mutex);
    model = switch_core_hash_find(globals.models, lang);
    switch_mutex_unlock(globals.mutex);

    return model;
}

static switch_status_t speech_open(switch_speech_handle_t *sh, const char *voice, int samplerate, int channels, switch_speech_flag_t *flags) {
    char name_uuid[SWITCH_UUID_FORMATTED_LENGTH + 1] = { 0 };
    switch_status_t status = SWITCH_STATUS_SUCCESS;
    tts_ctx_t *tts_ctx = NULL;

    tts_ctx = switch_core_alloc(sh->memory_pool, sizeof(tts_ctx_t));
    tts_ctx->pool = sh->memory_pool;
    tts_ctx->fhnd = switch_core_alloc(tts_ctx->pool, sizeof(switch_file_handle_t));
    tts_ctx->voice = switch_core_strdup(tts_ctx->pool, voice);
    tts_ctx->language = (globals.fl_voice_as_language && voice ? switch_core_strdup(sh->memory_pool, voice) : "en");
    tts_ctx->channels = channels;
    tts_ctx->samplerate = samplerate;

    sh->private_info = tts_ctx;

    if(tts_ctx->language) {
        tts_ctx->model_info = piper_lookup_model(tts_ctx->language);
        if(!tts_ctx->model_info) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Language '%s' not registered!\n", tts_ctx->language);
            switch_goto_status(SWITCH_STATUS_FALSE, out);
        }
    }

    if(!globals.fl_cache_enabled) {
        switch_uuid_str((char *)name_uuid, sizeof(name_uuid));
        tts_ctx->dst_fname = switch_core_sprintf(sh->memory_pool, "%s%spiper-%s.%s",
                                                 globals.tmp_path,
                                                 SWITCH_PATH_SEPARATOR,
                                                 name_uuid,
                                                 PIPER_FILE_ENCODING
                                );
    }

out:
    return status;
}

static switch_status_t speech_close(switch_speech_handle_t *sh, switch_speech_flag_t *flags) {
    tts_ctx_t *tts_ctx = (tts_ctx_t *) sh->private_info;
    assert(tts_ctx != NULL);

    if(switch_test_flag(tts_ctx->fhnd, SWITCH_FILE_OPEN)) {
        switch_core_file_close(tts_ctx->fhnd);
    }

    if(tts_ctx->dst_fname && !globals.fl_cache_enabled) {
        unlink(tts_ctx->dst_fname);
    }

    return SWITCH_STATUS_SUCCESS;
}

static switch_status_t speech_feed_tts(switch_speech_handle_t *sh, char *text, switch_speech_flag_t *flags) {
    tts_ctx_t *tts_ctx = (tts_ctx_t *)sh->private_info;
    char digest[SWITCH_MD5_DIGEST_STRING_SIZE + 1] = { 0 };
    switch_status_t status = SWITCH_STATUS_SUCCESS;

    assert(tts_ctx != NULL);

    if(!tts_ctx->dst_fname) {
        switch_md5_string(digest, (void *)text, strlen(text));
        tts_ctx->dst_fname = switch_core_sprintf(sh->memory_pool, "%s%s%s.%s",
                                                 globals.cache_path,
                                                 SWITCH_PATH_SEPARATOR,
                                                 digest,
                                                 PIPER_FILE_ENCODING
                            );
    }

    if(switch_file_exists(tts_ctx->dst_fname, tts_ctx->pool) == SWITCH_STATUS_SUCCESS) {
        if((status = switch_core_file_open(tts_ctx->fhnd, tts_ctx->dst_fname, tts_ctx->channels, tts_ctx->samplerate,
                                           (SWITCH_FILE_FLAG_READ | SWITCH_FILE_DATA_SHORT), sh->memory_pool)) != SWITCH_STATUS_SUCCESS) {

            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to open file: %s\n", tts_ctx->dst_fname);
            switch_goto_status(SWITCH_STATUS_FALSE, out);
        }
    } else {
        char *cmd = NULL;
        char *textq = NULL;

        if(!tts_ctx->model_info) {
            if(tts_ctx->language) {
                tts_ctx->model_info = piper_lookup_model(tts_ctx->language);
            }
            if(!tts_ctx->model_info) {
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to lookup the model for lang: %s\n", tts_ctx->language);
                switch_goto_status(SWITCH_STATUS_FALSE, out);
            }
        }

        textq = switch_util_quote_shell_arg(text);
        cmd = switch_mprintf("echo %s | %s %s --model '%s' --output_file '%s'",
                             textq, globals.piper_bin,
                             globals.piper_opts ? globals.piper_opts : "",
                             tts_ctx->model_info->model,
                             tts_ctx->dst_fname
                );

#ifdef PIPER_DEBUG
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "PIPER-CMD: [%s]\n", cmd);
#endif

        if(switch_system(cmd, SWITCH_TRUE)) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to perform cmd: %s\n", cmd);
            status = SWITCH_STATUS_FALSE;
        }

        switch_safe_free(textq);
        switch_safe_free(cmd);

        if(status == SWITCH_STATUS_SUCCESS) {
            if(switch_file_exists(tts_ctx->dst_fname, tts_ctx->pool) == SWITCH_STATUS_SUCCESS) {
                if((status = switch_core_file_open(tts_ctx->fhnd, tts_ctx->dst_fname, tts_ctx->channels, tts_ctx->samplerate,
                                                   (SWITCH_FILE_FLAG_READ | SWITCH_FILE_DATA_SHORT), sh->memory_pool)) != SWITCH_STATUS_SUCCESS) {

                    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to open file: %s\n", tts_ctx->dst_fname);
                    switch_goto_status(SWITCH_STATUS_FALSE, out);
                }
            } else {
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "File not found: %s\n", tts_ctx->dst_fname);
                switch_goto_status(SWITCH_STATUS_FALSE, out);
            }
        }
    }

out:
    return status;
}

static switch_status_t speech_read_tts(switch_speech_handle_t *sh, void *data, size_t *data_len, switch_speech_flag_t *flags) {
    tts_ctx_t *tts_ctx = (tts_ctx_t *) sh->private_info;
    size_t len = (*data_len / sizeof(int16_t));

    assert(tts_ctx != NULL);

    if(tts_ctx->fhnd->file_interface == NULL) {
        return SWITCH_STATUS_FALSE;
    }

    if(switch_core_file_read(tts_ctx->fhnd, data, &len) != SWITCH_STATUS_SUCCESS) {
        switch_core_file_close(tts_ctx->fhnd);
        return SWITCH_STATUS_FALSE;
    }

    *data_len = (len * sizeof(int16_t));
    if(!data_len) {
        switch_core_file_close(tts_ctx->fhnd);
        return SWITCH_STATUS_BREAK;
    }

    return SWITCH_STATUS_SUCCESS;
}

static void speech_flush_tts(switch_speech_handle_t *sh) {
    tts_ctx_t *tts_ctx = (tts_ctx_t *)sh->private_info;

    assert(tts_ctx != NULL);

    if(tts_ctx->fhnd != NULL && tts_ctx->fhnd->file_interface != NULL) {
        switch_core_file_close(tts_ctx->fhnd);
    }
}

static void speech_text_param_tts(switch_speech_handle_t *sh, char *param, const char *val) {
    tts_ctx_t *tts_ctx = (tts_ctx_t *) sh->private_info;

    assert(tts_ctx != NULL);

    if(strcasecmp(param, "lang") == 0) {
        if(val) {  tts_ctx->language = switch_core_strdup(sh->memory_pool, val); }
    } else if(strcasecmp(param, "voice") == 0) {
        if(val) {  tts_ctx->voice = switch_core_strdup(sh->memory_pool, val); }
    }
}

static void speech_numeric_param_tts(switch_speech_handle_t *sh, char *param, int val) {
}

static void speech_float_param_tts(switch_speech_handle_t *sh, char *param, double val) {
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------------------------------------------------------------------------
SWITCH_MODULE_LOAD_FUNCTION(mod_piper_tts_load) {
    switch_status_t status = SWITCH_STATUS_SUCCESS;
    switch_xml_t cfg, xml, settings, param, xmodels, xmodel;
    switch_speech_interface_t *speech_interface;

    memset(&globals, 0, sizeof(globals));
    switch_mutex_init(&globals.mutex, SWITCH_MUTEX_NESTED, pool);
    switch_core_hash_init(&globals.models);

    if((xml = switch_xml_open_cfg(MOD_CONFIG_NAME, &cfg, NULL)) == NULL) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to open configuration: %s\n", MOD_CONFIG_NAME);
        switch_goto_status(SWITCH_STATUS_GENERR, out);
    }

    if((settings = switch_xml_child(cfg, "settings"))) {
        for(param = switch_xml_child(settings, "param"); param; param = param->next) {
            char *var = (char *) switch_xml_attr_soft(param, "name");
            char *val = (char *) switch_xml_attr_soft(param, "value");

            if(!strcasecmp(var, "cache-path")) {
                if(val) globals.cache_path = switch_core_strdup(pool, val);
            } else if(!strcasecmp(var, "piper-bin")) {
                if(val) globals.piper_bin = switch_core_strdup(pool, val);
            } else if(!strcasecmp(var, "piper-opts")) {
                if(val) globals.piper_opts = switch_core_strdup(pool, val);
            } else if(!strcasecmp(var, "voice-name-as-language")) {
                if(val) globals.fl_voice_as_language = switch_true(val);
            } else if(!strcasecmp(var, "cache-enable")) {
                if(val) globals.fl_cache_enabled = switch_true(val);
            }
        }
    }

    if((xmodels = switch_xml_child(cfg, "models"))) {
        for(xmodel = switch_xml_child(xmodels, "model"); xmodel; xmodel = xmodel->next) {
            char *lang = (char *) switch_xml_attr_soft(xmodel, "language");
            char *model = (char *) switch_xml_attr_soft(xmodel, "model");
            piper_model_info_t *model_info = NULL;

            if(!lang || !model) { continue; }

            if(switch_core_hash_find(globals.models, lang)) {
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Language '%s' already registered\n", lang);
                continue;
            }

            if((model_info = switch_core_alloc(pool, sizeof(piper_model_info_t))) == NULL) {
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "switch_core_alloc()\n");
                switch_goto_status(SWITCH_STATUS_GENERR, out);
            }
            model_info->lang = switch_core_strdup(pool, lang);
            model_info->model = switch_core_strdup(pool, model);

            switch_core_hash_insert(globals.models, model_info->lang, model_info);
        }
    }

    if(!globals.piper_bin) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "piper-bin - not determined!\n");
        switch_goto_status(SWITCH_STATUS_FALSE, out);
    }

    globals.tmp_path = SWITCH_GLOBAL_dirs.temp_dir;
    globals.cache_path = (globals.cache_path == NULL ? "/tmp/piper-tts-cache" : globals.cache_path);

    if(switch_directory_exists(globals.cache_path, NULL) != SWITCH_STATUS_SUCCESS) {
        switch_dir_make(globals.cache_path, SWITCH_FPROT_OS_DEFAULT, NULL);
    }

    *module_interface = switch_loadable_module_create_module_interface(pool, modname);
    speech_interface = switch_loadable_module_create_interface(*module_interface, SWITCH_SPEECH_INTERFACE);
    speech_interface->interface_name = "piper";

    speech_interface->speech_open = speech_open;
    speech_interface->speech_close = speech_close;
    speech_interface->speech_feed_tts = speech_feed_tts;
    speech_interface->speech_read_tts = speech_read_tts;
    speech_interface->speech_flush_tts = speech_flush_tts;

    speech_interface->speech_text_param_tts = speech_text_param_tts;
    speech_interface->speech_float_param_tts = speech_float_param_tts;
    speech_interface->speech_numeric_param_tts = speech_numeric_param_tts;

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "PiperTTS (%s)\n", MOD_VERSION);
out:
    if(xml) {
        switch_xml_free(xml);
    }
    if(status != SWITCH_STATUS_SUCCESS) {
        if(globals.models) {
            switch_core_hash_destroy(&globals.models);
        }
    }
    return status;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_piper_tts_shutdown) {

    if(globals.models) {
        switch_core_hash_destroy(&globals.models);
    }

    return SWITCH_STATUS_SUCCESS;
}
