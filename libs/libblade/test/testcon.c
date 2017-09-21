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
void command_presence(blade_handle_t *bh, char *args);
void command_identity(blade_handle_t *bh, char *args);

static const struct command_def_s command_defs[] = {
	{ "quit", command_quit },
	{ "presence", command_presence },
	{ "identity", command_identity },

	{ NULL, NULL }
};

struct testproto_s {
	blade_handle_t *handle;
	ks_pool_t *pool;
	ks_hash_t *participants;
	ks_hash_t *channels;
};
typedef struct testproto_s testproto_t;

testproto_t *g_test = NULL;

static void testproto_cleanup(void *ptr, void *arg, ks_pool_cleanup_action_t action, ks_pool_cleanup_type_t type)
{
	//testproto_t *test = (testproto_t *)ptr;

	//ks_assert(test);

	switch (action) {
	case KS_MPCL_ANNOUNCE:
		break;
	case KS_MPCL_TEARDOWN:
		break;
	case KS_MPCL_DESTROY:
		break;
	}
}

ks_status_t testproto_create(testproto_t **testP, blade_handle_t *bh)
{
	testproto_t *test = NULL;
	ks_pool_t *pool = NULL;

	ks_assert(testP);
	ks_assert(bh);

	ks_pool_open(&pool);
	ks_assert(pool);

	test = ks_pool_alloc(pool, sizeof(testproto_t));
	test->handle = bh;
	test->pool = pool;

	ks_hash_create(&test->participants, KS_HASH_MODE_CASE_INSENSITIVE, KS_HASH_FLAG_RWLOCK | KS_HASH_FLAG_DUP_CHECK | KS_HASH_FLAG_FREE_KEY, pool);
	ks_hash_create(&test->channels, KS_HASH_MODE_CASE_INSENSITIVE, KS_HASH_FLAG_RWLOCK | KS_HASH_FLAG_DUP_CHECK | KS_HASH_FLAG_FREE_KEY, pool);

	ks_pool_set_cleanup(test, NULL, testproto_cleanup);

	*testP = test;

	return KS_STATUS_SUCCESS;
}

ks_status_t testproto_destroy(testproto_t **testP)
{
	testproto_t *test = NULL;
	ks_pool_t *pool = NULL;

	ks_assert(testP);
	ks_assert(*testP);

	test = *testP;
	pool = test->pool;

	ks_pool_close(&pool);

	*testP = NULL;

	return KS_STATUS_SUCCESS;
}

ks_bool_t test_publish_response_handler(blade_rpc_response_t *brpcres, void *data)
{
	//testproto_t *test = NULL;
	blade_handle_t *bh = NULL;
	blade_session_t *bs = NULL;

	ks_assert(brpcres);
	ks_assert(data);

	//test = (testproto_t *)data;

	bh = blade_rpc_response_handle_get(brpcres);
	ks_assert(bh);

	bs = blade_sessionmgr_session_lookup(blade_handle_sessionmgr_get(bh), blade_rpc_response_sessionid_get(brpcres));
	ks_assert(bs);

	ks_log(KS_LOG_DEBUG, "Session (%s) publish response processing\n", blade_session_id_get(bs));

	blade_session_read_unlock(bs);

	return KS_FALSE;
}

