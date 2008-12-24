#include <stdio.h>
#include <stdlib.h>
#include <esl.h>
#include <signal.h>

#ifdef WIN32
#define strdup(src) _strdup(src)
#define usleep(time) Sleep(time/1000)
#define fileno _fileno
#define read _read
#include <io.h>
#include <getopt.h>
#else
#include <sys/select.h>
#include <histedit.h>
#define HAVE_EDITLINE
#endif

static char prompt_str[512] = "";
static char hostname[512] = "";

#ifdef HAVE_EDITLINE
static char *prompt(EditLine * e)
{
    return prompt_str;
}

static EditLine *el;
static History *myhistory;
static HistEvent ev;
#endif

static int running = 1;
static int thread_running = 0;

static void handle_SIGINT(int sig)
{
	if (sig);
	return;
}


#ifdef WIN32
static HANDLE hStdout;
static WORD wOldColorAttrs;
static CONSOLE_SCREEN_BUFFER_INFO csbiInfo;

static WORD 
#else
static const char*
#endif
COLORS[] = { ESL_SEQ_DEFAULT_COLOR, ESL_SEQ_FRED, ESL_SEQ_FRED, 
			ESL_SEQ_FRED, ESL_SEQ_FMAGEN, ESL_SEQ_FCYAN, ESL_SEQ_FGREEN, ESL_SEQ_FYELLOW };

static void *msg_thread_run(esl_thread_t *me, void *obj)
{

	esl_handle_t *handle = (esl_handle_t *) obj;

	thread_running = 1;

	while(thread_running && handle->connected) {
		esl_status_t status = esl_recv_timed(handle, 10);
		
		if (status == ESL_FAIL) {
			esl_log(ESL_LOG_WARNING, "Disconnected.\n");
			running = thread_running = 0;
		} else if (status == ESL_SUCCESS) {
			if (handle->last_event) {
				const char *type = esl_event_get_header(handle->last_event, "content-type");
				int known = 0;

				if (!esl_strlen_zero(type)) {
					if (!strcasecmp(type, "log/data")) {
						int level = 0, tchannel = 0;
						const char *lname = esl_event_get_header(handle->last_event, "log-level");
						const char *channel = esl_event_get_header(handle->last_event, "text-channel");
						const char *file = esl_event_get_header(handle->last_event, "log-file");
					
						if (channel) {
							tchannel = atoi(channel);
						}

						if (lname) {
							level = atoi(lname);
						}

						if (tchannel == 0 || (file && !strcmp(file, "switch_console.c"))) {
#ifdef WIN32
							DWORD len = (DWORD) strlen(handle->last_event->body);
							DWORD outbytes = 0;
							SetConsoleTextAttribute(hStdout, COLORS[level]);
							WriteFile(hStdout, handle->last_event->body, len, &outbytes, NULL);
							SetConsoleTextAttribute(hStdout, wOldColorAttrs);
#else
							printf("%s%s%s", COLORS[level], handle->last_event->body, ESL_SEQ_DEFAULT_COLOR);
#endif
						}
						known++;
					} else if (!strcasecmp(type, "text/disconnect-notice")) {
						running = thread_running = 0;
						known++;
					}
				}
				
				if (!known) {
					printf("INCOMING DATA [%s]\n%s", type, handle->last_event->body);
				}
			}
		}

		usleep(1000);
	}

	thread_running = 0;
	esl_log(ESL_LOG_DEBUG, "Thread Done\n");

	return NULL;
}

static int process_command(esl_handle_t *handle, const char *cmd) 
{
	if (
		!strcasecmp(cmd, "exit") ||
		!strcasecmp(cmd, "quit") ||
		!strcasecmp(cmd, "bye")
		) {
		return -1;
	}

	if (
		!strncasecmp(cmd, "event", 5) || 
		!strncasecmp(cmd, "noevent", 7) ||
		!strncasecmp(cmd, "nixevent", 8) ||
		!strncasecmp(cmd, "log", 3) || 
		!strncasecmp(cmd, "nolog", 5) || 
		!strncasecmp(cmd, "filter", 6)
		) {

		esl_send_recv(handle, cmd);			
		printf("%s\n", handle->last_sr_reply);

		goto end;
	}
	
	printf("Unknown command [%s]\n", cmd);

 end:

	return 0;

}

