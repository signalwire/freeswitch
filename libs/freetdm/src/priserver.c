/*****************************************************************************
 * priserver.c Refactoring of pritest.c
 *
 * Author(s):   Anthony Minessale II <anthm@freeswitch.org>
 *              Nenad Corbic <ncorbic@sangoma.com>
 *
 * Copyright:   (c) 2005-2014 Anthony Minessale II
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 * ============================================================================
 */

#include "freetdm.h"
#include <sangoma_pri.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>

typedef struct {
	int pid;
	q931_call call;
	void *pri;
	int ready;
}call_info_t;


#define SANGOMA_MAX_CHAN_PER_SPAN 32

static call_info_t pidmap[SANGOMA_MAX_CHAN_PER_SPAN];

FIO_EVENT_CB_FUNCTION(my_ftdm_event_handler)
{
	if (event->e_type = FTDM_EVENT_DTMF) {
		char *dtmf = event->data;
		printf("DTMF %s\n", dtmf);
	}
}

/* Stupid runtime process to play a file to a b channel*/
#define BYTES 320
#define MAX_BYTES 1000

static int ready = 1;

static void handle_SIGINT(int sig)
{
	if (sig) {
		ready = 0;
	}

	return;
}

static void launch_channel(struct sangoma_pri *spri, int channo)
{
	pid_t pid;
	int fd = 0, file = 0, inlen = 0, outlen = 0;
	unsigned char inframe[MAX_BYTES], outframe[MAX_BYTES];
	fd_set readfds;
	int mtu_mru=BYTES / 2;
	int err;
	ftdm_channel_t *chan;
	ftdm_codec_t codec = FTDM_CODEC_SLIN;
	unsigned ms = 20;
	unsigned int lead = 50;
	int ifd = -1;
	ftdm_tone_type_t tt = FTDM_TONE_DTMF;
	char dtmf[] = "1234567890";
	int loops = 0;

	pid = fork();
	
	if (pid) {
		pidmap[channo-1].pid = pid;
		printf("-- Launching process %d to handle channel %d\n", pid, channo);
		return;
	}

	signal(SIGINT, handle_SIGINT);

	//ifd = open("/nfs/sounds/ptest.raw", O_WRONLY|O_CREAT|O_TRUNC, 777);
	
	memset(inframe, 0, MAX_BYTES);
	memset(outframe, 0, MAX_BYTES);
		
	if (ftdm_channel_open(spri->span, channo, &chan) != FTDM_SUCCESS) {
		printf("DEBUG cant open fd!\n");
	}
	


#if 1
	if (ftdm_channel_command(chan, FTDM_COMMAND_SET_CODEC, &codec) != FTDM_SUCCESS) {
		printf("Critical Error: Failed to set driver codec!\n");
		ftdm_channel_close(&chan);
		exit(-1);
	}
#endif
	
#if 1
	if (ftdm_channel_command(chan, FTDM_COMMAND_ENABLE_DTMF_DETECT, &tt) != FTDM_SUCCESS) {
		printf("Critical Error: Failed to set dtmf detect!\n");
		ftdm_channel_close(&chan);
		exit(-1);
	}
	ftdm_channel_set_event_callback(chan, my_ftdm_event_handler);
#endif


	if (ftdm_channel_command(chan, FTDM_COMMAND_SET_INTERVAL, &ms) != FTDM_SUCCESS) {
		printf("Critical Error: Failed to set codec interval!\n");
		ftdm_channel_close(&chan);
		exit(-1);
	}
		
	file = open("sound.raw", O_RDONLY);
	if (file < 0){
		printf("Critical Error: Failed to open sound file!\n");
		ftdm_channel_close(&chan);
		exit(-1);
	}


	while(ready) {
		ftdm_wait_flag_t flags = FTDM_READ;
		ftdm_size_t len;
		loops++;

		if (lead) {
			lead--;
		}

		if (!lead && loops == 300) {
#if 1
			if (ftdm_channel_command(chan, FTDM_COMMAND_SEND_DTMF, dtmf) != FTDM_SUCCESS) {
				printf("Critical Error: Failed to send dtmf\n");
				ftdm_channel_close(&chan);
				exit(-1);
			}
#endif

		}

		if (ftdm_channel_wait(chan, &flags, 2000) != FTDM_SUCCESS) {
			printf("wait FAIL! [%s]\n", chan->last_error);
			break;
		}
	
		if (flags & FTDM_READ) {
			len = MAX_BYTES;
			if (ftdm_channel_read(chan, inframe, &len) == FTDM_SUCCESS) {
				//printf("READ: %d\n", len);
				//write(ifd, inframe, len);
				if(!lead && (outlen = read(file, outframe, len)) <= 0) {
					break;
				}

			} else {
				printf("READ FAIL! %d [%s]\n", len, chan->last_error);
				break;
			}
			if (lead) {
				continue;
			} 
			ftdm_channel_write(chan, outframe, sizeof(outframe), &len);
		} else {
			printf("BREAK");
			break;
		}
	}

	printf("loop done\n");

	//sangoma_get_full_cfg(fd, &tdm_api);
	close(file);
	//close(ifd);

	pri_hangup(spri->pri, channo, 16);
	if (ftdm_channel_close(&chan) != FTDM_SUCCESS) {
		printf("Critical Error: Failed to close channel [%s]\n", chan->last_error);
	}

	printf("Call Handler: Process Finished\n");
	exit(0);
}


