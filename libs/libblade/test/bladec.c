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
void command_myid(blade_handle_t *bh, char *args);
void command_bind(blade_handle_t *bh, char *args);

static const struct command_def_s command_defs[] = {
	{ "test", command_test },
	{ "quit", command_quit },
	{ "myid", command_myid },
	{ "bind", command_bind },
	
	{ NULL, NULL }
};


int main(int argc, char **argv)
{
	blade_handle_t *bh = NULL;
	const char *nodeid;

	ks_assert(argc >= 2);

	nodeid = argv[1];

	ks_global_set_default_logger(KS_LOG_LEVEL_DEBUG);
	
	blade_init();

	blade_handle_create(&bh, NULL, NULL, nodeid);

	blade_handle_autoroute(bh, KS_TRUE, KS_DHT_DEFAULT_PORT);

	loop(bh);
	
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
		blade_handle_pulse(bh, 1);
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

void command_myid(blade_handle_t *bh, char *args)
{
	char buf[KS_DHT_NODEID_SIZE * 2 + 1];

	ks_assert(bh);
	ks_assert(args);

	blade_handle_myid(bh, buf);

	ks_log(KS_LOG_INFO, "%s\n", buf);
}

void command_bind(blade_handle_t *bh, char *args)
{
	char *ip = NULL;
	char *port = NULL;
	ks_port_t p;

	ks_assert(args);

	parse_argument(&args, &ip, ' ');
	parse_argument(&args, &port, ' ');

	p = atoi(port); // @todo use strtol for error handling

	blade_handle_bind(bh, ip, p, NULL);
}