ks_bool_t test_join_request_handler(blade_rpc_request_t *brpcreq, void *data)
{
	testproto_t *test = NULL;
	blade_handle_t *bh = NULL;
	blade_session_t *bs = NULL;
	const char *requester_nodeid = NULL;
	const char *key = NULL;
	cJSON *params = NULL;
	cJSON *channels = NULL;
	cJSON *result = NULL;

	ks_assert(brpcreq);
	ks_assert(data);

	test = (testproto_t *)data;

	bh = blade_rpc_request_handle_get(brpcreq);
	ks_assert(bh);

	// session for execute response
	bs = blade_sessionmgr_session_lookup(blade_handle_sessionmgr_get(bh), blade_rpc_request_sessionid_get(brpcreq));
	ks_assert(bs);

	requester_nodeid = blade_rpcexecute_request_requester_nodeid_get(brpcreq);
	ks_assert(requester_nodeid);

	// inner rpcexecute parameters
	params = blade_rpcexecute_request_params_get(brpcreq);
	ks_assert(params);

	ks_log(KS_LOG_DEBUG, "Session (%s) test.join request processing\n", blade_session_id_get(bs));

	// add to participants
	key = ks_pstrdup(test->pool, requester_nodeid);
	ks_assert(key);

	// @todo to properly maintain protocol details tied to a specific node like this participants list requires a way to know if a specific node of interest goes offline to cleanup associated details
	// refer back to work notes on ideas about this
	ks_hash_write_lock(test->participants);
	ks_hash_insert(test->participants, (void *)key, (void *)KS_TRUE);
	ks_hash_write_unlock(test->participants);

	// authorize channels with the master for the requester
	channels = cJSON_CreateArray();
	cJSON_AddItemToArray(channels, cJSON_CreateString("channel"));
	for (ks_hash_iterator_t *it = ks_hash_first(test->channels, KS_UNLOCKED); it; it = ks_hash_next(&it)) {
		void *key = NULL;
		void *value = NULL;

		ks_hash_this(it, (const void **)&key, NULL, &value);
		cJSON_AddItemToArray(channels, cJSON_CreateString((const char *)key));
	}

	blade_handle_rpcauthorize(bh, requester_nodeid, KS_FALSE, "test", channels, NULL, NULL);

	cJSON_Delete(channels);

	// send rpcexecute response to the requester
	result = cJSON_CreateObject();

	blade_rpcexecute_response_send(brpcreq, result);

	cJSON_Delete(result);

	blade_session_read_unlock(bs);

	// broadcast to authorized nodes that have subscribed, that the requester has joined
	params = cJSON_CreateObject();

	cJSON_AddStringToObject(params, "joiner-nodeid", requester_nodeid);

	blade_handle_rpcbroadcast(bh, "test", "channel", "join", params, NULL, NULL);

	cJSON_Delete(params);

	return KS_FALSE;
}

ks_bool_t test_leave_request_handler(blade_rpc_request_t *brpcreq, void *data)
{
	testproto_t *test = NULL;
	blade_handle_t *bh = NULL;
	blade_session_t *bs = NULL;
	const char *requester_nodeid = NULL;
	//const char *key = NULL;
	cJSON *params = NULL;
	cJSON *channels = NULL;
	cJSON *result = NULL;

	ks_assert(brpcreq);
	ks_assert(data);

	test = (testproto_t *)data;

	bh = blade_rpc_request_handle_get(brpcreq);
	ks_assert(bh);

	bs = blade_sessionmgr_session_lookup(blade_handle_sessionmgr_get(bh), blade_rpc_request_sessionid_get(brpcreq));
	ks_assert(bs);

	requester_nodeid = blade_rpcexecute_request_requester_nodeid_get(brpcreq);
	ks_assert(requester_nodeid);

	params = blade_rpcexecute_request_params_get(brpcreq);
	ks_assert(params);

	ks_log(KS_LOG_DEBUG, "Session (%s) test.leave (%s) request processing\n", blade_session_id_get(bs), requester_nodeid);

	ks_hash_write_lock(test->participants);
	ks_hash_remove(test->participants, (void *)requester_nodeid);
	ks_hash_write_unlock(test->participants);

	// deauthorize channels with the master for the requester
	channels = cJSON_CreateArray();
	cJSON_AddItemToArray(channels, cJSON_CreateString("channel"));
	for (ks_hash_iterator_t *it = ks_hash_first(test->channels, KS_UNLOCKED); it; it = ks_hash_next(&it)) {
		void *key = NULL;
		void *value = NULL;

		ks_hash_this(it, (const void **)&key, NULL, &value);
		cJSON_AddItemToArray(channels, cJSON_CreateString((const char *)key));
	}

	blade_handle_rpcauthorize(bh, requester_nodeid, KS_TRUE, "test", channels, NULL, NULL);

	cJSON_Delete(channels);

	// send rpcexecute response to the requester
	result = cJSON_CreateObject();

	blade_rpcexecute_response_send(brpcreq, result);

	cJSON_Delete(result);

	blade_session_read_unlock(bs);

	// broadcast to authorized nodes that have subscribed, that the requester has left
	params = cJSON_CreateObject();

	cJSON_AddStringToObject(params, "leaver-nodeid", requester_nodeid);

	blade_handle_rpcbroadcast(bh, "test", "channel", "leave", params, NULL, NULL);

	cJSON_Delete(params);

	return KS_FALSE;
}

