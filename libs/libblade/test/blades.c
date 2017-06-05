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



ks_bool_t blade_publish_response_handler(blade_rpc_response_t *brpcres, void *data)
{
	blade_handle_t *bh = NULL;
	blade_session_t *bs = NULL;

	ks_assert(brpcres);

	bh = blade_rpc_response_handle_get(brpcres);
	ks_assert(bh);

	bs = blade_handle_sessions_lookup(bh, blade_rpc_response_sessionid_get(brpcres));
	ks_assert(bs);

	ks_log(KS_LOG_DEBUG, "Session (%s) blade.publish response processing\n", blade_session_id_get(bs));

	blade_session_read_unlock(bs);

	return KS_FALSE;
}

ks_bool_t test_echo_request_handler(blade_rpc_request_t *brpcreq, void *data)
{
	blade_handle_t *bh = NULL;
	blade_session_t *bs = NULL;
	cJSON *params = NULL;
	cJSON *result = NULL;
	const char *text = NULL;

	ks_assert(brpcreq);

	bh = blade_rpc_request_handle_get(brpcreq);
	ks_assert(bh);

	bs = blade_handle_sessions_lookup(bh, blade_rpc_request_sessionid_get(brpcreq));
	ks_assert(bs);

	// @todo get the inner parameters of a blade.execute request for protocolrpcs
	params = blade_protocol_execute_request_params_get(brpcreq);
	ks_assert(params);

	text = cJSON_GetObjectCstr(params, "text");
	ks_assert(text);

	ks_log(KS_LOG_DEBUG, "Session (%s) test.echo request processing\n", blade_session_id_get(bs));

	blade_session_read_unlock(bs);

	// @todo build and send response
	result = cJSON_CreateObject();
	cJSON_AddStringToObject(result, "text", text);

	blade_protocol_execute_response_send(brpcreq, result);

	return KS_FALSE;
}


int main(int argc, char **argv)
{
	blade_handle_t *bh = NULL;
	config_t config;
	config_setting_t *config_blade = NULL;
	const char *cfgpath = "blades.cfg";
	const char *autoconnect = NULL;

	ks_global_set_default_logger(KS_LOG_LEVEL_DEBUG);

	blade_init();

	blade_handle_create(&bh);

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

	if (autoconnect) {
		blade_connection_t *bc = NULL;
		blade_identity_t *target = NULL;
		blade_rpc_t *brpc = NULL;

		blade_identity_create(&target, blade_handle_pool_get(bh));

		if (blade_identity_parse(target, autoconnect) == KS_STATUS_SUCCESS) blade_handle_connect(bh, &bc, target, NULL);

		blade_identity_destroy(&target);

		ks_sleep_ms(5000); // @todo use session state change callback to know when the session is ready, this hack temporarily ensures it's ready before trying to publish upstream

		blade_rpc_create(&brpc, bh, "test.echo", "test", "mydomain.com", test_echo_request_handler, NULL);
		blade_handle_protocolrpc_register(brpc);

		// @todo build up json-based method schema for each protocolrpc registered above, and pass into blade_protocol_publish() to attach to the request, to be stored in the blade_protocol_t tracked by the master node
		blade_protocol_publish(bh, "test", "mydomain.com", blade_publish_response_handler, NULL);
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
