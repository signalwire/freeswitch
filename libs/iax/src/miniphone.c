/*
 * Miniphone: A simple, command line telephone
 *
 * IAX Support for talking to Asterisk and other Gnophone clients
 *
 * Copyright (C) 1999, Linux Support Services, Inc.
 *
 * Mark Spencer <markster@linux-support.net>
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <iax-client.h>
#include <linux/soundcard.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/signal.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <gsm.h>
#include <miniphone.h>
#include <time.h>

#include "busy.h"
#include "dialtone.h"
#include "answer.h"
#include "ringtone.h"
#include "ring10.h"
#include "options.h"

#define FRAME_SIZE 160

static char callerid[80];

struct peer {
	int time;
	gsm gsmin;
	gsm gsmout;

	struct iax_session *session;
	struct peer *next;
};

static char *audiodev = "/dev/dsp";
static int audiofd = -1;
static struct peer *peers;
static int answered_call = 0;

static struct peer *find_peer(struct iax_session *);
static int audio_setup(char *);
static void sighandler(int);
static void parse_args(FILE *, unsigned char *);
void do_iax_event(FILE *);
void call(FILE *, char *);
void answer_call(void);
void reject_call(void);
static void handle_event(FILE *, struct iax_event *e, struct peer *p);
void parse_cmd(FILE *, int, char **);
void issue_prompt(FILE *);
void dump_array(FILE *, char **);

struct sound {
	short *data;
	int datalen;
	int samplen;
	int silencelen;
	int repeat;
};

static int cursound = -1;

static int sampsent = 0;
static int offset = 0;
static int silencelen = 0;
static int nosound = 0;

static int offhook = 0;
static int ringing = 0;

static int writeonly = 0;

static struct iax_session *registry = NULL;
static struct timeval regtime;

#define TONE_NONE     -1
#define TONE_RINGTONE 0
#define TONE_BUSY     1
#define TONE_CONGEST  2
#define TONE_RINGER   3
#define TONE_ANSWER   4
#define TONE_DIALTONE  5

#define OUTPUT_NONE    0
#define OUTPUT_SPEAKER 1
#define OUTPUT_HANDSET 2
#define OUTPUT_BOTH    3

static struct sound sounds[] = {
	{ ringtone, sizeof(ringtone)/2, 16000, 32000, 1 },
	{ busy, sizeof(busy)/2, 4000, 4000, 1 },
	{ busy, sizeof(busy)/2, 2000, 2000, 1 },
	{ ring10, sizeof(ring10)/2, 16000, 32000, 1 },
	{ answer, sizeof(answer)/2, 2200, 0, 0 },
	{ dialtone, sizeof(dialtone)/2, 8000, 0, 1 },
};

static char *help[] = {
"Welcome to the miniphone telephony client, the commands are as follows:\n",
"Help\t\t-\tDisplays this screen.",
"Help <Command>\t-\tInqueries specific information on a command.",
"Dial <Number>\t-\tDials the number supplied in the first arguement",
"Status\t\t-\tLists the current sessions and their current status.",
"Quit\t\t-\tShuts down the client.",
"",
0
};

static short silence[FRAME_SIZE];

static struct peer *most_recent_answer;
static struct iax_session *newcall = 0;

static struct peer *find_peer(struct iax_session *session)
{
	struct peer *cur = peers;
	while(cur) {
		if (cur->session == session)
			return cur;
		cur = cur->next;
	}
	return NULL;
}

static int audio_setup(char *dev)
{
	int fd;	
	int fmt = AFMT_S16_LE;
	int channels = 1;
	int speed = 8000;
	int fragsize = (40 << 16) | 6;
	if ( (fd = open(dev, O_RDWR | O_NONBLOCK)) < 0) {
		fprintf(stderr, "Unable to open audio device %s: %s\n", dev, strerror(errno));
		return -1;
	}
	if (ioctl(fd, SNDCTL_DSP_SETFMT, &fmt) || (fmt != AFMT_S16_LE)) {
		fprintf(stderr, "Unable to set in signed linear format.\n");
		return -1;
	}
	if (ioctl(fd, SNDCTL_DSP_SETDUPLEX, 0)) {
		fprintf(stderr, "Unable to set full duplex operation.\n");
		writeonly = 1;
		/* return -1; */
	}
	if (ioctl(fd, SNDCTL_DSP_CHANNELS, &channels) || (channels != 1)) {
		fprintf(stderr, "Unable to set to mono\n");
		return -1;
	}
	if (ioctl(fd, SNDCTL_DSP_SPEED, &speed) || (speed != 8000)) {
		fprintf(stderr, "Unable to set speed to 8000 hz\n");
		return -1;
	}
	if (ioctl(fd, SNDCTL_DSP_SETFRAGMENT, &fragsize)) {
		fprintf(stderr, "Unable to set fragment size...\n");
		return -1;
	}

	return fd;
}

