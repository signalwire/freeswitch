#include <stdio.h>
#include <stdlib.h>
#include <esl.h>
#include <signal.h>
#include <sys/select.h>

#include <histedit.h>
static char prompt_str[512] = "";
static char hostname[512] = "";

char *prompt(EditLine * e)
{
	if (*prompt_str == '\0') {
        gethostname(hostname, sizeof(hostname));
        snprintf(prompt_str, sizeof(prompt_str), "freeswitch@%s> ", hostname);
    }

    return prompt_str;

}

static EditLine *el;
static History *myhistory;
static HistEvent ev;
static char *hfile = NULL;
static int running = 1;
static int thread_running = 0;
static esl_mutex_t *global_mutex;

static void handle_SIGINT(int sig)
{
	if (sig);
	return;
}

static const char* COLORS[] = { ESL_SEQ_DEFAULT_COLOR, ESL_SEQ_FRED, ESL_SEQ_FRED, 
								ESL_SEQ_FRED, ESL_SEQ_FMAGEN, ESL_SEQ_FCYAN, ESL_SEQ_FGREEN, ESL_SEQ_FYELLOW };


static void *msg_thread_run(esl_thread_t *me, void *obj)
{

	esl_handle_t *handle = (esl_handle_t *) obj;

	thread_running = 1;

	while(thread_running && handle->connected) {
		fd_set rfds, efds;
		struct timeval tv = { 0, 50 * 1000 };
		int max, activity, i = 0;
		
		esl_mutex_lock(global_mutex);
		FD_ZERO(&rfds);
		FD_ZERO(&efds);
		FD_SET(handle->sock, &rfds);
		FD_SET(handle->sock, &efds);
	
		max = handle->sock + 1;
		
		if ((activity = select(max, &rfds, NULL, &efds, &tv)) < 0) {
			esl_mutex_unlock(global_mutex);
			goto done;
		}


		if (activity && FD_ISSET(handle->sock, &rfds)) {
			esl_recv(handle);
			if (handle->last_event) {
				const char *type = esl_event_get_header(handle->last_event, "content-type");
				if (!strcasecmp(type, "log/data")) {
					int level;
					if (strstr(handle->last_event->body, "[CONSOLE]")) {
						level = 0;
					} else if (strstr(handle->last_event->body, "[ALERT]")) {
						level = 1;
					} else if (strstr(handle->last_event->body, "[CRIT]")) {
						level = 2;
					} else if (strstr(handle->last_event->body, "[ERROR]")) {
						level = 3;
					} else if (strstr(handle->last_event->body, "[WARNING]")) {
						level = 4;
					} else if (strstr(handle->last_event->body, "[NOTICE]")) {
						level = 5;
					} else if (strstr(handle->last_event->body, "[INFO]")) {
						level = 6;
					} else if (strstr(handle->last_event->body, "[DEBUG]")) {
						level = 7;
					}

					printf("%s%s%s", COLORS[level], handle->last_event->body, ESL_SEQ_DEFAULT_COLOR);
				}
			}

		}

		esl_mutex_unlock(global_mutex);
		usleep(1000);
	}

 done:

	thread_running = 0;

	return NULL;
}

static int process_command(esl_handle_t *handle, const char *cmd) 
{
	if (!strcasecmp(cmd, "exit")) {
		return -1;
	}

	if (!strncasecmp(cmd, "loglevel", 8)) {
		const char *level = cmd + 8;
		
		while(*level == ' ') level++;
		if (!esl_strlen_zero(level)) {
			char cb[128] = "";
			
			snprintf(cb, sizeof(cb), "log %s\n\n", level);
			esl_mutex_lock(global_mutex);
			esl_send_recv(handle, cb);			
			printf("%s\n", handle->last_reply);
			esl_mutex_unlock(global_mutex);
		}

		goto end;
	}


	printf("Unknown command [%s]\n", cmd);

 end:

	return 0;

}

int main(void)
{
	esl_handle_t handle = {0};
	int count;
	const char *line;
	char cmd_str[1024] = "";
	char hfile[512] = "/tmp/fs_cli_history";
	char *home = getenv("HOME");

	if (home) {
		snprintf(hfile, sizeof(hfile), "%s/.fs_cli_history", home);
	}
	
	esl_mutex_create(&global_mutex);

	signal(SIGINT, handle_SIGINT);
	gethostname(hostname, sizeof(hostname));

	handle.debug = 0;
	

	// um ya add some command line parsing for host port and pass 

	if (esl_connect(&handle, "localhost", 8021, "ClueCon")) {
		printf("Error Connecting [%s]\n", handle.err);
		goto done;
	}
	
	esl_thread_create_detached(msg_thread_run, &handle);
	
	el = el_init(__FILE__, stdout, stdout, stdout);
	el_set(el, EL_PROMPT, &prompt);
	el_set(el, EL_EDITOR, "emacs");
	myhistory = history_init();

	if (myhistory == 0) {
		fprintf(stderr, "history could not be initialized\n");
		goto done;
	}

	history(myhistory, &ev, H_SETSIZE, 800);
	el_set(el, EL_HIST, history, myhistory);
	history(myhistory, &ev, H_LOAD, hfile);
	

	snprintf(cmd_str, sizeof(cmd_str), "log info\n\n");
	esl_mutex_lock(global_mutex);
	esl_send_recv(&handle, cmd_str);
	esl_mutex_unlock(global_mutex);

	while (running) {

		line = el_gets(el, &count);
		
		if (count > 1) {
			if (!esl_strlen_zero(line)) {
				char *cmd = strdup(line);
				char *p;
				const LineInfo *lf = el_line(el);
				char *foo = (char *) lf->buffer;
				if ((p = strrchr(cmd, '\r')) || (p = strrchr(cmd, '\n'))) {
					*p = '\0';
				}
				assert(cmd != NULL);
				history(myhistory, &ev, H_ENTER, line);

				if (!strncasecmp(cmd, "...", 3)) {
					goto done;
				} else if (*cmd == '/') {
					if (process_command(&handle, cmd + 1)) {
						running = 0;
					}
				} else {
					snprintf(cmd_str, sizeof(cmd_str), "api %s\n\n", cmd);
					esl_mutex_lock(global_mutex);
					esl_send_recv(&handle, cmd_str);
					printf("%s\n", handle.last_event->body);
					esl_mutex_unlock(global_mutex);
				}

				el_deletestr(el, strlen(foo) + 1);
				memset(foo, 0, strlen(foo));
				free(cmd);
			}
		}

		usleep(1000);

	}


 done:
	
	history(myhistory, &ev, H_SAVE, hfile);

	/* Clean up our memory */
	history_end(myhistory);
	el_end(el);

	esl_disconnect(&handle);
	
	thread_running = 0;

	esl_mutex_destroy(&global_mutex);

	return 0;
}
