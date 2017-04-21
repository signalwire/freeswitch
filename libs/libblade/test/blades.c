#include "blade.h"
#include "tap.h"

#define CONSOLE_INPUT_MAX 512

ks_bool_t g_shutdown = KS_FALSE;

void loop(blade_handle_t *bh);
void process_console_input(blade_handle_t *bh, char *line);

typedef void (*command_callback)(blade_handle_t *bh, char *args);

struct command_def_s {
	const char *cmd;
	command_callback callback;
};

void command_quit(blade_handle_t *bh, char *args);

static const struct command_def_s command_defs[] = {
	{ "quit", command_quit },

	{ NULL, NULL }
};



ks_status_t blade_module_chat_create(blade_module_t **bmP, blade_handle_t *bh);
ks_status_t blade_module_chat_on_startup(blade_module_t *bm, config_setting_t *config);
ks_status_t blade_module_chat_on_shutdown(blade_module_t *bm);

typedef struct blade_module_chat_s blade_module_chat_t;
struct blade_module_chat_s {
	blade_handle_t *handle;
	ks_pool_t *pool;
	ks_thread_pool_t *tpool;
	blade_module_t *module;
	//blade_module_callbacks_t *module_callbacks;

	blade_space_t *blade_chat_space;
	const char *session_state_callback_id;
	ks_list_t *participants;
};

void blade_module_chat_on_session_state(blade_session_t *bs, blade_session_state_condition_t condition, void *data);

ks_bool_t blade_chat_join_request_handler(blade_module_t *bm, blade_request_t *breq);
ks_bool_t blade_chat_leave_request_handler(blade_module_t *bm, blade_request_t *breq);
ks_bool_t blade_chat_send_request_handler(blade_module_t *bm, blade_request_t *breq);

static blade_module_callbacks_t g_module_chat_callbacks =
{
	blade_module_chat_on_startup,
	blade_module_chat_on_shutdown,
};


int main(int argc, char **argv)
{
	blade_handle_t *bh = NULL;
	config_t config;
	config_setting_t *config_blade = NULL;
	blade_module_t *mod_chat = NULL;
	//blade_identity_t *id = NULL;
	const char *cfgpath = "blades.cfg";

	ks_global_set_default_logger(KS_LOG_LEVEL_DEBUG);

	blade_init();

	blade_handle_create(&bh);

	//if (argc > 1) cfgpath = argv[1];

	config_init(&config);
	if (!config_read_file(&config, cfgpath)) {
		ks_log(KS_LOG_ERROR, "%s:%d - %s\n", config_error_file(&config), config_error_line(&config), config_error_text(&config));
		config_destroy(&config);
		return EXIT_FAILURE;
	}
	config_blade = config_lookup(&config, "blade");
	if (!config_blade) {
		ks_log(KS_LOG_ERROR, "Missing 'blade' config group\n");
		config_destroy(&config);
		return EXIT_FAILURE;
	}
	if (config_setting_type(config_blade) != CONFIG_TYPE_GROUP) {
		ks_log(KS_LOG_ERROR, "The 'blade' config setting is not a group\n");
		return EXIT_FAILURE;
	}

	// must occur before startup
	blade_module_chat_create(&mod_chat, bh);
	blade_handle_module_register(mod_chat);

	if (blade_handle_startup(bh, config_blade) != KS_STATUS_SUCCESS) {
		ks_log(KS_LOG_ERROR, "Blade startup failed\n");
		return EXIT_FAILURE;
	}

	loop(bh);

	blade_handle_destroy(&bh);

	config_destroy(&config);

	blade_shutdown();

	return 0;
}

void loop(blade_handle_t *bh)
{
	char buf[CONSOLE_INPUT_MAX];
	while (!g_shutdown) {
		if (!fgets(buf, CONSOLE_INPUT_MAX, stdin)) break;

		for (int index = 0; buf[index]; ++index) {
			if (buf[index] == '\r' || buf[index] == '\n') {
				buf[index] = '\0';
				break;
			}
		}
		process_console_input(bh, buf);
	}
}

void parse_argument(char **input, char **arg, char terminator)
{
	char *tmp;

	ks_assert(input);
	ks_assert(*input);
	ks_assert(arg);

	tmp = *input;
	*arg = tmp;

	while (*tmp && *tmp != terminator) ++tmp;
	if (*tmp == terminator) {
		*tmp = '\0';
		++tmp;
	}
	*input = tmp;
}