static int send_sound(int soundfd)
{
	/* Send FRAME_SIZE samples of whatever */
	short myframe[FRAME_SIZE];
	short *frame = NULL;
	int total = FRAME_SIZE;
	int amt=0;
	int res;
	int myoff;
	audio_buf_info abi;
	if (cursound > -1) {
		res = ioctl(soundfd, SNDCTL_DSP_GETOSPACE ,&abi);
		if (res) {
			fprintf(stderr,"Unable to read output space\n");
			return -1;
		}
		/* Calculate how many samples we can send, max */
		if (total > (abi.fragments * abi.fragsize / 2)) 
			total = abi.fragments * abi.fragsize / 2;
		res = total;
		if (sampsent < sounds[cursound].samplen) {
			myoff=0;
			while(total) {
				amt = total;
				if (amt > (sounds[cursound].datalen - offset)) 
					amt = sounds[cursound].datalen - offset;
				memcpy(myframe + myoff, sounds[cursound].data + offset, amt * 2);
				total -= amt;
				offset += amt;
				sampsent += amt;
				myoff += amt;
				if (offset >= sounds[cursound].datalen)
					offset = 0;
			}
			/* Set it up for silence */
			if (sampsent >= sounds[cursound].samplen) 
				silencelen = sounds[cursound].silencelen;
			frame = myframe;
		} else {
			if (silencelen > 0) {
				frame = silence;
				silencelen -= res;
			} else {
				if (sounds[cursound].repeat) {
					/* Start over */
					sampsent = 0;
					offset = 0;
				} else {
					cursound = -1;
					nosound = 0;
				}
			}
		}
#if 0
		if (frame)
			printf("res is %d, frame[0] is %d\n", res, frame[0]);
#endif		
		res = write(soundfd, frame, res * 2);
		if (res > 0)
			return 0;
		return res;
	}
	return 0;
}

static int iax_regtimeout(int timeout)
{
	if (timeout) {
		gettimeofday(&regtime, NULL);
		regtime.tv_sec += timeout;
	} else {
		regtime.tv_usec = 0;
		regtime.tv_sec = 0;
	}
	return 0;
}

static int check_iax_register(void)
{
	int res;
	if (strlen(regpeer) && strlen(server)) {
		registry = iax_session_new();
	
		res = iax_register(registry, server,regpeer,regsecret, refresh);
	
		if (res) {
			fprintf(stderr, "Failed registration: %s\n", iax_errstr);
			return -1;
		}
		iax_regtimeout(5 * refresh / 6);
	} else {
		iax_regtimeout(0);
		refresh = 60;
	}
	return 0;
}

static int check_iax_timeout(void)
{
	struct timeval tv;
	int ms;
	if (!regtime.tv_usec || !regtime.tv_sec)
		return -1;
	gettimeofday(&tv, NULL);
	if ((tv.tv_usec >= regtime.tv_usec) && (tv.tv_sec >= regtime.tv_sec)) {
		check_iax_register();
		/* Have it check again soon */
		return 100;
	}
	ms = (regtime.tv_sec - tv.tv_sec) * 1000 + (regtime.tv_usec - tv.tv_usec) / 1000;
	return ms;
}

static int gentone(int sound, int uninterruptible)
{
	cursound = sound;
	sampsent = 0;
	offset = 0;
	silencelen = 0;
	nosound = uninterruptible;
	printf("Sending tone %d\n", sound);
	return 0;
}

void
sighandler(int sig)
{
	if(sig == SIGHUP) {
		puts("rehashing!");
	} else if(sig == SIGINT) {
		static int prev = 0;
		int cur;
		
		if ( (cur = time(0))-prev <= 5) {
			printf("Terminating!\n");
			exit(0);
		} else {
			prev = cur;
			printf("Press interrupt key again in the next %d seconds to really terminate\n", 5-(cur-prev));
		}
	}
}

