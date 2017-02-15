#include "blade.h"
#include "tap.h"

#ifdef _WIN32
#define STDIO_FD(_fs) _fileno(_fs)
#define READ(_fd, _buffer, _count) _read(_fd, _buffer, _count)
#else
#define STDIO_FD(_fs) fileno(_fs)
#define READ(_fd, _buffer, _count) read(_fd, _buffer, _count)
#endif

#define CONSOLE_INPUT_MAX 512

ks_bool_t g_shutdown = KS_FALSE;
char g_console_input[CONSOLE_INPUT_MAX];
size_t g_console_input_length = 0;
size_t g_console_input_eol = 0;

void loop(blade_handle_t *bh);
void process_console_input(blade_handle_t *bh, char *line);

typedef void (*command_callback)(blade_handle_t *bh, char *args);

struct command_def_s {
	const char *cmd;
	command_callback callback;
};

void command_test(blade_handle_t *bh, char *args);
void command_quit(blade_handle_t *bh, char *args);
void command_store(blade_handle_t *bh, char *args);
void command_fetch(blade_handle_t *bh, char *args);
void command_connect(blade_handle_t *bh, char *args);

static const struct command_def_s command_defs[] = {
	{ "test", command_test },
	{ "quit", command_quit },
	{ "store", command_store },
	{ "fetch", command_fetch },
	{ "connect", command_connect },
	
	{ NULL, NULL }
};

int main(int argc, char **argv)
{
	blade_handle_t *bh = NULL;
	config_t config;
	config_setting_t *config_blade = NULL;
	blade_module_t *mod_wss = NULL;
	//blade_identity_t *id = NULL;
	const char *cfgpath = "bladec.cfg";
	

	ks_global_set_default_logger(KS_LOG_LEVEL_DEBUG);
	
	blade_init();

	blade_handle_create(&bh, NULL, NULL);

	if (argc > 1) cfgpath = argv[1];
	
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

	if (blade_module_wss_on_load(&mod_wss, bh) != KS_STATUS_SUCCESS) {
		ks_log(KS_LOG_ERROR, "Blade WSS module load failed\n");
		return EXIT_FAILURE;
	}
	if (blade_module_wss_on_startup(mod_wss, config_blade) != KS_STATUS_SUCCESS) {
		ks_log(KS_LOG_ERROR, "Blade WSS module startup failed\n");
		return EXIT_FAILURE;
	}

	loop(bh);

	blade_module_wss_on_shutdown(mod_wss);

	blade_module_wss_on_unload(mod_wss);

	blade_handle_destroy(&bh);

	blade_shutdown();

	return 0;
}



void buffer_console_input(void)
{
	ssize_t bytes = 0;
	struct pollfd poll[1];
	poll[0].fd = STDIO_FD(stdin);
	poll[0].events = POLLIN | POLLERR;

	if (ks_poll(poll, 1, 1) > 0) {
		if (poll[0].revents & POLLIN) {
			if ((bytes = READ(poll[0].fd, g_console_input + g_console_input_length, CONSOLE_INPUT_MAX - g_console_input_length)) <= 0) {
				// @todo error
				return;
			}
			g_console_input_length += bytes;
		}
	}
}

void loop(blade_handle_t *bh)
{
	while (!g_shutdown) {
		ks_bool_t eol = KS_FALSE;
		buffer_console_input();

		for (; g_console_input_eol < g_console_input_length; ++g_console_input_eol) {
			char c = g_console_input[g_console_input_eol];
			if (c == '\r' || c == '\n') {
				eol = KS_TRUE;
				break;
			}
		}
		if (eol) {
			g_console_input[g_console_input_eol] = '\0';
			process_console_input(bh, g_console_input);
			g_console_input_eol++;
			for (; g_console_input_eol < g_console_input_length; ++g_console_input_eol) {
				char c = g_console_input[g_console_input_eol];
				if (c != '\r' && c != '\n') break;
			}
			if (g_console_input_eol == g_console_input_length) g_console_input_eol = g_console_input_length = 0;
			else {
				memcpy(g_console_input, g_console_input + g_console_input_eol, g_console_input_length - g_console_input_eol);
				g_console_input_length -= g_console_input_eol;
				g_console_input_eol = 0;
			}
		}
		if (g_console_input_length == CONSOLE_INPUT_MAX) {
			// @todo lines must not exceed 512 bytes, treat as error and ignore buffer until next new line?
			ks_assert(0);
		}
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

void command_test(blade_handle_t *bh, char *args)
{
	ks_log(KS_LOG_DEBUG, "Hello World!\n");
}

void command_quit(blade_handle_t *bh, char *args)
{
	ks_assert(bh);
	ks_assert(args);
	
	ks_log(KS_LOG_DEBUG, "Shutting down\n");
	g_shutdown = KS_TRUE;
}

void command_store(blade_handle_t *bh, char *args)
{
	char *key;
	char *data;

	ks_assert(args);

	parse_argument(&args, &key, ' ');
	parse_argument(&args, &data, ' ');

	blade_handle_datastore_store(bh, key, strlen(key), data, strlen(data) + 1);
}

ks_bool_t blade_datastore_fetch_callback(blade_datastore_t *bds, const void *data, uint32_t data_length, void *userdata)
{
	ks_log(KS_LOG_INFO, "%s\n", data);
	return KS_TRUE;
}

void command_fetch(blade_handle_t *bh, char *args)
{
	char *key;

	ks_assert(args);

	parse_argument(&args, &key, ' ');

	blade_handle_datastore_fetch(bh, blade_datastore_fetch_callback, key, strlen(key), bh);
}

void command_connect(blade_handle_t *bh, char *args)
{
	blade_connection_t *bc = NULL;
	blade_identity_t *target = NULL;
	
	ks_assert(bh);
	ks_assert(args);

	blade_identity_create(&target, blade_handle_pool_get(bh));
	
	if (blade_identity_parse(target, args) == KS_STATUS_SUCCESS) blade_handle_connect(bh, &bc, target);

	blade_identity_destroy(&target);
}