void process_console_input(blade_handle_t *bh, char *line)
{
	char *args = line;
	char *cmd = NULL;
	ks_bool_t found = KS_FALSE;

	ks_log(KS_LOG_DEBUG, "Output: %s\n", line);

	parse_argument(&args, &cmd, ' ');

	ks_log(KS_LOG_DEBUG, "Command: %s, Args: %s\n", cmd, args);

	for (int32_t index = 0; command_defs[index].cmd; ++index) {
		if (!strcmp(command_defs[index].cmd, cmd)) {
			found = KS_TRUE;
			command_defs[index].callback(bh, args);
		}
	}
	if (!found) ks_log(KS_LOG_INFO, "Command '%s' unknown.\n", cmd);
}

void command_quit(blade_handle_t *bh, char *args)
{
	ks_assert(bh);
	ks_assert(args);

	ks_log(KS_LOG_DEBUG, "Shutting down\n");
	g_shutdown = KS_TRUE;
}




static void blade_module_chat_cleanup(ks_pool_t *pool, void *ptr, void *arg, ks_pool_cleanup_action_t action, ks_pool_cleanup_type_t type)
{
	blade_module_chat_t *bm_chat = (blade_module_chat_t *)ptr;

	ks_assert(bm_chat);

	switch (action) {
	case KS_MPCL_ANNOUNCE:
		break;
	case KS_MPCL_TEARDOWN:
		break;
	case KS_MPCL_DESTROY:
		break;
	}
}


ks_status_t blade_module_chat_create(blade_module_t **bmP, blade_handle_t *bh)
{
	blade_module_chat_t *bm_chat = NULL;
	ks_pool_t *pool = NULL;

	ks_assert(bmP);
	ks_assert(bh);

	ks_pool_open(&pool);
	ks_assert(pool);

	bm_chat = ks_pool_alloc(pool, sizeof(blade_module_chat_t));
	bm_chat->handle = bh;
	bm_chat->pool = pool;
	bm_chat->tpool = blade_handle_tpool_get(bh);
	bm_chat->session_state_callback_id = NULL;

	ks_list_create(&bm_chat->participants, pool);
	ks_assert(bm_chat->participants);

	blade_module_create(&bm_chat->module, bh, pool, bm_chat, &g_module_chat_callbacks);

	ks_assert(ks_pool_set_cleanup(pool, bm_chat, NULL, blade_module_chat_cleanup) == KS_STATUS_SUCCESS);

	ks_log(KS_LOG_DEBUG, "Created\n");

	*bmP = bm_chat->module;

	return KS_STATUS_SUCCESS;
}


ks_status_t blade_module_chat_config(blade_module_chat_t *bm_chat, config_setting_t *config)
{
	config_setting_t *chat = NULL;

	ks_assert(bm_chat);
	ks_assert(config);

	if (!config_setting_is_group(config)) {
		ks_log(KS_LOG_DEBUG, "!config_setting_is_group(config)\n");
		return KS_STATUS_FAIL;
	}

	chat = config_setting_get_member(config, "chat");
	if (chat) {
	}


	// Configuration is valid, now assign it to the variables that are used
	// If the configuration was invalid, then this does not get changed

	ks_log(KS_LOG_DEBUG, "Configured\n");

	return KS_STATUS_SUCCESS;
}

ks_status_t blade_module_chat_on_startup(blade_module_t *bm, config_setting_t *config)
{
	blade_module_chat_t *bm_chat = NULL;
	blade_space_t *space = NULL;
	blade_method_t *method = NULL;

	ks_assert(bm);
	ks_assert(config);

	bm_chat = (blade_module_chat_t *)blade_module_data_get(bm);

	if (blade_module_chat_config(bm_chat, config) != KS_STATUS_SUCCESS) {
		ks_log(KS_LOG_DEBUG, "blade_module_chat_config failed\n");
		return KS_STATUS_FAIL;
	}

	blade_space_create(&space, bm_chat->handle, bm, "blade.chat");
	ks_assert(space);

	bm_chat->blade_chat_space = space;

	blade_method_create(&method, space, "join", blade_chat_join_request_handler);
	ks_assert(method);
	blade_space_methods_add(space, method);

	blade_method_create(&method, space, "leave", blade_chat_leave_request_handler);
	ks_assert(method);
	blade_space_methods_add(space, method);

	blade_method_create(&method, space, "send", blade_chat_send_request_handler);
	ks_assert(method);
	blade_space_methods_add(space, method);

	blade_handle_space_register(space);

	blade_handle_session_state_callback_register(blade_module_handle_get(bm), bm, blade_module_chat_on_session_state, &bm_chat->session_state_callback_id);

	ks_log(KS_LOG_DEBUG, "Started\n");

	return KS_STATUS_SUCCESS;
}