void
parse_args(FILE *f, unsigned char *cmd)
{
	static char *argv[MAXARGS];
	unsigned char *parse = cmd;
	int argc = 0, t = 0;

	// Don't mess with anything that doesn't exist...
	if(!*parse)
		return;

	bzero(argv, sizeof(argv));
	while(*parse) {
		if(*parse < 33 || *parse > 128) {
			*parse = 0, t++;
			if(t > MAXARG) {
				fprintf(f, "Warning: Argument exceeds maximum argument size, command ignored!\n");
				return;
			}
		} else if(t || !argc) {
			if(argc == MAXARGS) {
				fprintf(f, "Warning: Command ignored, too many arguments\n");
				return;
			}
			argv[argc++] = parse;
			t = 0;
		}

		parse++;
	}

	if(argc)
		parse_cmd(f, argc, argv);
}

int
main(int argc, char *argv[])
{
	int port;
	int netfd;
 	int c, h=0, m, regm;
	FILE *f;
	int fd = STDIN_FILENO;
	char rcmd[RBUFSIZE];
	fd_set readfd;
	fd_set writefd;
	struct timeval timer;
	struct timeval *timerptr = NULL;
	gsm_frame fo;

	load_options();

	if (!strlen(callerid))
		gethostname(callerid, sizeof(callerid));

	signal(SIGHUP, sighandler);
	signal(SIGINT, sighandler);

	if ( !(f = fdopen(fd, "w+"))) {
		fprintf(stderr, "Unable to create file on fd %d\n", fd);
		return -1;
	}

	if ( (audiofd = audio_setup(audiodev)) == -1) {
		fprintf(stderr, "Fatal error: failed to open sound device");
		return -1;
	}

	if ( (port = iax_init(0) < 0)) {
		fprintf(stderr, "Fatal error: failed to initialize iax with port %d\n", port);
		return -1;
	}

	iax_set_formats(AST_FORMAT_GSM);
	netfd = iax_get_fd();

	check_iax_register();

	fprintf(f, "Text Based Telephony Client.\n\n");
	issue_prompt(f);

	timer.tv_sec = 0;
	timer.tv_usec = 0;

	while(1) {
		FD_ZERO(&readfd);
		FD_ZERO(&writefd);
		FD_SET(fd, &readfd);
			if(fd > h)
				h = fd;
		if(answered_call && !writeonly) {
			FD_SET(audiofd, &readfd);
				if(audiofd > h)
					h = audiofd;
		}
		if (cursound > -1) {
			FD_SET(audiofd, &writefd);
			if (audiofd > h)
				h = audiofd;
		}
		FD_SET(netfd, &readfd);
		if(netfd > h)
			h = netfd;

		if ( (c = select(h+1, &readfd, &writefd, 0, timerptr)) >= 0) {
			if(FD_ISSET(fd, &readfd)) {
				if ( ( fgets(&*rcmd, 256, f))) {
					rcmd[strlen(rcmd)-1] = 0;
					parse_args(f, &*rcmd);
				} else fprintf(f, "Fatal error: failed to read data!\n");

				issue_prompt(f);
			}
			if(answered_call) {
				if(FD_ISSET(audiofd, &readfd)) {
				static int ret, rlen = 0;
					static short rbuf[FRAME_SIZE];

					if ( (ret = read(audiofd, rbuf + rlen, 2 * (FRAME_SIZE-rlen))) == -1) {
						puts("Failed to read audio.");
						return -1;
					}
					rlen += ret/2;
					if(rlen == FRAME_SIZE) {
						rlen = 0;

						if(!most_recent_answer->gsmout)
							most_recent_answer->gsmout = gsm_create();

						gsm_encode(most_recent_answer->gsmout, rbuf, fo);
						if(iax_send_voice(most_recent_answer->session,
						AST_FORMAT_GSM, (char *)fo, sizeof(fo)) == -1)
							puts("Failed to send voice!");
					}
				}
			}
			do_iax_event(f);
			m = iax_time_to_next_event();
			if(m > -1) {
				timerptr = &timer;
				timer.tv_sec = m /1000;
				timer.tv_usec = (m % 1000) * 1000;
			} else 
				timerptr = 0;
			regm = check_iax_timeout();
			if (!timerptr || (m > regm)) {
				timerptr = &timer;
				timer.tv_sec = regm /1000;
				timer.tv_usec = (regm % 1000) * 1000;
			}
			if (FD_ISSET(audiofd, &writefd)) {
				send_sound(audiofd);
			}
		} else {
			if(errno == EINTR)
				continue;
			fprintf(stderr, "Fatal error in select(): %s\n", strerror(errno));
			return -1;
		}
	}
	return 0;
}

