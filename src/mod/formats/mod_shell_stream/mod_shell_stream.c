/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2012, Anthony Minessale II <anthm@freeswitch.org>
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
 * Anthony Minessale II <anthm@freeswitch.org>
 *
 * mod_shell_stream.c -- Local Streaming Audio
 *
 */
#include <switch.h>
#include <sys/wait.h>

#define MY_BUF_LEN 1024 * 32
#define MY_BLOCK_SIZE MY_BUF_LEN


SWITCH_MODULE_LOAD_FUNCTION(mod_shell_stream_load);

SWITCH_MODULE_DEFINITION(mod_shell_stream, mod_shell_stream_load, NULL, NULL);

struct shell_stream_context {
	int fds[2];
	int pid;
	char *command;
	switch_buffer_t *audio_buffer;
	switch_mutex_t *mutex;
	switch_thread_rwlock_t *rwlock;
	int running;
};

typedef struct shell_stream_context shell_stream_context_t;


static void *SWITCH_THREAD_FUNC buffer_thread_run(switch_thread_t *thread, void *obj)
{
	shell_stream_context_t *context = (shell_stream_context_t *) obj;
	switch_byte_t data[MY_BUF_LEN];
	ssize_t rlen;

	context->running = 1;

	if (switch_thread_rwlock_tryrdlock(context->rwlock) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Read Lock Fail\n");
		goto end;
	}

	while (context->running) {

		rlen = read(context->fds[0], data, MY_BUF_LEN);

		if (rlen <= 3) {
			break;
		}

		switch_mutex_lock(context->mutex);
		switch_buffer_write(context->audio_buffer, data, rlen);
		switch_mutex_unlock(context->mutex);
	}

	switch_thread_rwlock_unlock(context->rwlock);

  end:

	context->running = 0;

	return NULL;
}

static switch_status_t shell_stream_file_open(switch_file_handle_t *handle, const char *path)
{
	shell_stream_context_t *context;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;

	if (switch_test_flag(handle, SWITCH_FILE_FLAG_WRITE)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "This format does not support writing!\n");
		return SWITCH_STATUS_FALSE;
	}

	context = switch_core_alloc(handle->memory_pool, sizeof(*context));

	context->fds[0] = -1;
	context->fds[1] = -1;
	context->command = switch_core_sprintf(handle->memory_pool, "%s -r %d -c %d", path, handle->samplerate, handle->channels);

	if (pipe(context->fds)) {
		goto error;
	} else {					/* good to go */
		context->pid = switch_fork();

		if (context->pid < 0) {	/* ok maybe not */
			goto error;
		} else if (context->pid) {	/* parent */
			handle->private_info = context;
			status = SWITCH_STATUS_SUCCESS;
			close(context->fds[1]);
			context->fds[1] = -1;

			if (switch_buffer_create_dynamic(&context->audio_buffer, MY_BLOCK_SIZE, MY_BUF_LEN, 0) != SWITCH_STATUS_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Write Buffer Failed!\n");
				goto error;
			}

			switch_thread_rwlock_create(&context->rwlock, handle->memory_pool);
			switch_mutex_init(&context->mutex, SWITCH_MUTEX_NESTED, handle->memory_pool);

			switch_threadattr_create(&thd_attr, handle->memory_pool);
			switch_threadattr_detach_set(thd_attr, 1);
			switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
			switch_thread_create(&thread, thd_attr, buffer_thread_run, context, handle->memory_pool);
			context->running = 2;

			while (context->running == 2) {
				switch_cond_next();
			}

			wait(&(context->pid));

			goto end;
		} else {				/*  child */
			close(context->fds[0]);
			dup2(context->fds[1], STDOUT_FILENO);
			switch_system(context->command, SWITCH_TRUE);
			printf("EOF");
			close(context->fds[1]);
			exit(0);
		}
	}

  error:

	close(context->fds[0]);
	close(context->fds[1]);
	status = SWITCH_STATUS_FALSE;


  end:

	return status;
}

static switch_status_t shell_stream_file_close(switch_file_handle_t *handle)
{
	shell_stream_context_t *context = handle->private_info;

	context->running = 0;

	if (context->fds[0] > -1) {
		close(context->fds[0]);
	}

	if (context->audio_buffer) {
		switch_buffer_destroy(&context->audio_buffer);
	}

	switch_thread_rwlock_wrlock(context->rwlock);
	switch_thread_rwlock_unlock(context->rwlock);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t shell_stream_file_read(switch_file_handle_t *handle, void *data, size_t *len)
{
	shell_stream_context_t *context = handle->private_info;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_size_t rlen = *len * 2;

	while (context->running && switch_buffer_inuse(context->audio_buffer) < rlen) {
		switch_cond_next();
	}

	switch_mutex_lock(context->mutex);
	*len = switch_buffer_read(context->audio_buffer, data, rlen) / 2;
	switch_mutex_unlock(context->mutex);

	return status;
}

/* Registration */

static char *supported_formats[SWITCH_MAX_CODECS] = { 0 };

SWITCH_MODULE_LOAD_FUNCTION(mod_shell_stream_load)
{
	switch_file_interface_t *file_interface;
	supported_formats[0] = "shell_stream";

	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	file_interface = switch_loadable_module_create_interface(*module_interface, SWITCH_FILE_INTERFACE);
	file_interface->interface_name = modname;
	file_interface->extens = supported_formats;
	file_interface->file_open = shell_stream_file_open;
	file_interface->file_close = shell_stream_file_close;
	file_interface->file_read = shell_stream_file_read;

	/* indicate that the module should continue to be loaded */
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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