ks_bool_t test_talk_request_handler(blade_rpc_request_t *brpcreq, void *data)
{
	//testproto_t *test = NULL;
	blade_handle_t *bh = NULL;
	blade_session_t *bs = NULL;
	const char *requester_nodeid = NULL;
	const char *text = NULL;
	cJSON *params = NULL;
	cJSON *result = NULL;

	ks_assert(brpcreq);
	ks_assert(data);

	//test = (testproto_t *)data;

	bh = blade_rpc_request_handle_get(brpcreq);
	ks_assert(bh);

	bs = blade_sessionmgr_session_lookup(blade_handle_sessionmgr_get(bh), blade_rpc_request_sessionid_get(brpcreq));
	ks_assert(bs);

	requester_nodeid = blade_rpcexecute_request_requester_nodeid_get(brpcreq);
	ks_assert(requester_nodeid);

	params = blade_rpcexecute_request_params_get(brpcreq);
	ks_assert(params);

	text = cJSON_GetObjectCstr(params, "text");
	ks_assert(text);

	ks_log(KS_LOG_DEBUG, "Session (%s) test.talk (%s) request processing\n", blade_session_id_get(bs), requester_nodeid);

	// send rpcexecute response to the requester
	result = cJSON_CreateObject();

	blade_rpcexecute_response_send(brpcreq, result);

	cJSON_Delete(result);

	blade_session_read_unlock(bs);

	// broadcast to authorized nodes that have subscribed, that the requester has said something
	params = cJSON_CreateObject();

	cJSON_AddStringToObject(params, "text", text);

	cJSON_AddStringToObject(params, "talker-nodeid", requester_nodeid);

	blade_handle_rpcbroadcast(bh, "test", "channel", "talk", params, NULL, NULL);

	cJSON_Delete(params);

	return KS_FALSE;
}

ks_bool_t test_presence_request_handler(blade_rpc_request_t *brpcreq, void *data)
{
	blade_handle_t *bh = NULL;
	blade_session_t *bs = NULL;
	const char *protocol = NULL;
	const char *channel = NULL;
	const char *event = NULL;
	cJSON *params = NULL;
	const char *nodeid = NULL;

	ks_assert(brpcreq);
	ks_assert(data);

	bh = blade_rpc_request_handle_get(brpcreq);
	ks_assert(bh);

	bs = blade_sessionmgr_session_lookup(blade_handle_sessionmgr_get(bh), blade_rpc_request_sessionid_get(brpcreq));
	ks_assert(bs);

	protocol = blade_rpcbroadcast_request_protocol_get(brpcreq);
	channel = blade_rpcbroadcast_request_channel_get(brpcreq);
	event = blade_rpcbroadcast_request_event_get(brpcreq);

	params = blade_rpcbroadcast_request_params_get(brpcreq);
	nodeid = cJSON_GetObjectCstr(params, "nodeid");

	ks_log(KS_LOG_DEBUG, "Session (%s) presence (protocol %s, channel %s, event %s for %s) request processing\n", blade_session_id_get(bs), protocol, channel, event, nodeid);

	return KS_FALSE;
}