void
do_iax_event(FILE *f) {
	int sessions = 0;
	struct iax_event *e = 0;
	struct peer *peer;

	while ( (e = iax_get_event(0))) {
		peer = find_peer(e->session);
		if(peer) {
			handle_event(f, e, peer);
		} else if (e->session == registry) {
			fprintf(stderr, "Registration complete: %s (%d)\n",
				(e->event.regreply.status == IAX_REG_SUCCESS) ? "Success" : "Failed",
				e->event.regreply.status);
			registry = NULL;
		} else {
			if(e->etype != IAX_EVENT_CONNECT) {
				fprintf(stderr, "Huh? This is an event for a non-existant session?\n");
				continue;
			}
			sessions++;

			if(sessions >= MAX_SESSIONS) {
				fprintf(f, "Missed a call... too many sessions open.\n");
			}


			if(e->event.connect.callerid && e->event.connect.dnid)
				fprintf(f, "Call from '%s' for '%s'", e->event.connect.callerid, 
				e->event.connect.dnid);
			else if(e->event.connect.dnid) {
				fprintf(f, "Call from '%s'", e->event.connect.dnid);
			} else if(e->event.connect.callerid) {
				fprintf(f, "Call from '%s'", e->event.connect.callerid);
			} else printf("Call from");
			fprintf(f, " (%s)\n", inet_ntoa(iax_get_peer_addr(e->session).sin_addr));

			if(most_recent_answer) {
				fprintf(f, "Incoming call ignored, there's already a call waiting for answer... \
please accept or reject first\n");
				iax_reject(e->session, "Too many calls, we're busy!");
			} else {
				if ( !(peer = malloc(sizeof(struct peer)))) {
					fprintf(f, "Warning: Unable to allocate memory!\n");
					return;
				}

				peer->time = time(0);
				peer->session = e->session;
				if (peer->gsmin)
					free(peer->gsmin);
				peer->gsmin = 0;
				if (peer->gsmout)
					free(peer->gsmout);
				peer->gsmout = 0;

				peer->next = peers;
				peers = peer;

				iax_accept(peer->session);
				iax_ring_announce(peer->session);
				most_recent_answer = peer;
				ringing = 1;
				gentone(TONE_RINGER, 0);
				fprintf(f, "Incoming call!\n");
			}
			issue_prompt(f);
		}
		iax_event_free(e);
	}
}

void
call(FILE *f, char *num)
{
	struct peer *peer;

	if(!newcall)
		newcall = iax_session_new();
	else {
		fprintf(f, "Already attempting to call somewhere, please cancel first!\n");
		return;
	}

	if ( !(peer = malloc(sizeof(struct peer)))) {
		fprintf(f, "Warning: Unable to allocate memory!\n");
		return;
	}

	peer->time = time(0);
	peer->session = newcall;
	peer->gsmin = 0;
	peer->gsmout = 0;

	peer->next = peers;
	peers = peer;

	most_recent_answer = peer;

	offhook = 1;
	
	iax_call(peer->session, callerid, num, NULL, 10);
}

void
answer_call(void)
{
	if(most_recent_answer)
		iax_answer(most_recent_answer->session);
	printf("Answering call!\n");
	answered_call = 1;
	offhook = 1;
	ringing = 0;
	gentone(TONE_ANSWER, 1);
}

void
reject_call(void)
{
	iax_reject(most_recent_answer->session, "Call rejected manually.");
	most_recent_answer = 0;
	ringing = 0;
	gentone(TONE_NONE, 1);
}