/* Event Handlers */

static int on_info(struct sangoma_pri *spri, sangoma_pri_event_t event_type, pri_event *event) 
{
	printf( "number is: %s\n", event->ring.callednum);
	if(strlen(event->ring.callednum) > 3) {
		printf( "final number is: %s\n", event->ring.callednum);
		pri_answer(spri->pri, event->ring.call, 0, 1);
	}
	return 0;
}

static int on_hangup(struct sangoma_pri *spri, sangoma_pri_event_t event_type, pri_event *event) 
{
	//pri_hangup(spri->pri, event->hangup.call, event->hangup.cause);
	printf("-- Hanging up channel %d\n", event->hangup.channel);
	if(pidmap[event->hangup.channel-1].pid) {
		pri_hangup(spri->pri, event->hangup.call, 16);
		pri_destroycall(spri->pri, event->hangup.call);
		kill(pidmap[event->hangup.channel-1].pid, SIGINT);
		pidmap[event->hangup.channel-1].pid = 0;
	}
	return 0;
}

static int on_ring(struct sangoma_pri *spri, sangoma_pri_event_t event_type, pri_event *event) 
{
	printf("-- Ring on channel %d (from %s to %s), answering...\n", event->ring.channel, event->ring.callingnum, event->ring.callednum);
	pri_answer(spri->pri, event->ring.call, event->ring.channel, 1);
	memcpy(&pidmap[event->ring.channel-1].call, event->ring.call, sizeof(q931_call));
	pidmap[event->ring.channel-1].pri=spri->pri;
	pidmap[event->ring.channel-1].call = *event->ring.call;
	launch_channel(spri, event->ring.channel);
	return 0;
}

static int on_restart(struct sangoma_pri *spri, sangoma_pri_event_t event_type, pri_event *event)
{
	printf("-- Restarting channel %d\n", event->restart.channel);
	return 0;
}

static int on_anything(struct sangoma_pri *spri, sangoma_pri_event_t event_type, pri_event *event) 
{
	printf("%s: Caught Event %d (%s)\n", __FTDM_FUNC__, event_type, sangoma_pri_event_str(event_type));
	return 0;
}

/* Generic Reaper */
static void chan_ended(int sig)
{
	int status;
	int x;
	struct rusage rusage;
	pid_t pid;
	pid = wait4(-1, &status, WNOHANG, &rusage);

	printf("-- PID %d ended\n", pid);

	for (x=0;x<SANGOMA_MAX_CHAN_PER_SPAN;x++) {
		if (pid == pidmap[x].pid) {
			pidmap[x].pid = 0;
			if (pidmap[x].pri){
				int err=pri_hangup(pidmap[x].pri, &pidmap[x].call, 16);
				//pri_destroycall(pidmap[x].pri, &pidmap[x].call);
				printf("Hanging up on PID %i Err=%i\n",pid,err);
			}

			pidmap[x].pri=NULL;
			signal(SIGCHLD, chan_ended);
			return;
		}
	}

	if (pid > -1) {
		fprintf(stderr, "--!! Unknown PID %d exited\n", pid);
		signal(SIGCHLD, chan_ended);
		return;
	}
}

/* Our Program */
int main(int argc, char *argv[])
{
	struct sangoma_pri spri;
	int debug = 0;
	if (argv[1]) {
		debug = atoi(argv[1]);
	}

	ftdm_global_set_default_logger(FTDM_LOG_LEVEL_DEBUG);
	if (ftdm_global_init() != FTDM_SUCCESS) {
        fprintf(stderr, "Error loading FreeTDM\n");
        exit(-1);
    }

    printf("FreeTDM loaded\n");


	debug = PRI_DEBUG_Q931_DUMP | PRI_DEBUG_Q931_STATE;
	printf("WTF %d\n", debug);

	if (sangoma_init_pri(&spri,
						 1,  // span
						 24, // dchan
						 SANGOMA_PRI_SWITCH_DMS100,
						 SANGOMA_PRI_CPE,
						 debug) < 0) {
		return -1;
	}
	//spri.pri->debug = (PRI_DEBUG_Q931_DUMP | PRI_DEBUG_Q921_DUMP | PRI_DEBUG_Q921_RAW | PRI_DEBUG_Q921_STATE);

	//pri_set_debug(&spri.pri, (PRI_DEBUG_Q931_DUMP | PRI_DEBUG_Q921_DUMP | PRI_DEBUG_Q921_RAW | PRI_DEBUG_Q921_STATE));

	SANGOMA_MAP_PRI_EVENT(spri, SANGOMA_PRI_EVENT_ANY, on_anything);
	SANGOMA_MAP_PRI_EVENT(spri, SANGOMA_PRI_EVENT_RING, on_ring);
	SANGOMA_MAP_PRI_EVENT(spri, SANGOMA_PRI_EVENT_HANGUP, on_hangup);
	SANGOMA_MAP_PRI_EVENT(spri, SANGOMA_PRI_EVENT_HANGUP_REQ, on_hangup);
	SANGOMA_MAP_PRI_EVENT(spri, SANGOMA_PRI_EVENT_INFO_RECEIVED, on_info);
	SANGOMA_MAP_PRI_EVENT(spri, SANGOMA_PRI_EVENT_RESTART, on_restart);

	signal(SIGCHLD, chan_ended);
	sangoma_run_pri(&spri);
	return 0;
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