ks_status_t blade_module_chat_on_shutdown(blade_module_t *bm)
{
	blade_module_chat_t *bm_chat = NULL;

	ks_assert(bm);

	bm_chat = (blade_module_chat_t *)blade_module_data_get(bm);
	ks_assert(bm_chat);

	if (bm_chat->session_state_callback_id)	blade_handle_session_state_callback_unregister(blade_module_handle_get(bm), bm_chat->session_state_callback_id);
	bm_chat->session_state_callback_id = NULL;

	if (bm_chat->blade_chat_space) blade_handle_space_unregister(bm_chat->blade_chat_space);

	ks_log(KS_LOG_DEBUG, "Stopped\n");

	return KS_STATUS_SUCCESS;
}

void blade_module_chat_on_session_state(blade_session_t *bs, blade_session_state_condition_t condition, void *data)
{
	blade_module_t *bm = NULL;
	blade_module_chat_t *bm_chat = NULL;

	ks_assert(bs);
	ks_assert(data);

	bm = (blade_module_t *)data;
	bm_chat = (blade_module_chat_t *)blade_module_data_get(bm);
	ks_assert(bm_chat);

	if (blade_session_state_get(bs) == BLADE_SESSION_STATE_HANGUP && condition == BLADE_SESSION_STATE_CONDITION_PRE) {
		cJSON *props = NULL;

		ks_log(KS_LOG_DEBUG, "Removing session from chat participants if present\n");

		props = blade_session_properties_get(bs);
		ks_assert(props);

		cJSON_DeleteItemFromObject(props, "blade.chat.participant");

		ks_list_delete(bm_chat->participants, blade_session_id_get(bs)); // @todo make copy of session id instead and search manually, also free the id
	}
}

ks_bool_t blade_chat_join_request_handler(blade_module_t *bm, blade_request_t *breq)
{
	blade_module_chat_t *bm_chat = NULL;
	blade_session_t *bs = NULL;
	cJSON *res = NULL;
	cJSON *props = NULL;
	cJSON *props_participant = NULL;

	ks_assert(bm);
	ks_assert(breq);

	ks_log(KS_LOG_DEBUG, "Request Received!\n");

	bm_chat = (blade_module_chat_t *)blade_module_data_get(bm);
	ks_assert(bm_chat);

	bs = blade_handle_sessions_get(breq->handle, breq->session_id);
	ks_assert(bs);

	// @todo properties only used to demonstrate a flexible container for session data, should just rely on the participants list/hash
	blade_session_properties_write_lock(bs, KS_TRUE);

	props = blade_session_properties_get(bs);
	ks_assert(props);

	props_participant = cJSON_GetObjectItem(props, "blade.chat.participant");
	if (props_participant && props_participant->type == cJSON_True) {
		ks_log(KS_LOG_DEBUG, "Session (%s) attempted to join chat but is already a participant\n", blade_session_id_get(bs));
		blade_rpc_error_create(&res, NULL, breq->message_id, -10000, "Already a participant of chat");
	}
	else {
		ks_log(KS_LOG_DEBUG, "Session (%s) joined chat\n", blade_session_id_get(bs));

		if (props_participant) props_participant->type = cJSON_True;
		else cJSON_AddTrueToObject(props, "blade.chat.participant");

		ks_list_append(bm_chat->participants, blade_session_id_get(bs)); // @todo make copy of session id instead and cleanup when removed

		blade_rpc_response_create(&res, NULL, breq->message_id);

		// @todo create an event to send to participants when a session joins and leaves, send after main response though
	}

	blade_session_properties_write_unlock(bs);

	blade_session_send(bs, res, NULL);

	blade_session_read_unlock(bs);

	cJSON_Delete(res);

	return KS_FALSE;
}