void
handle_event(FILE *f, struct iax_event *e, struct peer *p)
{
	short fr[FRAME_SIZE];
	int len;

	switch(e->etype) {
		case IAX_EVENT_HANGUP:
			iax_hangup(most_recent_answer->session, "Byeee!");
			fprintf(f, "Call disconnected by peer\n");
			free(most_recent_answer);
			most_recent_answer = 0;
			answered_call = 0;
			peers = 0;
			newcall = 0;
			if (offhook)
				gentone(TONE_CONGEST, 0);
			break;

		case IAX_EVENT_REJECT:
			fprintf(f, "Authentication was rejected\n");
			break;
		case IAX_EVENT_ACCEPT:
			fprintf(f, "Accepted...\n");
			issue_prompt(f);
			break;
		case IAX_EVENT_RINGA:
			fprintf(f, "Ringing...\n");
			issue_prompt(f);
			gentone(TONE_RINGTONE, 0);
			break;
		case IAX_EVENT_ANSWER:
			answer_call();
			gentone(TONE_ANSWER, 1);
 			break;
		case IAX_EVENT_VOICE:
			switch(e->event.voice.format) {
				case AST_FORMAT_GSM:
					if(e->event.voice.datalen % 33) {
						fprintf(stderr, "Weird gsm frame, not a multiple of 33.\n");
						break;
					}

					if (!p->gsmin)
						p->gsmin = gsm_create();

					len = 0;
					while(len < e->event.voice.datalen) {
						if(gsm_decode(p->gsmin, e->event.voice.data + len, fr)) {
							fprintf(stderr, "Bad GSM data\n");
							break;
						} else {
							int res;

							res = write(audiofd, fr, sizeof(fr));
							if (res < 0) 
								fprintf(f, "Write failed: %s\n", strerror(errno));
						}
						len += 33;
					}
					break;
				default :
					fprintf(f, "Don't know how to handle that format %d\n", e->event.voice.format);
			}
			break;
		default:
			fprintf(f, "Unknown event: %d\n", e->etype);
	}
}

void
dump_call(void)
{
        if(most_recent_answer)
        {
	        printf("Dumping call!\n");
            iax_hangup(most_recent_answer->session,"");
            free(most_recent_answer);
        }
        answered_call = 0;
        most_recent_answer = 0;
        answered_call = 0;
        peers = 0;
        newcall = 0;
		offhook = 0;
		ringing = 0;
		gentone(TONE_NONE, 0);
}                                                                               

void
parse_cmd(FILE *f, int argc, char **argv)
{
	if(!strcasecmp(argv[0], "HELP")) {
		if(argc == 1)
			dump_array(f, help);
		else if(argc == 2) {
			if(!strcasecmp(argv[1], "HELP"))
				fprintf(f, "Help <Command>\t-\tDisplays general help or specific help on command if supplied an arguement\n");
			else if(!strcasecmp(argv[1], "QUIT"))
				fprintf(f, "Quit\t\t-\tShuts down the miniphone\n");
			else fprintf(f, "No help available on %s\n", argv[1]);
		} else {
			fprintf(f, "Too many arguements for command help.\n");
		}
	} else if(!strcasecmp(argv[0], "STATUS")) {
		if(argc == 1) {
			int c = 0;
			struct peer *peerptr = peers;

			if(!peerptr)
				fprintf(f, "No session matches found.\n");
			else while(peerptr) {
	 			fprintf(f, "Listing sessions:\n\n");
				fprintf(f, "Session %d\n", ++c);
				fprintf(f, "Session existed for %d seconds\n", (int)time(0)-peerptr->time);
				if(answered_call)
					fprintf(f, "Call answered.\n");
				else fprintf(f, "Call ringing.\n");

				peerptr = peerptr->next;
			}
		} else fprintf(f, "Too many arguments for command status.\n");
	} else if(!strcasecmp(argv[0], "ANSWER")) {
		if(argc > 1)
			fprintf(f, "Too many arguements for command answer\n");
		else answer_call();
	} else if(!strcasecmp(argv[0], "REJECT")) {
		if(argc > 1)
			fprintf(f, "Too many arguements for command reject\n");
		else {
			fprintf(f, "Rejecting current phone call.\n");
			reject_call();
		}
	} else if (!strcasecmp(argv[0], "DUMP")) {
		dump_call();
	} else if (!strcasecmp(argv[0], "HANGUP")) {
		dump_call();
	} else if(!strcasecmp(argv[0], "CALL")) {
		if(argc > 2)
			fprintf(f, "Too many arguements for command call\n");
		else {
			call(f, argv[1]);
		}
	} else if(!strcasecmp(argv[0], "QUIT")) {
		if(argc > 1)
			fprintf(f, "Too many arguements for command quit\n");
		else {
			fprintf(f, "Good bye!\n");
			exit(1);
		}
	} else fprintf(f, "Unknown command of %s\n", argv[0]);
}

void
issue_prompt(FILE *f)
{
	fprintf(f, "TeleClient> ");
	fflush(f);
}

void
dump_array(FILE *f, char **array) {
	while(*array)
		fprintf(f, "%s\n", *array++);
}