ks_bool_t test_register_response_handler(blade_rpc_response_t *brpcres, void *data)
{
	blade_handle_t *bh = NULL;
	blade_session_t *bs = NULL;

	ks_assert(brpcres);

	bh = blade_rpc_response_handle_get(brpcres);
	ks_assert(bh);

	bs = blade_sessionmgr_session_lookup(blade_handle_sessionmgr_get(bh), blade_rpc_response_sessionid_get(brpcres));
	ks_assert(bs);

	ks_log(KS_LOG_DEBUG, "Session (%s) register response processing\n", blade_session_id_get(bs));

	blade_session_read_unlock(bs);

	return KS_FALSE;
}


int main(int argc, char **argv)
{
	blade_handle_t *bh = NULL;
	ks_pool_t *pool = NULL;
	config_t config;
	config_setting_t *config_blade = NULL;
	const char *cfgpath = "testcon.cfg";
	const char *autoconnect = NULL;

	ks_global_set_default_logger(KS_LOG_LEVEL_DEBUG);

	blade_init();

	blade_handle_create(&bh);
	ks_assert(bh);

	pool = ks_pool_get(bh);
	ks_assert(pool);

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

	testproto_create(&g_test, bh);

	if (autoconnect) {
		blade_connection_t *bc = NULL;
		blade_identity_t *target = NULL;
		ks_bool_t connected = KS_FALSE;
		blade_rpc_t *brpc = NULL;

		blade_identity_create(&target, ks_pool_get(bh));

		if (blade_identity_parse(target, autoconnect) == KS_STATUS_SUCCESS) connected = blade_handle_connect(bh, &bc, target, NULL) == KS_STATUS_SUCCESS;

		blade_identity_destroy(&target);

		if (connected) {
			cJSON *channels = NULL;
			cJSON *entry = NULL;

			// @todo use session state change callback to know when the session is ready after blade.connect, this hack temporarily ensures it's ready before trying to publish upstream
			ks_sleep_ms(3000);

			blade_rpc_create(&brpc, bh, "test.join", "test", test_join_request_handler, (void *)g_test);
			blade_rpcmgr_protocolrpc_add(blade_handle_rpcmgr_get(bh), brpc);

			blade_rpc_create(&brpc, bh, "test.leave", "test", test_leave_request_handler, (void *)g_test);
			blade_rpcmgr_protocolrpc_add(blade_handle_rpcmgr_get(bh), brpc);

			blade_rpc_create(&brpc, bh, "test.talk", "test", test_talk_request_handler, (void *)g_test);
			blade_rpcmgr_protocolrpc_add(blade_handle_rpcmgr_get(bh), brpc);

			channels = cJSON_CreateArray();
			entry = cJSON_CreateObject();
			cJSON_AddStringToObject(entry, "name", "channel");
			cJSON_AddNumberToObject(entry, "flags", BLADE_CHANNEL_FLAGS_NONE);
			cJSON_AddItemToArray(channels, entry);

			blade_handle_rpcpublish(bh, BLADE_RPCPUBLISH_COMMAND_CONTROLLER_ADD, "test", channels, test_publish_response_handler, (void *)g_test);

			cJSON_Delete(channels);
		}
	}

	loop(bh);

	blade_handle_destroy(&bh);

	testproto_destroy(&g_test);

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

	g_shutdown = KS_TRUE;
}

void command_presence(blade_handle_t *bh, char *args)
{
	cJSON *channels = NULL;

	ks_assert(bh);
	ks_assert(args);

	channels = cJSON_CreateArray();
	cJSON_AddItemToArray(channels, cJSON_CreateString("join"));
	cJSON_AddItemToArray(channels, cJSON_CreateString("leave"));

	blade_handle_rpcsubscribe(bh, BLADE_RPCSUBSCRIBE_COMMAND_SUBSCRIBER_ADD, "blade.presence", channels, NULL, NULL, test_presence_request_handler, (void *)g_test);
}

void command_identity(blade_handle_t *bh, char *args)
{
	ks_assert(bh);
	ks_assert(args);

	blade_handle_rpcregister(bh, "blade:testcon@freeswitch.com", test_register_response_handler, NULL);
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
