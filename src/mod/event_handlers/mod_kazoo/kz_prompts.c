#include "kz_prompts.h"

// return should be freed
char* kz_prompt_get(const char * path)
{
	const char *var = NULL;
	switch_xml_t xml_node = NULL;
	switch_xml_t xml_root = NULL;
	char*req_path = NULL;
	char *reply = NULL;

	req_path = switch_mprintf("prompt://%s", path);
	if (!req_path) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "failed to to create req_path\n");
		return NULL;
	}

	if (switch_xml_locate("configuration", "prompt", "path", req_path, &xml_root, &xml_node, NULL, SWITCH_FALSE) != SWITCH_STATUS_SUCCESS) {
		switch_safe_free(req_path);
		return NULL;
	}

	var = switch_xml_attr(xml_node, "value");
	if (!zstr(var)) {
		if (strncasecmp(var, "prompt://", 9)) {
			reply = switch_mprintf("http_cache://%s", var);
		}
	}

	switch_safe_free(req_path);
	switch_xml_free(xml_root);

	return reply;
}

#define KZ_PROMPT_SYNTAX "path"
SWITCH_STANDARD_API(kz_prompt_api_get)
{

	char *reply = NULL;

	if (!cmd) {
		return SWITCH_STATUS_GENERR;
	}

	reply = kz_prompt_get(cmd);

	if (reply) {
		stream->write_function(stream, "%s", reply);
	} else {
		stream->write_function(stream, "not_found");
	}

	switch_safe_free(reply);

	return SWITCH_STATUS_SUCCESS;

}

static switch_status_t kz_prompt_file_open(switch_file_handle_t *handle, const char *path)
{
	switch_status_t ret;
	switch_file_handle_t* context = NULL;
	int file_flags = SWITCH_FILE_DATA_SHORT | (switch_test_flag(handle, SWITCH_FILE_FLAG_VIDEO) ? SWITCH_FILE_FLAG_VIDEO : 0);
	char * real_path = kz_prompt_get(path);
	if (real_path == NULL) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "failed to open %s\n", path);
		return SWITCH_STATUS_NOTFOUND;
	}

	if ((context = switch_core_alloc(handle->memory_pool, sizeof(*context))) == NULL) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "failed to allocate memory for %s => %s\n", path, real_path);
		switch_safe_free(real_path);
		return SWITCH_STATUS_MEMERR;
	}

	if (switch_test_flag(handle, SWITCH_FILE_FLAG_WRITE)) {
		file_flags |= SWITCH_FILE_FLAG_WRITE;
	} else {
		file_flags |= SWITCH_FILE_FLAG_READ;
	}

	context->pre_buffer_datalen = handle->pre_buffer_datalen;

	ret = switch_core_file_open(context, real_path, handle->channels, handle->samplerate, file_flags, handle->memory_pool);
	if (ret != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "failed to open %s => %s\n", path, real_path);
		switch_safe_free(real_path);
		return ret;
	}

	handle->private_info = context;
	handle->samples = context->samples;
	handle->format = context->format;
	handle->sections = context->sections;
	handle->seekable = context->seekable;
	handle->speed = context->speed;
	handle->interval = context->interval;
	handle->channels = context->channels;
	handle->cur_channels = context->real_channels;
	handle->flags |= SWITCH_FILE_NOMUX;
	handle->pre_buffer_datalen = 0;

	if (switch_test_flag(context, SWITCH_FILE_NATIVE)) {
		switch_set_flag_locked(handle, SWITCH_FILE_NATIVE);
	} else {
		switch_clear_flag_locked(handle, SWITCH_FILE_NATIVE);
	}

	if (switch_test_flag(context, SWITCH_FILE_FLAG_VIDEO)) {
		switch_set_flag_locked(handle, SWITCH_FILE_FLAG_VIDEO);
	} else {
		switch_clear_flag_locked(handle, SWITCH_FILE_FLAG_VIDEO);
	}

	switch_safe_free(real_path);

	return ret;
}

static switch_status_t kz_prompt_file_read(switch_file_handle_t *handle, void *data, size_t *len)
{
	switch_file_handle_t *context = (switch_file_handle_t *)handle->private_info;
	if (context == NULL) {
		return SWITCH_STATUS_GENERR;
	}
	return switch_core_file_read(context, data, len);
}

static switch_status_t kz_prompt_file_read_video(switch_file_handle_t *handle, switch_frame_t *frame, switch_video_read_flag_t flags)
{
	switch_file_handle_t *context = (switch_file_handle_t *)handle->private_info;
	if (context == NULL) {
		return SWITCH_STATUS_GENERR;
	}
	return switch_core_file_read_video(context, frame, flags);
}

static switch_status_t kz_prompt_file_close(switch_file_handle_t *handle)
{
	switch_file_handle_t *context = (switch_file_handle_t *)handle->private_info;
	if (context == NULL) {
		return SWITCH_STATUS_GENERR;
	}
	return switch_core_file_close(context);
}

static switch_status_t kz_prompt_file_seek(switch_file_handle_t *handle, unsigned int *cur_sample, int64_t samples, int whence)
{
	switch_file_handle_t *context = (switch_file_handle_t *)handle->private_info;
	if (context == NULL) {
		return SWITCH_STATUS_GENERR;
	}
	return switch_core_file_seek(context, cur_sample, samples, whence);
}

static switch_status_t kz_prompt_file_write(switch_file_handle_t *handle, void *data, size_t *len)
{
	switch_file_handle_t *context = (switch_file_handle_t *)handle->private_info;
	if (context == NULL) {
		return SWITCH_STATUS_GENERR;
	}
	return switch_core_file_write(context, data, len);
}

static switch_status_t kz_prompt_file_write_video(switch_file_handle_t *handle, switch_frame_t *frame)
{
	switch_file_handle_t *context = (switch_file_handle_t *)handle->private_info;
	if (context == NULL) {
		return SWITCH_STATUS_GENERR;
	}
	return switch_core_file_write_video(context, frame);
}

static char *kz_prompt_supported_formats[] = { "prompt", NULL };

void kz_prompts_init(switch_loadable_module_interface_t **module_interface)
{
	switch_file_interface_t *file_interface;
	switch_api_interface_t *api;

	SWITCH_ADD_API(api, "kz_prompt", "Translates a prompt", kz_prompt_api_get, KZ_PROMPT_SYNTAX);

	file_interface = switch_loadable_module_create_interface(*module_interface, SWITCH_FILE_INTERFACE);
	file_interface->interface_name = MODNAME;
	file_interface->extens = kz_prompt_supported_formats;
	file_interface->file_open = kz_prompt_file_open;
	file_interface->file_close = kz_prompt_file_close;
	file_interface->file_read = kz_prompt_file_read;
	file_interface->file_read_video = kz_prompt_file_read_video;
    file_interface->file_seek = kz_prompt_file_seek;
	file_interface->file_write = kz_prompt_file_write;
	file_interface->file_write_video = kz_prompt_file_write_video;
}


void kz_prompts_destroy()
{
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
