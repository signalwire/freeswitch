/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2015, Anthony Minessale II <anthm@freeswitch.org>
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
 * Seven Du <dujinfang@gmail.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Seven Du <dujinfang@gmail.com>
 * Anthony Minessale <anthm@freeswitch.org>
 *
 * mod_av -- FS Video Codec / File Format using libav.org
 *
 */

#include <switch.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

SWITCH_MODULE_LOAD_FUNCTION(mod_avformat_load);
SWITCH_MODULE_LOAD_FUNCTION(mod_avcodec_load);
SWITCH_MODULE_LOAD_FUNCTION(mod_av_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_av_shutdown);
SWITCH_MODULE_DEFINITION(mod_av, mod_av_load, mod_av_shutdown, NULL);

typedef struct av_mutex_helper_s {
	switch_mutex_t *mutex;
	switch_memory_pool_t *pool;
} av_mutex_helper_t;

int mod_av_lockmgr_cb(void **m, enum AVLockOp op)
{
	av_mutex_helper_t *context = NULL;

	if (!m) return -1;

	context = (av_mutex_helper_t *)*m;

	switch(op)
		{
		case AV_LOCK_CREATE:
			{
				switch_memory_pool_t *pool;
				switch_core_new_memory_pool(&pool);
				context = switch_core_alloc(pool, sizeof(av_mutex_helper_t));
				switch_mutex_init(&context->mutex, SWITCH_MUTEX_NESTED, pool);
				context->pool = pool;
				*m = (void *)context;
				break;
			}
		case AV_LOCK_OBTAIN:
			{
				switch_mutex_t *mutex = context->mutex;
				if (!mutex) return -1;
				switch_mutex_lock(mutex);
				break;
			}
		case AV_LOCK_RELEASE:
			{
				switch_mutex_t *mutex = context->mutex;
				if (!mutex) return -1;
				switch_mutex_unlock(mutex);
				break;
			}
		case AV_LOCK_DESTROY:
			{
				switch_core_destroy_memory_pool(&context->pool);
				break;
			}
		default:
			break;
		}
	return 0;
}

static void log_callback(void *ptr, int level, const char *fmt, va_list vl)
{
	switch_log_level_t switch_level = SWITCH_LOG_DEBUG;

	/* naggy messages */
	if (level == AV_LOG_DEBUG || level == AV_LOG_WARNING) return;

	switch(level) {
		case AV_LOG_QUIET:   switch_level = SWITCH_LOG_CONSOLE; break;
		case AV_LOG_PANIC:   switch_level = SWITCH_LOG_DEBUG2;   break;
		case AV_LOG_FATAL:   switch_level = SWITCH_LOG_DEBUG2;   break;
		case AV_LOG_ERROR:   switch_level = SWITCH_LOG_DEBUG2;   break;
		case AV_LOG_WARNING: switch_level = SWITCH_LOG_WARNING; break;
		case AV_LOG_INFO:    switch_level = SWITCH_LOG_INFO;    break;
		case AV_LOG_VERBOSE: switch_level = SWITCH_LOG_INFO;    break;
		case AV_LOG_DEBUG:   switch_level = SWITCH_LOG_DEBUG;   break;
		default: break;
	}

	// switch_level = SWITCH_LOG_ERROR; // hardcoded for debug
	switch_log_vprintf(SWITCH_CHANNEL_LOG_CLEAN, switch_level, fmt, vl);
}


SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_av_shutdown)
{
	avformat_network_deinit();
	av_log_set_callback(NULL);
	av_lockmgr_register(NULL);
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_av_load)
{
	av_lockmgr_register(&mod_av_lockmgr_cb);
	av_log_set_callback(log_callback);
	av_log_set_level(AV_LOG_INFO);
	avformat_network_init();
	av_register_all();

	av_log(NULL, AV_LOG_INFO, "%s %d\n", "av_log callback installed, level=", av_log_get_level());

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	mod_avformat_load(module_interface, pool);
	mod_avcodec_load(module_interface, pool);

	return SWITCH_STATUS_SUCCESS;
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