ks_bool_t blade_chat_leave_request_handler(blade_module_t *bm, blade_request_t *breq)
{
	blade_module_chat_t *bm_chat = NULL;
	blade_session_t *bs = NULL;
	cJSON *res = NULL;
	cJSON *props = NULL;
	cJSON *props_participant = NULL;

	ks_assert(bm);
	ks_assert(breq);

	ks_log(KS_LOG_DEBUG, "Request Received!\n");

	bm_chat = (blade_module_chat_t *)blade_module_data_get(bm);
	ks_assert(bm_chat);

	bs = blade_handle_sessions_get(breq->handle, breq->session_id);
	ks_assert(bs);

	blade_session_properties_write_lock(bs, KS_TRUE);

	props = blade_session_properties_get(bs);
	ks_assert(props);

	props_participant = cJSON_GetObjectItem(props, "blade.chat.participant");
	if (!props_participant || props_participant->type == cJSON_False) {
		ks_log(KS_LOG_DEBUG, "Session (%s) attempted to leave chat but is not a participant\n", blade_session_id_get(bs));
		blade_rpc_error_create(&res, NULL, breq->message_id, -10000, "Not a participant of chat");
	}
	else {
		ks_log(KS_LOG_DEBUG, "Session (%s) left chat\n", blade_session_id_get(bs));

		cJSON_DeleteItemFromObject(props, "blade.chat.participant");

		ks_list_delete(bm_chat->participants, blade_session_id_get(bs)); // @todo make copy of session id instead and search manually, also free the id

		blade_rpc_response_create(&res, NULL, breq->message_id);

		// @todo create an event to send to participants when a session joins and leaves, send after main response though
	}

	blade_session_properties_write_unlock(bs);

	blade_session_send(bs, res, NULL);

	blade_session_read_unlock(bs);

	cJSON_Delete(res);

	return KS_FALSE;
}

ks_bool_t blade_chat_send_request_handler(blade_module_t *bm, blade_request_t *breq)
{
	blade_module_chat_t *bm_chat = NULL;
	blade_session_t *bs = NULL;
	cJSON *params = NULL;
	cJSON *res = NULL;
	cJSON *event = NULL;
	const char *message = NULL;
	ks_bool_t sendevent = KS_FALSE;

	ks_assert(bm);
	ks_assert(breq);

	ks_log(KS_LOG_DEBUG, "Request Received!\n");

	bm_chat = (blade_module_chat_t *)blade_module_data_get(bm);
	ks_assert(bm_chat);

	params = cJSON_GetObjectItem(breq->message, "params"); // @todo cache this in blade_request_t for quicker/easier access
	if (!params) {
		ks_log(KS_LOG_DEBUG, "Session (%s) attempted to send chat message with no 'params' object\n", blade_session_id_get(bs));
		blade_rpc_error_create(&res, NULL, breq->message_id, -32602, "Missing params object");
	}
	else if (!(message = cJSON_GetObjectCstr(params, "message"))) {
		ks_log(KS_LOG_DEBUG, "Session (%s) attempted to send chat message with no 'message'\n", blade_session_id_get(bs));
		blade_rpc_error_create(&res, NULL, breq->message_id, -32602, "Missing params message string");
	}

	bs = blade_handle_sessions_get(breq->handle, breq->session_id);
	ks_assert(bs);

	if (!res) {
		blade_rpc_response_create(&res, NULL, breq->message_id);
		sendevent = KS_TRUE;
	}
	blade_session_send(bs, res, NULL);

	blade_session_read_unlock(bs);

	cJSON_Delete(res);

	if (sendevent) {
		blade_rpc_event_create(&event, &res, "blade.chat.message");
		ks_assert(event);
		cJSON_AddStringToObject(res, "from", breq->session_id); // @todo should really be the identity, but we don't have that in place yet
		cJSON_AddStringToObject(res, "message", message);

		blade_handle_sessions_send(breq->handle, bm_chat->participants, NULL, event);

		cJSON_Delete(event);
	}

	return KS_FALSE;
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