typedef struct {
	char name[128];
	char host[128];
	esl_port_t port;
	char pass[128];
} cli_profile_t;

static cli_profile_t profiles[128] = {{{0}}};
static int pcount;


static int get_profile(const char *name, cli_profile_t **profile)
{
	int x;

	for (x = 0; x < pcount; x++) {
		if (!strcmp(profiles[x].name, name)) {
			*profile = &profiles[x];
			return 0;
		}
	}

	return -1;
}

#ifndef HAVE_EDITLINE
static char command_buf[2048] = "";

static const char *basic_gets(int *cnt)
{
	int x = 0;

	printf("%s", prompt_str);

	memset(&command_buf, 0, sizeof(command_buf));
	for (x = 0; x < (sizeof(command_buf) - 1); x++) {
		int c = getchar();
		if (c < 0) {
			int y = read(fileno(stdin), command_buf, sizeof(command_buf) - 1);
			command_buf[y - 1] = '\0';
			break;
		}
		
		command_buf[x] = (char) c;
		
		if (command_buf[x] == '\n') {
			command_buf[x] = '\0';
			break;
		}
	}

	*cnt = x;

	return command_buf;

}
#endif

int main(int argc, char *argv[])
{
	esl_handle_t handle = {{0}};
	int count = 0;
	const char *line = NULL;
	char cmd_str[1024] = "";
	esl_config_t cfg;
	cli_profile_t *profile = &profiles[0];
	int cur = 0;
	int opt;
#ifndef WIN32
	char hfile[512] = "/tmp/fs_cli_history";
	char cfile[512] = "/tmp/fs_cli_config";
	char *home = getenv("HOME");
#else
	char hfile[512] = ".\\fs_cli_history";
	char cfile[512] = ".\\fs_cli_config";
	char *home = ""; //getenv("HOME");
#endif
	
	strncpy(profiles[0].host, "127.0.0.1", sizeof(profiles[0].host));
	strncpy(profiles[0].pass, "ClueCon", sizeof(profiles[0].pass));
	strncpy(profiles[0].name, "default", sizeof(profiles[0].name));
	profiles[0].port = 8021;
	pcount++;
	
	if (home) {
		snprintf(hfile, sizeof(hfile), "%s/.fs_cli_history", home);
		snprintf(cfile, sizeof(cfile), "%s/.fs_cli_config", home);
	}
	
	signal(SIGINT, handle_SIGINT);
	gethostname(hostname, sizeof(hostname));

	handle.debug = 0;
	esl_global_set_default_logger(7);
	
	if (esl_config_open_file(&cfg, cfile)) {
		char *var, *val;
		char cur_cat[128] = "";

		while (esl_config_next_pair(&cfg, &var, &val)) {
			if (strcmp(cur_cat, cfg.category)) {
				cur++;
				esl_set_string(cur_cat, cfg.category);
				esl_set_string(profiles[cur].name, cur_cat);
				esl_set_string(profiles[cur].host, "localhost");
				esl_set_string(profiles[cur].pass, "ClueCon");
				profiles[cur].port = 8021;
				esl_log(ESL_LOG_INFO, "Found Profile [%s]\n", profiles[cur].name);
				pcount++;
			}
			
			if (!strcasecmp(var, "host")) {
				esl_set_string(profiles[cur].host, val);
			} else if (!strcasecmp(var, "password")) {
				esl_set_string(profiles[cur].pass, val);
			} else if (!strcasecmp(var, "port")) {
				int pt = atoi(val);
				if (pt > 0) {
					profiles[cur].port = (esl_port_t)pt;
				}
			}
		}
		esl_config_close_file(&cfg);
	}

	while ((opt = getopt (argc, argv, "H:U:P:S:p:h?")) != -1){
		switch (opt)
		{
			case 'H':
				printf("Host: %s\n", optarg);
				esl_set_string(profile[0].host, optarg);
				break;
			case 'P':
				printf("Port: %s\n", optarg);
				profile[0].port= (esl_port_t)atoi(optarg);
				break;
			case 'p':
				printf("password: %s\n", optarg);
				esl_set_string(profile[0].pass, optarg);
				break;
			case 'h':
			case '?':
				printf("%s [-H <host>] [-P <port>] [-s <secret>] [-p <port>]\n", argv[0]);
				return 0;
			default:
				opt = 0;
		}
	}
	
	if (optind < argc) {
		if (get_profile(argv[optind], &profile)) {
			esl_log(ESL_LOG_INFO, "Chosen profile %s does not exist using builtin default\n", argv[optind]);
			profile = &profiles[0];
		} else {
			esl_log(ESL_LOG_INFO, "Chosen profile %s\n", profile->name);
		}
	}
	
	esl_log(ESL_LOG_INFO, "Using profile %s\n", profile->name);
		
	gethostname(hostname, sizeof(hostname));
	snprintf(prompt_str, sizeof(prompt_str), "freeswitch@%s> ", profile->name);

	
	if (esl_connect(&handle, profile->host, profile->port, profile->pass)) {
		esl_log(ESL_LOG_ERROR, "Error Connecting [%s]\n", handle.err);
		return -1;
	}
	
	esl_thread_create_detached(msg_thread_run, &handle);

#ifdef HAVE_EDITLINE
	el = el_init(__FILE__, stdout, stdout, stdout);
	el_set(el, EL_PROMPT, &prompt);
	el_set(el, EL_EDITOR, "emacs");
	myhistory = history_init();

	if (myhistory == 0) {
		esl_log(ESL_LOG_ERROR, "history could not be initialized\n");
		goto done;
	}

	history(myhistory, &ev, H_SETSIZE, 800);
	el_set(el, EL_HIST, history, myhistory);
	history(myhistory, &ev, H_LOAD, hfile);
#endif
#ifdef WIN32
	hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
	if (hStdout != INVALID_HANDLE_VALUE && GetConsoleScreenBufferInfo(hStdout, &csbiInfo)) {
		wOldColorAttrs = csbiInfo.wAttributes;
	}
#endif

	snprintf(cmd_str, sizeof(cmd_str), "log info\n\n");
	esl_send_recv(&handle, cmd_str);

	esl_log(ESL_LOG_INFO, "FS CLI Ready.\n");

	while (running) {

#ifdef HAVE_EDITLINE
		line = el_gets(el, &count);
#else
		line = basic_gets(&count);
#endif

		if (count > 1) {
			if (!esl_strlen_zero(line)) {
				char *cmd = strdup(line);
				char *p;

#ifdef HAVE_EDITLINE
				const LineInfo *lf = el_line(el);
				char *foo = (char *) lf->buffer;
#endif

				if ((p = strrchr(cmd, '\r')) || (p = strrchr(cmd, '\n'))) {
					*p = '\0';
				}
				assert(cmd != NULL);

#ifdef HAVE_EDITLINE
				history(myhistory, &ev, H_ENTER, line);
#endif

				if (!strncasecmp(cmd, "...", 3)) {
					goto done;
				} else if (*cmd == '/') {
					if (process_command(&handle, cmd + 1)) {
						running = 0;
					}
				} else {
					snprintf(cmd_str, sizeof(cmd_str), "api %s\n\n", cmd);
					esl_send_recv(&handle, cmd_str);
					if (handle.last_sr_event) {
						printf("%s\n", handle.last_sr_event->body);
					}
				}

#ifdef HAVE_EDITLINE
				el_deletestr(el, strlen(foo) + 1);
				memset(foo, 0, strlen(foo));
#endif
				free(cmd);
			}
		}

		usleep(1000);

	}


 done:
	
#ifdef HAVE_EDITLINE
	history(myhistory, &ev, H_SAVE, hfile);

	/* Clean up our memory */
	history_end(myhistory);
	el_end(el);
#endif

	esl_disconnect(&handle);
	
	thread_running = 0;

	return 0;
}
