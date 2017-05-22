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
void command_connect(blade_handle_t *bh, char *args);
//void command_chat(blade_handle_t *bh, char *args);

static const struct command_def_s command_defs[] = {
	{ "quit", command_quit },
	{ "connect", command_connect },
//	{ "chat", command_chat },

	{ NULL, NULL }
};

//ks_bool_t on_blade_chat_join_response(blade_response_t *bres);
//ks_bool_t on_blade_chat_message_event(blade_event_t *bev);
//void on_blade_session_state_callback(blade_session_t *bs, blade_session_state_condition_t condition, void *data);

int main(int argc, char **argv)
{
	blade_handle_t *bh = NULL;
	config_t config;
	config_setting_t *config_blade = NULL;
	const char *cfgpath = "bladec.cfg";
	const char *session_state_callback_id = NULL;
	const char *autoconnect = NULL;

	ks_global_set_default_logger(KS_LOG_LEVEL_DEBUG);

	blade_init();

	blade_handle_create(&bh);

	//if (argc > 1) cfgpath = argv[1];
	if (argc > 1) autoconnect = argv[1];

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

	if (blade_handle_startup(bh, config_blade) != KS_STATUS_SUCCESS) {
		ks_log(KS_LOG_ERROR, "Blade startup failed\n");
		return EXIT_FAILURE;
	}

	//blade_handle_event_register(bh, "blade.chat.message", on_blade_chat_message_event);
	//blade_handle_session_state_callback_register(bh, NULL, on_blade_session_state_callback, &session_state_callback_id);

	if (autoconnect) {
		blade_connection_t *bc = NULL;
		blade_identity_t *target = NULL;

		blade_identity_create(&target, blade_handle_pool_get(bh));

		if (blade_identity_parse(target, autoconnect) == KS_STATUS_SUCCESS) blade_handle_connect(bh, &bc, target, NULL);

		blade_identity_destroy(&target);

		ks_sleep_ms(5000);
	} else loop(bh);

	//blade_handle_session_state_callback_unregister(bh, session_state_callback_id);

	blade_handle_destroy(&bh);

	config_destroy(&config);

	blade_shutdown();

	return 0;
}

//ks_bool_t on_blade_chat_message_event(blade_event_t *bev)
//{
//	cJSON *res = NULL;
//	const char *from = NULL;
//	const char *message = NULL;
//
//	ks_assert(bev);
//
//	res = cJSON_GetObjectItem(bev->message, "result");
//	from = cJSON_GetObjectCstr(res, "from");
//	message = cJSON_GetObjectCstr(res, "message");
//
//	ks_log(KS_LOG_DEBUG, "Received Chat Message Event: (%s) %s\n", from, message);
//
//	return KS_FALSE;
//}
//
//void on_blade_session_state_callback(blade_session_t *bs, blade_session_state_condition_t condition, void *data)
//{
//	blade_session_state_t state = blade_session_state_get(bs);
//
//	if (condition == BLADE_SESSION_STATE_CONDITION_PRE) {
//		ks_log(KS_LOG_DEBUG, "Blade Session State Changed: %s, %d\n", blade_session_id_get(bs), state);
//		if (state == BLADE_SESSION_STATE_READY) {
//			cJSON *req = NULL;
//			blade_jsonrpc_request_raw_create(blade_handle_pool_get(blade_session_handle_get(bs)), &req, NULL, NULL, "blade.chat.join");
//			blade_session_send(bs, req, on_blade_chat_join_response);
//			cJSON_Delete(req);
//		}
//	}
//}

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
	//ks_assert(bh);
	ks_assert(args);

	ks_log(KS_LOG_DEBUG, "Shutting down\n");
	g_shutdown = KS_TRUE;
}

void command_connect(blade_handle_t *bh, char *args)
{
	blade_connection_t *bc = NULL;
	blade_identity_t *target = NULL;

	ks_assert(bh);
	ks_assert(args);

	blade_identity_create(&target, blade_handle_pool_get(bh));

	if (blade_identity_parse(target, args) == KS_STATUS_SUCCESS) blade_handle_connect(bh, &bc, target, NULL);

	blade_identity_destroy(&target);
}

//ks_bool_t on_blade_chat_send_response(blade_response_t *bres);
//
//ks_bool_t on_blade_chat_join_response(blade_response_t *bres) // @todo this should get userdata passed in from when the callback is registered
//{
//	blade_session_t *bs = NULL;
//	cJSON *req = NULL;
//	cJSON *params = NULL;
//
//	ks_log(KS_LOG_DEBUG, "Received Chat Join Response!\n");
//
//	bs = blade_handle_sessions_get(bres->handle, bres->session_id);
//	if (!bs) {
//		ks_log(KS_LOG_DEBUG, "Unknown Session: %s\n", bres->session_id);
//		return KS_FALSE;
//	}
//
//	blade_jsonrpc_request_raw_create(blade_handle_pool_get(bres->handle), &req, &params, NULL, "blade.chat.send");
//	ks_assert(req);
//	ks_assert(params);
//
//	cJSON_AddStringToObject(params, "message", "Hello World!");
//
//	blade_session_send(bs, req, on_blade_chat_send_response);
//
//	blade_session_read_unlock(bs);
//
//	return KS_FALSE;
//}
//
//ks_bool_t on_blade_chat_send_response(blade_response_t *bres) // @todo this should get userdata passed in from when the callback is registered
//{
//	ks_log(KS_LOG_DEBUG, "Received Chat Send Response!\n");
//	return KS_FALSE;
//}
//
//void command_chat(blade_handle_t *bh, char *args)
//{
//	char *cmd = NULL;
//
//	ks_assert(bh);
//	ks_assert(args);
//
//	parse_argument(&args, &cmd, ' ');
//	ks_log(KS_LOG_DEBUG, "Chat Command: %s, Args: %s\n", cmd, args);
//
//	if (!strcmp(cmd, "leave")) {
//	} else if (!strcmp(cmd, "send")) {
//		char *sid = NULL;
//		blade_session_t *bs = NULL;
//		cJSON *req = NULL;
//		cJSON *params = NULL;
//
//		parse_argument(&args, &sid, ' ');
//
//		bs = blade_handle_sessions_get(bh, sid);
//		if (!bs) {
//			ks_log(KS_LOG_DEBUG, "Unknown Session: %s\n", sid);
//			return;
//		}
//		blade_jsonrpc_request_raw_create(blade_handle_pool_get(bh), &req, &params, NULL, "blade.chat.send");
//		ks_assert(req);
//		ks_assert(params);
//
//		cJSON_AddStringToObject(params, "message", args);
//
//		blade_session_send(bs, req, on_blade_chat_send_response);
//
//		blade_session_read_unlock(bs);
//
//		cJSON_Delete(req);
//	} else {
//		ks_log(KS_LOG_DEBUG, "Unknown Chat Command: %s\n", cmd);
//	}
//}
